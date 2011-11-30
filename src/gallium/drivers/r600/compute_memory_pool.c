/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *      Adam Rak <adam.rak@streamnovation.com>
 */

#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "util/u_blitter.h"
#include "util/u_double_list.h"
#include "util/u_transfer.h"
#include "util/u_surface.h"
#include "util/u_pack_color.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_framebuffer.h"
#include "r600.h"
#include "r600_resource.h"
#include "r600_shader.h"
#include "r600_pipe.h"
#include "r600_formats.h"
#include "compute_memory_pool.h"
#include "evergreen_compute_internal.h"

struct compute_memory_pool* compute_memory_pool_new(int64_t initial_size_in_dw, struct r600_context *rctx) ///Creates a new pool
{
  struct compute_memory_pool* pool = (struct compute_memory_pool*)calloc(sizeof(struct compute_memory_pool), 1);

  pool->next_id = 1;
  pool->size_in_dw = initial_size_in_dw;
  pool->ctx = rctx;
  pool->bo = (struct r600_resource*)r600_compute_buffer_alloc_vram(pool->ctx->screen, pool->size_in_dw*4);
//   pool->ctx->screen->screen.user_buffer_create((struct pipe_screen*)pool->ctx->screen, NULL, pool->size_in_dw*4, PIPE_BIND_GLOBAL);
  pool->shadow = (uint32_t*)calloc(4, pool->size_in_dw);

  return pool;
}

void compute_memory_pool_delete(struct compute_memory_pool* pool) ///Frees all stuff in the pool and the pool struct itself too
{
  free(pool->shadow);
  pool->ctx->screen->screen.resource_destroy((struct pipe_screen *)pool->ctx->screen, (struct pipe_resource *)pool->bo);
  free(pool);
}

int64_t compute_memory_prealloc_chunk(struct compute_memory_pool* pool, int64_t size_in_dw) ///searches for an empty space in the pool, return with the pointer to the allocatable space in the pool, returns -1 on failure
{
  assert(size_in_dw <= pool->size_in_dw);
  
  struct compute_memory_item *item;

  int last_end = 0;
  
  for (item = pool->item_list; item; item = item->next)
  {
    if (item->start_in_dw > -1)
    {
      if (item->start_in_dw-last_end > size_in_dw)
      {
        return last_end;
      }

      last_end = item->start_in_dw + item->size_in_dw;
      last_end += (1024 - last_end % 1024);
    }
  }

  if (pool->size_in_dw - last_end < size_in_dw)
  {
    return -1;
  }
  
  return last_end;
}

struct compute_memory_item* compute_memory_postalloc_chunk(struct compute_memory_pool* pool, int64_t start_in_dw) ///search for the chunk where we can link our new chunk after it
{
  struct compute_memory_item* item;

  for (item = pool->item_list; item; item = item->next)
  {
    if (item->next)
    {
      if (item->start_in_dw < start_in_dw && item->next->start_in_dw > start_in_dw)
      {
        return item;
      }
    }
    else ///end of chain
    {
      assert(item->start_in_dw < start_in_dw);
      return item;
    }
  }

  assert(0 && "unreachable");
  return NULL;
}

void compute_memory_grow_pool(struct compute_memory_pool* pool, int new_size_in_dw) ///reallocates pool, conserves data
{
  assert(new_size_in_dw >= pool->size_in_dw);

  new_size_in_dw += 1024 - (new_size_in_dw % 1024);
  
  compute_memory_shadow(pool, 1);
  pool->shadow = (uint32_t*)realloc(pool->shadow, new_size_in_dw*4);
  pool->size_in_dw = new_size_in_dw;
  pool->ctx->screen->screen.resource_destroy((struct pipe_screen *)pool->ctx->screen, (struct pipe_resource *)pool->bo);
  pool->bo = r600_compute_buffer_alloc_vram(pool->ctx->screen, pool->size_in_dw*4);
//   (struct r600_resource*)pool->ctx->screen->screen.user_buffer_create((struct pipe_screen*)pool->ctx->screen, NULL, pool->size_in_dw*4, PIPE_BIND_GLOBAL);
  compute_memory_shadow(pool, 0);
}

void compute_memory_shadow(struct compute_memory_pool* pool, int device_to_host) ///Copy pool from device to host, or host to device
{
  /*
  struct pipe_transfer *xfer;
  uint32_t *map;
  struct pipe_context *pipe = (struct pipe_context*)pool->ctx;

  if (device_to_host)
  {
    xfer = pipe->get_transfer(pipe, (struct pipe_resource*)pool->bo, 0, PIPE_TRANSFER_READ,
                              &(struct pipe_box)
                              { .width = pool->size_in_dw, .height = 1, .depth = 1 });
    assert(xfer);
    map = pipe->transfer_map(pipe, xfer);
    assert(map);
    memcpy(pool->shadow, map, pool->size_in_dw*4);
  }
  else
  {  
    xfer = pipe->get_transfer(pipe, (struct pipe_resource*)pool->bo, 0, PIPE_TRANSFER_WRITE,
                              &(struct pipe_box)
                              { .width = pool->size_in_dw, .height = 1, .depth = 1 });
    assert(xfer);
    map = pipe->transfer_map(pipe, xfer);
    assert(map);
    memcpy(map, pool->shadow, pool->size_in_dw*4);
  }
  
  pipe->transfer_unmap(pipe, xfer);
  pipe->transfer_destroy(pipe, xfer);
*/
  
  struct compute_memory_item chunk;
  
  chunk.id = 0;
  chunk.start_in_dw = 0;
  chunk.size_in_dw = pool->size_in_dw;
  chunk.prev = chunk.next = NULL;
  compute_memory_transfer(pool, device_to_host, &chunk, pool->shadow, 0, pool->size_in_dw*4);
}

void compute_memory_finalize_pending(struct compute_memory_pool* pool) ///Allocates pending allocations in the pool
{
  struct compute_memory_item *pending_list = NULL, *end_p = NULL;
  struct compute_memory_item *item, *next;
  
  int64_t allocated = 0;
  int64_t unallocated = 0;

  for (item = pool->item_list; item; item = item->next)
  {
    printf("list: %i %p\n", item->start_in_dw, item->next);
  }
  
  for (item = pool->item_list; item; item = next)
  {
    next = item->next;
    
    
    if (item->start_in_dw == -1)
    {
      if (end_p)
      {
        end_p->next = item;
      }
      else
      {
        pending_list = item;
      }

      if (item->prev)
      {
        item->prev->next = next;
      }
      else
      {
        pool->item_list = next;
      }

      if (next)
      {
        next->prev = item->prev;
      }
      
      item->prev = end_p;
      item->next = NULL;
      end_p = item;
      
      unallocated += item->size_in_dw+1024;
    }
    else
    {
      allocated += item->size_in_dw;
    }
  }

  if (pool->size_in_dw < allocated+unallocated)
  {
    compute_memory_grow_pool(pool, allocated+unallocated);
  }
  
  for (item = pending_list; item; item = next)
  {
    next = item->next;
    
    int64_t start_in_dw;
    
    while ((start_in_dw=compute_memory_prealloc_chunk(pool, item->size_in_dw)) == -1)
    {
      int64_t need = item->size_in_dw+2048 - (pool->size_in_dw - allocated);

      need += 1024 - (need % 1024);

      if (need > 0)
      {
        compute_memory_grow_pool(pool, pool->size_in_dw + need);
      }
      else
      {
        need = pool->size_in_dw / 10;
        need += 1024 - (need % 1024);
        compute_memory_grow_pool(pool, pool->size_in_dw + need);
      }
    }

    item->start_in_dw = start_in_dw;
    item->next = NULL;
    item->prev = NULL;

    if (pool->item_list)
    {
      struct compute_memory_item *pos;
      
      pos = compute_memory_postalloc_chunk(pool, start_in_dw);
      item->prev = pos;
      item->next = pos->next;
      pos->next = item;
      
      if (item->next)
      {
        item->next->prev = item;
      }
    }
    else
    {
      pool->item_list = item;
    }
    
    allocated += item->size_in_dw;
  }
}


void compute_memory_free(struct compute_memory_pool* pool, int64_t id)
{
  struct compute_memory_item *item, *next;

  for (item = pool->item_list; item; item = next)
  {
    next = item->next;

    if (item->id == id)
    {
      if (item->prev)
      {
        item->prev->next = item->next;
      }
      else
      {
        pool->item_list = item->next;
      }

      if (item->next)
      {
        item->next->prev = item->prev;
      }

      free(item);

      return;
    }
  }

  fprintf(stderr, "Internal error, invalid id %lli for compute_memory_free\n", id);
  
  assert(0 && "error");
}

struct compute_memory_item* compute_memory_alloc(struct compute_memory_pool* pool, int64_t size_in_dw) ///Creates pending allocations
{
  struct compute_memory_item *new_item;

  printf("Alloc: %i\n", size_in_dw);
  
  new_item = (struct compute_memory_item *)calloc(sizeof(struct compute_memory_item), 1);
  new_item->size_in_dw = size_in_dw;
  new_item->start_in_dw = -1; ///mark pending
  new_item->id = pool->next_id++;
  new_item->pool = pool;
  
  struct compute_memory_item *last_item;

  if (pool->item_list)
  {
    for (last_item = pool->item_list; last_item->next; last_item = last_item->next);

    last_item->next = new_item;
    new_item->prev = last_item;
  }
  else
  {
    pool->item_list = new_item;
  }

  return new_item;
}

void compute_memory_transfer(struct compute_memory_pool* pool, int device_to_host, struct compute_memory_item* chunk, void* data, int offset_in_chunk, int size) ///Transfer data host<->device, offset and size is in bytes
{
//   int aligned_size = size;
//   aligned_size += 4 - aligned_size%4;
//
//   struct pipe_resource* gart = ...;
// pool->ctx->screen->screen.user_buffer_create((struct pipe_screen*)pool->ctx->screen, NULL, aligned_size, PIPE_BIND_GLOBAL);
//   bo->usage = PIPE_USAGE_DYNAMIC;
  int64_t aligned_size = pool->size_in_dw;
  struct pipe_resource* gart = (struct pipe_resource*)pool->bo;
  int64_t internal_offset = chunk->start_in_dw*4 + offset_in_chunk;
 
  struct pipe_transfer *xfer;
  uint32_t *map;
  struct pipe_context *pipe = (struct pipe_context*)pool->ctx;

  if (device_to_host)
  {
//   compute_memory_transfer_direct(pool, device_to_host, chunk, gart_data, offset_in_chunk, 0, size);
    xfer = pipe->get_transfer(pipe, gart, 0, PIPE_TRANSFER_READ,
                              &(struct pipe_box)
                              { .width = aligned_size, .height = 1, .depth = 1 });
    assert(xfer);
    map = pipe->transfer_map(pipe, xfer);
    assert(map);
    memcpy(data, map + internal_offset, size);
    pipe->transfer_unmap(pipe, xfer);
    pipe->transfer_destroy(pipe, xfer);
  }
  else
  {
    xfer = pipe->get_transfer(pipe, gart, 0, PIPE_TRANSFER_WRITE,
                              &(struct pipe_box)
                              { .width = aligned_size, .height = 1, .depth = 1 });
    assert(xfer);
    map = pipe->transfer_map(pipe, xfer);
    assert(map);
    memcpy(map + internal_offset, data, size);
    pipe->transfer_unmap(pipe, xfer);
    pipe->transfer_destroy(pipe, xfer);
//   compute_memory_transfer_direct(pool, device_to_host, chunk, gart_data, offset_in_chunk, 0, size);
  }

  //pool->ctx->screen->screen.resource_destroy((struct pipe_screen *)pool->ctx->screen, gart);
}

void compute_memory_transfer_direct(struct compute_memory_pool* pool, int chunk_to_data, struct compute_memory_item* chunk, struct r600_resource* data, int offset_in_chunk, int offset_in_data, int size) ///Transfer data between chunk<->data, it is for VRAM<->GART transfers
{
  ///TODO: DMA
}
