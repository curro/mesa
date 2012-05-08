/*
 * Copyright (C) 2012 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "pipe/p_context.h"
#include "nv50_context.h"
#include "nv50_defs.xml.h"
#include "nv50_compute.xml.h"
#include "codegen/nv50_ir_driver.h"
#include "codegen/nv50_ir_surfcfg.h"

int
nv50_compute_init(struct nv50_screen *screen)
{
   struct nouveau_device *dev = screen->base.device;
   struct nouveau_object *chan = screen->base.channel;
   struct nouveau_pushbuf *push = screen->base.pushbuf;
   struct nv04_fifo *fifo = (struct nv04_fifo *)chan->data;
   unsigned turing_class;
   int i, ret;

   if (dev->chipset >= 0xa3 &&
       dev->chipset != 0xaa &&
       dev->chipset != 0xac)
      turing_class = NVA3_COMPUTE_CLASS;
   else
      turing_class = NV50_COMPUTE_CLASS;

   ret = nouveau_object_new(chan, 0xbeef50c0, turing_class,
                            NULL, 0, &screen->compute);
   if (ret)
      return ret;

   ret = nouveau_bo_new(dev, NOUVEAU_BO_VRAM, 0, 4096, NULL,
                        &screen->surf_cfg);
   if (ret)
      return ret;

   BEGIN_NV04(push, SUBC_COMPUTE(NV01_SUBCHAN_OBJECT), 1);
   PUSH_DATA (push, screen->compute->handle);

   BEGIN_NV04(push, NV50_COMPUTE(UNK02A0), 1);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_COMPUTE(DMA_STACK), 1);
   PUSH_DATA (push, fifo->vram);
   BEGIN_NV04(push, NV50_COMPUTE(STACK_ADDRESS_HIGH), 2);
   PUSH_DATAh(push, screen->stack_bo->offset);
   PUSH_DATA (push, screen->stack_bo->offset);
   BEGIN_NV04(push, NV50_COMPUTE(STACK_SIZE_LOG), 1);
   PUSH_DATA (push, 4);

   BEGIN_NV04(push, NV50_COMPUTE(UNK0290), 1);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_COMPUTE(LANES32_ENABLE), 1);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_COMPUTE(REG_MODE), 1);
   PUSH_DATA (push, NV50_COMPUTE_REG_MODE_STRIPED);
   BEGIN_NV04(push, NV50_COMPUTE(UNK0384), 1);
   PUSH_DATA (push, 0x100);
   BEGIN_NV04(push, NV50_COMPUTE(DMA_GLOBAL), 1);
   PUSH_DATA (push, fifo->vram);

   for (i = 0; i < 15; i++) {
      BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_ADDRESS_HIGH(i)), 1);
      PUSH_DATA (push, 0);
      BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_ADDRESS_LOW(i)), 1);
      PUSH_DATA (push, 0);
      BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_LIMIT(i)), 1);
      PUSH_DATA (push, 0);
      BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_MODE(i)), 1);
      PUSH_DATA (push, NV50_COMPUTE_GLOBAL_MODE_LINEAR);
   }

   BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_ADDRESS_HIGH(15)), 1);
   PUSH_DATA (push, 0);
   BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_ADDRESS_LOW(15)), 1);
   PUSH_DATA (push, 0);
   BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_LIMIT(15)), 1);
   PUSH_DATA (push, ~0);
   BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_MODE(15)), 1);
   PUSH_DATA (push, NV50_COMPUTE_GLOBAL_MODE_LINEAR);

   BEGIN_NV04(push, NV50_COMPUTE(LOCAL_WARPS_LOG_ALLOC), 1);
   PUSH_DATA (push, 7);
   BEGIN_NV04(push, NV50_COMPUTE(LOCAL_WARPS_NO_CLAMP), 1);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_COMPUTE(STACK_WARPS_LOG_ALLOC), 1);
   PUSH_DATA (push, 7);
   BEGIN_NV04(push, NV50_COMPUTE(STACK_WARPS_NO_CLAMP), 1);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_COMPUTE(USER_PARAM_COUNT), 1);
   PUSH_DATA (push, 0);

   BEGIN_NV04(push, NV50_COMPUTE(DMA_TEXTURE), 1);
   PUSH_DATA (push, fifo->vram);
   BEGIN_NV04(push, NV50_COMPUTE(TEX_LIMITS), 1);
   PUSH_DATA (push, 0x54);
   BEGIN_NV04(push, NV50_COMPUTE(LINKED_TSC), 1);
   PUSH_DATA (push, 0);

   BEGIN_NV04(push, NV50_COMPUTE(DMA_TIC), 1);
   PUSH_DATA (push, fifo->vram);
   BEGIN_NV04(push, NV50_COMPUTE(TIC_ADDRESS_HIGH), 3);
   PUSH_DATAh(push, screen->txc->offset);
   PUSH_DATA (push, screen->txc->offset);
   PUSH_DATA (push, NV50_TIC_MAX_ENTRIES - 1);

   BEGIN_NV04(push, NV50_COMPUTE(DMA_TSC), 1);
   PUSH_DATA (push, fifo->vram);
   BEGIN_NV04(push, NV50_COMPUTE(TSC_ADDRESS_HIGH), 3);
   PUSH_DATAh(push, screen->txc->offset + 65536);
   PUSH_DATA (push, screen->txc->offset + 65536);
   PUSH_DATA (push, NV50_TSC_MAX_ENTRIES - 1);

   BEGIN_NV04(push, NV50_COMPUTE(DMA_CODE_CB), 1);
   PUSH_DATA (push, fifo->vram);

   BEGIN_NV04(push, NV50_COMPUTE(CB_DEF_ADDRESS_HIGH), 3);
   PUSH_DATAh(push, screen->surf_cfg->offset);
   PUSH_DATA (push, screen->surf_cfg->offset);
   PUSH_DATA (push, (0xf << 16) | screen->surf_cfg->size);
   BEGIN_NV04(push, NV50_COMPUTE(SET_PROGRAM_CB), 1);
   PUSH_DATA (push, (0xf << 12) | (0xf << 8) | 1);

   BEGIN_NV04(push, NV50_COMPUTE(DMA_LOCAL), 1);
   PUSH_DATA (push, fifo->vram);
   BEGIN_NV04(push, NV50_COMPUTE(LOCAL_ADDRESS_HIGH), 2);
   PUSH_DATAh(push, screen->tls_bo->offset + 65536);
   PUSH_DATA (push, screen->tls_bo->offset + 65536);
   BEGIN_NV04(push, NV50_COMPUTE(LOCAL_SIZE_LOG), 1);
   PUSH_DATA (push, util_logbase2(NV50_CAP_MAX_PROGRAM_TEMPS * 2));

   PUSH_KICK(push);
   return 0;
}

void
nv50_compute_deinit(struct nv50_screen *screen)
{
   nouveau_object_del(&screen->compute);
}

static void
nv50_set_compute_resources(struct pipe_context *context,
                          unsigned start, unsigned count,
                          struct pipe_surface **res)
{
   struct nv50_context *nv50 = nv50_context(context);
   unsigned i;

   for (i = start; i < start + count; ++i)
      pipe_surface_reference(&nv50->resources[i],
                             (res ? res[i] : NULL));

   nv50->num_resources = (res ? MAX2(nv50->num_resources, start + count) :
                          MIN2(nv50->num_resources, start));

   nouveau_bufctx_reset(nv50->bufctx_3d, NV50_BIND_RESOURCES);
   nv50->dirty |= NV50_NEW_RESOURCES;
}

#define FMT(f, l, cpp, c0, c1, c2, c3, s0, s1, s2, s3)                 \
   {                                                                   \
      PIPE_FORMAT_##f,                                                 \
      NV50_IR_SURFCFG_LAYOUT_##l,                                      \
      cpp,                                                             \
      NV50_IR_SURFCFG_CVT(c0, c1, c2, c3),                             \
      NV50_IR_SURFCFG_SCALE_##l,                                       \
      NV50_IR_SURFCFG_SWZ(s0, s1, s2, s3)                              \
   }

struct surface_format {
   enum pipe_format fmt;
   uint32_t layout;
   uint32_t cpp;
   uint32_t cvt;
   uint32_t scale;
   uint32_t swz;
} nv50_compute_surface_formats[] = {
   FMT(B8G8R8A8_UNORM, X8Y8Z8W8, 4, UNORM, UNORM, UNORM, UNORM, Z, Y, X, W),
   FMT(B8G8R8X8_UNORM, X8Y8Z8W8, 4, UNORM, UNORM, UNORM, 1F, Z, Y, X, W),
   FMT(A8R8G8B8_UNORM, X8Y8Z8W8, 4, UNORM, UNORM, UNORM, UNORM, Y, Z, W, X),
   FMT(X8R8G8B8_UNORM, X8Y8Z8W8, 4, 1F, UNORM, UNORM, UNORM, Y, Z, W, X),
   FMT(L8_UNORM, X8Y8Z8W8, 1, UNORM, 1F, 0, 0, X, X, X, Y),
   FMT(A8_UNORM, X8Y8Z8W8, 1, UNORM, 0, 0, 0, Y, Y, Y, X),
   FMT(I8_UNORM, X8Y8Z8W8, 1, UNORM, 0, 0, 0, X, X, X, X),
   FMT(L8A8_UNORM, X8Y8Z8W8, 2, UNORM, UNORM, 0, 0, X, X, X, Y),
   FMT(R32_FLOAT, X32Y32Z32W32, 4, FLOAT, 0, 0, 1F, X, Y, Z, W),
   FMT(R32G32_FLOAT, X32Y32Z32W32, 8, FLOAT, FLOAT, 0, 1F, X, Y, Z, W),
   FMT(R32G32B32A32_FLOAT, X32Y32Z32W32, 16, FLOAT, FLOAT, FLOAT, FLOAT, X, Y, Z, W),
   FMT(R32_UNORM, X32Y32Z32W32, 4, UNORM, 0, 0, 1F, X, Y, Z, W),
   FMT(R32G32_UNORM, X32Y32Z32W32, 8, UNORM, UNORM, 0, 1F, X, Y, Z, W),
   FMT(R32G32B32A32_UNORM, X32Y32Z32W32, 16, UNORM, UNORM, UNORM, UNORM, X, Y, Z, W),
   FMT(R32_SNORM, X32Y32Z32W32, 4, SNORM, 0, 0, 1F, X, Y, Z, W),
   FMT(R32G32_SNORM, X32Y32Z32W32, 8, SNORM, SNORM, 0, 1F, X, Y, Z, W),
   FMT(R32G32B32A32_SNORM, X32Y32Z32W32, 16, SNORM, SNORM, SNORM, SNORM, X, Y, Z, W),
   FMT(R8_UINT, X8Y8Z8W8, 1, UINT, 0, 0, 1I, X, Y, Z, W),
   FMT(R8G8_UINT, X8Y8Z8W8, 2, UINT, UINT, 0, 1I, X, Y, Z, W),
   FMT(R8G8B8A8_UINT, X8Y8Z8W8, 4, UINT, UINT, UINT, UINT, X, Y, Z, W),
   FMT(R8_SINT, X8Y8Z8W8, 1, SINT, 0, 0, 1I, X, Y, Z, W),
   FMT(R8G8_SINT, X8Y8Z8W8, 2, SINT, SINT, 0, 1I, X, Y, Z, W),
   FMT(R8G8B8A8_SINT, X8Y8Z8W8, 4, SINT, SINT, SINT, SINT, X, Y, Z, W),
   FMT(R32_UINT, X32Y32Z32W32, 4, UINT, 0, 0, 1I, X, Y, Z, W),
   FMT(R32G32_UINT, X32Y32Z32W32, 8, UINT, UINT, 0, 1I, X, Y, Z, W),
   FMT(R32G32B32A32_UINT, X32Y32Z32W32, 16, UINT, UINT, UINT, UINT, X, Y, Z, W),
   FMT(R32_SINT, X32Y32Z32W32, 4, SINT, 0, 0, 1I, X, Y, Z, W),
   FMT(R32G32_SINT, X32Y32Z32W32, 8, SINT, SINT, 0, 1I, X, Y, Z, W),
   FMT(R32G32B32A32_SINT, X32Y32Z32W32, 16, SINT, SINT, SINT, SINT, X, Y, Z, W),
};

static struct surface_format *
get_surface_format(enum pipe_format fmt)
{
   int i;

   for (i = 0; i < Elements(nv50_compute_surface_formats); ++i) {
      if (nv50_compute_surface_formats[i].fmt == fmt)
         return &nv50_compute_surface_formats[i];
   }

   return NULL;
}

static void
nv50_compute_upload_surfcfg(struct nv50_context *nv50, int idx,
                            struct nv04_resource *res)
{
   struct nv50_screen *screen = nv50->screen;
   struct nouveau_pushbuf *push = screen->base.pushbuf;
   struct nouveau_bo *surf_cfg = screen->surf_cfg;
   struct surface_format *fmt = get_surface_format(res->base.format);

   PUSH_SPACE(push, 28);
   BEGIN_NV04(push, NV50_2D(DST_FORMAT), 2);
   PUSH_DATA (push, NV50_SURFACE_FORMAT_R8_UNORM);
   PUSH_DATA (push, 1);
   BEGIN_NV04(push, NV50_2D(DST_PITCH), 5);
   PUSH_DATA (push, surf_cfg->size);
   PUSH_DATA (push, surf_cfg->size);
   PUSH_DATA (push, 1);
   PUSH_DATAh(push, surf_cfg->offset);
   PUSH_DATA (push, surf_cfg->offset);
   BEGIN_NV04(push, NV50_2D(SIFC_BITMAP_ENABLE), 2);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, NV50_SURFACE_FORMAT_R8_UNORM);
   BEGIN_NV04(push, NV50_2D(SIFC_WIDTH), 10);
   PUSH_DATA (push, 28);
   PUSH_DATA (push, 1);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 1);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 1);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, NV50_IR_SURFCFG__SIZE * idx);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 0);
   BEGIN_NI04(push, NV50_2D(SIFC_DATA), 7);
   PUSH_DATA (push, res->base.width0);
   PUSH_DATA (push, res->base.height0);
   PUSH_DATA (push, fmt->layout);
   PUSH_DATA (push, fmt->cpp);
   PUSH_DATA (push, fmt->cvt);
   PUSH_DATA (push, fmt->scale);
   PUSH_DATA (push, fmt->swz);
}

void
nv50_compute_validate_resources(struct nv50_context *nv50)
{
   struct nouveau_pushbuf *push = nv50->base.pushbuf;
   int i, g = 0, c = 0;

   for (i = 0; i < nv50->num_resources; ++i) {
      struct pipe_surface *s = nv50->resources[i];
      struct nv04_resource *res = nv04_resource(s->texture);

      if (!s->writable && res->base.target == PIPE_BUFFER) {
         debug_printf("binding constant buffer sz%d %d -> %d\n",
                      res->base.width0, i, c);

         assert(c < 0xf);
         BCTX_REFN(nv50->bufctx_3d, RESOURCES, res, RD);

         BEGIN_NV04(push, NV50_COMPUTE(CB_DEF_ADDRESS_HIGH), 3);
         PUSH_DATAh(push, res->address);
         PUSH_DATA (push, res->address);
         PUSH_DATA (push, (c << 16) | res->base.width0);
         BEGIN_NV04(push, NV50_COMPUTE(SET_PROGRAM_CB), 1);
         PUSH_DATA (push, (c << 12) | (c << 8) | 1);

         nv50_compute_upload_surfcfg(nv50, 0x10 | c, res);
         c++;

      } else {
         assert(g < 0xf);

         BCTX_REFN(nv50->bufctx_3d, RESOURCES, res, RDWR);

         BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_ADDRESS_HIGH(g)), 2);
         PUSH_DATAh(push, res->address);
         PUSH_DATA (push, res->address);

         if (res->base.target == PIPE_BUFFER) {
            debug_printf("binding global buffer %p@%lx sz%d %d -> %d\n",
                         res, res->address, res->base.width0, i, g);

            BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_LIMIT(g)), 1);
            PUSH_DATA (push, align(res->base.width0, 0x100) - 1);
            BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_MODE(g)), 1);
            PUSH_DATA (push, NV50_COMPUTE_GLOBAL_MODE_LINEAR);

         } else if (res->base.target == PIPE_TEXTURE_2D) {
            struct nv50_miptree *mt = nv50_miptree(s->texture);

            debug_printf("binding global texture w%d p%d h%d t%d %d -> %d\n",
                         res->base.width0, mt->level[0].pitch,
                         res->base.height0, res->bo->config.nv50.tile_mode,
                         i, g);

            BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_PITCH(g)), 1);
            PUSH_DATA (push, res->base.width0 << 8);
            BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_LIMIT(g)), 1);
            PUSH_DATA (push, (res->base.height0 - 1) << 16 |
                       (mt->level[0].pitch - 1));
            BEGIN_NV04(push, NV50_COMPUTE(GLOBAL_MODE(g)), 1);
            PUSH_DATA (push, (res->bo->config.nv50.tile_mode & 0xf0) << 4);
         }

         nv50_compute_upload_surfcfg(nv50, g, res);
         g++;
      }
   }

   BEGIN_NV04(push, NV50_COMPUTE(CODE_CB_FLUSH), 1);
   PUSH_DATA (push, 0);
}

static void
nv50_compute_bind_global(struct pipe_context *context,
                         unsigned start, unsigned count,
                         struct pipe_resource **resources,
                         uint32_t **handles)
{
   struct nv50_context *nv50 = nv50_context(context);
   int i;

   nouveau_bufctx_reset(nv50->bufctx_3d, NV50_BIND_GLOBALS);

   for (i = start; i < start + count; ++i) {
      if (resources) {
         BCTX_REFN(nv50->bufctx_3d, GLOBALS, nv04_resource(resources[i]), RDWR);
         pipe_resource_reference(&nv50->globals[i].r, resources[i]);
         nv50->globals[i].handle = handles[i];

      } else {
         pipe_resource_reference(&nv50->globals[i].r, NULL);
      }
   }
}

static void
nv50_compute_upload_input(struct nv50_context *nv50,
                          const uint32_t *p)
{
   struct nv50_screen *screen = nv50->screen;
   struct nouveau_pushbuf *push = screen->base.pushbuf;
   unsigned size = align(nv50->compprog->cp.input_size, 4);

   BEGIN_NV04(push, NV50_COMPUTE(USER_PARAM_COUNT), 1);
   PUSH_DATA (push, (size / 4) << 8);

   if (size) {
      struct nouveau_mm_allocation *mm;
      struct nouveau_bo *bo = NULL;
      unsigned offset;
      int i;

      mm = nouveau_mm_allocate(screen->base.mm_GART, size, &bo, &offset);
      assert(mm);

      nouveau_bo_map(bo, 0, screen->base.client);
      memcpy(bo->map + offset, p, size);

      for (i = 0; i < Elements(nv50->globals); ++i) {
         if (nv50->globals[i].r) {
            struct nv04_resource *r = nv04_resource(nv50->globals[i].r);
            size_t reloc_off = (char *)nv50->globals[i].handle - (char *)p;
            uint32_t *reloc_loc = (uint32_t *)
               ((uint8_t *)bo->map + offset + reloc_off);

            if (reloc_off >= 0 && reloc_off < size)
               *reloc_loc = r->address;
         }
      }

      nouveau_bufctx_refn(nv50->bufctx, 0, bo, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
      nouveau_pushbuf_bufctx(push, nv50->bufctx);
      nouveau_pushbuf_validate(push);

      BEGIN_NV04(push, NV50_COMPUTE(USER_PARAM(0)), size / 4);
      nouveau_pushbuf_data(push, bo, offset, size);

      nouveau_fence_work(screen->base.fence.current, nouveau_mm_free_work, mm);
      nouveau_bo_ref(NULL, &bo);
      nouveau_bufctx_reset(nv50->bufctx, 0);
   }
}

static uint32_t
nv50_compute_find_symbol(struct nv50_context *nv50, uint32_t label)
{
   struct nv50_program *prog = nv50->compprog;
   int i;

   for (i = 0; i < prog->sym_count; ++i) {
      if (prog->syms[i].label == label)
         return prog->syms[i].offset;
   }

   assert(0);
   return 0;
}

static void
nv50_compute_launch(struct pipe_context *context,
                    const uint *block_layout, const uint *grid_layout,
                    uint32_t pc, const void *input)
{
   struct nv50_context *nv50 = nv50_context(context);
   struct nouveau_pushbuf *push = nv50->base.pushbuf;
   unsigned block_size = block_layout[0] * block_layout[1] * block_layout[2];

   nv50_state_validate(nv50, NV50_NEW_COMPPROG |
                       NV50_NEW_TEXTURES |
                       NV50_NEW_SAMPLERS |
                       NV50_NEW_RESOURCES, 0);

   BEGIN_NV04(push, NV50_COMPUTE(CP_START_ID), 1);
   PUSH_DATA (push, nv50_compute_find_symbol(nv50, pc));

   BEGIN_NV04(push, NV50_COMPUTE(BLOCKDIM_XY), 2);
   PUSH_DATA (push, block_layout[1] << 16 | block_layout[0]);
   PUSH_DATA (push, block_layout[2]);
   BEGIN_NV04(push, NV50_COMPUTE(BLOCK_ALLOC), 1);
   PUSH_DATA (push, 1 << 16 | block_size);
   BEGIN_NV04(push, NV50_COMPUTE(BLOCKDIM_LATCH), 1);
   PUSH_DATA (push, 1);

   BEGIN_NV04(push, NV50_COMPUTE(GRIDDIM), 1);
   PUSH_DATA (push, grid_layout[1] << 16 | grid_layout[0]);
   BEGIN_NV04(push, NV50_COMPUTE(GRIDID), 1);
   PUSH_DATA (push, 1);

   nv50_compute_upload_input(nv50, input);

   BEGIN_NV04(push, NV50_COMPUTE(LAUNCH), 1);
   PUSH_DATA (push, 0);
   BEGIN_NV04(push, SUBC_COMPUTE(NV50_GRAPH_SERIALIZE), 1);
   PUSH_DATA (push, 0);
}

void
nv50_init_compute_functions(struct nv50_context *nv50)
{
   struct pipe_context *pipe = &nv50->base.pipe;

   pipe->set_compute_resources = nv50_set_compute_resources;
   pipe->set_global_binding = nv50_compute_bind_global;
   pipe->launch_grid = nv50_compute_launch;
}
