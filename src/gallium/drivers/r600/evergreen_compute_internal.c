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

#include <stdlib.h>
#include <stdio.h>

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
#include "evergreend.h"
#include "evergreen_compute_internal.h"
#include "r600_hw_context_priv.h"

int get_compute_resource_num(void)
{
  int num = 0;
#define DECL_COMPUTE_RESOURCE(name, n) num += n;
#include "compute_resource.def"
#undef DECL_COMPUTE_RESOURCE
  return num;
}

void evergreen_emit_raw_value(struct evergreen_compute_resource* res, unsigned value)
{
  res->cs[res->cs_end++] = value;
}

void evergreen_emit_ctx_value(struct r600_context *ctx, unsigned value)
{
  ctx->cs->buf[ctx->cs->cdw++] = value;
}

void evergreen_mult_reg_set_(struct evergreen_compute_resource* res,  int index, u32* array, int size)
{
  int i = 0;

  evergreen_emit_raw_reg_set(res, index, size/4);

  for (i = 0; i < size; i+=4)
  {
    res->cs[res->cs_end++] = array[i/4];
  }
}

void evergreen_reg_set(struct evergreen_compute_resource* res, unsigned index, unsigned value)
{
  evergreen_emit_raw_reg_set(res, index, 1);
  res->cs[res->cs_end++] = value;
}

struct evergreen_compute_resource* get_empty_res(struct r600_pipe_compute* pipe, enum evergreen_compute_resources res_code, int offset_index)
{
  int code_index = -1;
  int code_size = -1;
  
  {
    int i = 0;
    #define DECL_COMPUTE_RESOURCE(name, n) if (COMPUTE_RESOURCE_ ## name  == res_code) {code_index = i; code_size = n;} i += n;
    #include "compute_resource.def"
    #undef DECL_COMPUTE_RESOURCE
  }

  assert(code_index != -1 && "internal error: resouce index not found");
  assert(offset_index < code_size && "internal error: overindexing resource");

  int index = code_index + offset_index;
  
  struct evergreen_compute_resource* res = &pipe->resources[index];

  res->enabled = true;
  res->bo = NULL;
  res->cs_end = 0;
  bzero(&res->do_reloc, sizeof(res->do_reloc));
  
  return res;
}

void evergreen_emit_raw_reg_set(struct evergreen_compute_resource* res, unsigned index, int num)
{
  res->enabled = 1;
  int cs_end = res->cs_end;

  if (index >= EVERGREEN_CONFIG_REG_OFFSET && index < EVERGREEN_CONFIG_REG_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_CONFIG_REG, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_CONFIG_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CONTEXT_REG_OFFSET && index < EVERGREEN_CONTEXT_REG_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_CONTEXT_REG, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_CONTEXT_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_RESOURCE_OFFSET && index < EVERGREEN_RESOURCE_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_RESOURCE, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_RESOURCE_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_SAMPLER_OFFSET && index < EVERGREEN_SAMPLER_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_SAMPLER, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_SAMPLER_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CTL_CONST_OFFSET && index < EVERGREEN_CTL_CONST_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_CTL_CONST, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_CTL_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_LOOP_CONST_OFFSET && index < EVERGREEN_LOOP_CONST_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_LOOP_CONST, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_LOOP_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_BOOL_CONST_OFFSET && index < EVERGREEN_BOOL_CONST_END)
  {
    res->cs[cs_end] = PKT3C(PKT3_SET_BOOL_CONST, num, 0);
    res->cs[cs_end+1] = (index - EVERGREEN_BOOL_CONST_OFFSET) >> 2;
  }
  else
  {
    res->cs[cs_end] = PKT0(index, num-1);
    res->cs_end--;
  }

  res->cs_end += 2;
}

void evergreen_emit_force_reloc(struct evergreen_compute_resource* res)
{
  res->do_reloc[res->cs_end] += 1;
}

void evergreen_emit_ctx_reg_set(struct r600_context *ctx, unsigned index, int num)
{
//   printf("> %i %i\n", index, num);

  if (index >= EVERGREEN_CONFIG_REG_OFFSET && index < EVERGREEN_CONFIG_REG_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_CONFIG_REG, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_CONFIG_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CONTEXT_REG_OFFSET && index < EVERGREEN_CONTEXT_REG_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_CONTEXT_REG, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_CONTEXT_REG_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_RESOURCE_OFFSET && index < EVERGREEN_RESOURCE_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_RESOURCE, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_RESOURCE_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_SAMPLER_OFFSET && index < EVERGREEN_SAMPLER_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_SAMPLER, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_SAMPLER_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_CTL_CONST_OFFSET && index < EVERGREEN_CTL_CONST_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_CTL_CONST, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_CTL_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_LOOP_CONST_OFFSET && index < EVERGREEN_LOOP_CONST_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_LOOP_CONST, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_LOOP_CONST_OFFSET) >> 2;
  }
  else if (index >= EVERGREEN_BOOL_CONST_OFFSET && index < EVERGREEN_BOOL_CONST_END)
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT3C(PKT3_SET_BOOL_CONST, num, 0);
    ctx->cs->buf[ctx->cs->cdw++] = (index - EVERGREEN_BOOL_CONST_OFFSET) >> 2;
  }
  else
  {
    ctx->cs->buf[ctx->cs->cdw++] = PKT0(index, num-1);
  }
}

void evergreen_emit_ctx_reloc(struct r600_context *ctx, struct r600_resource *bo, enum radeon_bo_usage usage)
{
  assert(bo);

  ctx->cs->buf[ctx->cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
  u32 rr = r600_context_bo_reloc(ctx, bo, usage);
  ctx->cs->buf[ctx->cs->cdw++] = rr;
//   printf("reloc: %i %p\n", rr, bo);
}

void evergreen_set_buffer_sync(struct r600_context *ctx, struct r600_resource* bo, int size, int flags, enum radeon_bo_usage usage)
{
  assert(bo);
  int32_t cp_coher_size = 0;

  if (size == 0xffffffff || size == 0)
  {
    cp_coher_size = 0xffffffff;
  }
  else
  {
    cp_coher_size = ((size + 255) >> 8);
  }

  uint32_t sync_flags = 0;

  if ((flags & COMPUTE_RES_TC_FLUSH) == COMPUTE_RES_TC_FLUSH)
  {
    sync_flags |= S_0085F0_TC_ACTION_ENA(1);
  }

  if ((flags & COMPUTE_RES_VC_FLUSH) == COMPUTE_RES_VC_FLUSH)
  {
    sync_flags |= S_0085F0_VC_ACTION_ENA(1);
  }

  if ((flags & COMPUTE_RES_SH_FLUSH) == COMPUTE_RES_SH_FLUSH)
  {
    sync_flags |= S_0085F0_SH_ACTION_ENA(1);
  }

  if ((flags & COMPUTE_RES_CB_FLUSH(0)) == COMPUTE_RES_CB_FLUSH(0))
  {
    sync_flags |= S_0085F0_CB_ACTION_ENA(1);

    switch((flags >> 8) & 0xF)
    {
      case 0:
        sync_flags |= S_0085F0_CB0_DEST_BASE_ENA(1);
        break;
      case 1:
        sync_flags |= S_0085F0_CB1_DEST_BASE_ENA(1);
        break;
      case 2:
        sync_flags |= S_0085F0_CB2_DEST_BASE_ENA(1);
        break;
      case 3:
        sync_flags |= S_0085F0_CB3_DEST_BASE_ENA(1);
        break;
      case 4:
        sync_flags |= S_0085F0_CB4_DEST_BASE_ENA(1);
        break;
      case 5:
        sync_flags |= S_0085F0_CB5_DEST_BASE_ENA(1);
        break;
      case 6:
        sync_flags |= S_0085F0_CB6_DEST_BASE_ENA(1);
        break;
      case 7:
        sync_flags |= S_0085F0_CB7_DEST_BASE_ENA(1);
        break;
      case 8:
        sync_flags |= S_0085F0_CB8_DEST_BASE_ENA(1);
        break;
      case 9:
        sync_flags |= S_0085F0_CB9_DEST_BASE_ENA(1);
        break;
      case 10:
        sync_flags |= S_0085F0_CB10_DEST_BASE_ENA(1);
        break;
      case 11:
        sync_flags |= S_0085F0_CB11_DEST_BASE_ENA(1);
        break;
      default:
        assert(0);
    };
  }
  
  int32_t poll_interval = 10;

  ctx->cs->buf[ctx->cs->cdw++] = PKT3(PKT3_SURFACE_SYNC, 3, 0);
  ctx->cs->buf[ctx->cs->cdw++] = sync_flags;
  ctx->cs->buf[ctx->cs->cdw++] = cp_coher_size;
  ctx->cs->buf[ctx->cs->cdw++] = 0;
  ctx->cs->buf[ctx->cs->cdw++] = poll_interval;

  if (cp_coher_size != 0xffffffff)
  {
    evergreen_emit_ctx_reloc(ctx, bo, usage);
  }
}

int evergreen_compute_get_gpu_format(struct number_type_and_format* fmt, struct r600_resource *bo)
{
  switch (bo->b.b.b.format)
  {
    case PIPE_FORMAT_R32_UNORM:
      fmt->format = V_028C70_COLOR_32;
      fmt->number_type = V_028C70_NUMBER_UNORM;
      fmt->num_format_all = 0;
    break;      
    case PIPE_FORMAT_R32_FLOAT:
      fmt->format = V_028C70_COLOR_32_FLOAT;
      fmt->number_type = V_028C70_NUMBER_FLOAT;
      fmt->num_format_all = 0;
    break;
    case PIPE_FORMAT_R32G32B32A32_FLOAT:
      fmt->format = V_028C70_COLOR_32_32_32_32_FLOAT;
      fmt->number_type = V_028C70_NUMBER_FLOAT;
      fmt->num_format_all = 0;
    break;
    
    ///TODO: other formats...
    
    default:
      return 0;
  }

  return 1;
}

void evergreen_set_rat(struct r600_pipe_compute *pipe, int id, struct r600_resource* bo, int start, int size)
{
  assert(id < 12);
  assert((size & 3) == 0);
  assert((start & 0xFF) == 0);
  
  int offset;
  printf("bind rat: %i \n", id);
  
  if (id < 8)
  {
    offset = id*0x3c;
  }
  else
  {
    offset = 8*0x3c + (id-8)*0x1c;
  }

  int linear = 0;

  if (bo->b.b.b.height0 <= 1 && bo->b.b.b.depth0 <= 1 && bo->b.b.b.target == PIPE_BUFFER)
  {
    linear = 1;
  }

  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_RAT, id);

  evergreen_emit_force_reloc(res);
  
  evergreen_reg_set(res, R_028C64_CB_COLOR0_PITCH, 0); ///TODO: for 2D?
  evergreen_reg_set(res, R_028C68_CB_COLOR0_SLICE, 0);

  struct number_type_and_format fmt;

  if (bo->b.b.b.format == PIPE_FORMAT_NONE) ///default config
  {
     fmt.format = V_028C70_COLOR_32; 
     fmt.number_type = V_028C70_NUMBER_FLOAT;
  }
  else
  { 
    evergreen_compute_get_gpu_format(&fmt, bo);
  }
  
  if (linear)
  {
    evergreen_reg_set(res, R_028C70_CB_COLOR0_INFO, S_028C70_RAT(1) | S_028C70_ARRAY_MODE(V_028C70_ARRAY_LINEAR_ALIGNED) |
      S_028C70_FORMAT(fmt.format) | S_028C70_NUMBER_TYPE(fmt.number_type)
    );
    evergreen_emit_force_reloc(res);
  }
  else
  {
    assert(0 && "TODO");
    ///TODO
//   evergreen_reg_set(res, R_028C70_CB_COLOR0_INFO, S_028C70_RAT(1) | S_028C70_ARRAY_MODE(????));
//   evergreen_emit_force_reloc(res);
  }
  
  evergreen_reg_set(res, R_028C74_CB_COLOR0_ATTRIB, S_028C74_NON_DISP_TILING_ORDER(1));
  evergreen_emit_force_reloc(res);

  if (linear)
  {
    evergreen_reg_set(res, R_028C78_CB_COLOR0_DIM, size >> 2);
  }
  else
  {
    evergreen_reg_set(res, R_028C78_CB_COLOR0_DIM, S_028C78_WIDTH_MAX(bo->b.b.b.width0) | S_028C78_HEIGHT_MAX(bo->b.b.b.height0));
  }
  
  if (id <  8)
  {
    evergreen_reg_set(res, R_028C7C_CB_COLOR0_CMASK, 0);
    evergreen_emit_force_reloc(res);
    evergreen_reg_set(res, R_028C84_CB_COLOR0_FMASK, 0);
    evergreen_emit_force_reloc(res);
  }

  evergreen_reg_set(res, R_028C60_CB_COLOR0_BASE + offset, start >> 8);

  res->bo = bo;
  res->usage = RADEON_USAGE_READWRITE;
  res->coher_bo_size = size;
  res->flags = COMPUTE_RES_CB_FLUSH(id);
}

void evergreen_set_lds(struct r600_pipe_compute *pipe, int num_lds, int size, int num_waves)
{
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_LDS, 0);

  evergreen_reg_set(res, R_008E2C_SQ_LDS_RESOURCE_MGMT, S_008E2C_NUM_LS_LDS(num_lds));
  evergreen_reg_set(res, CM_R_0288E8_SQ_LDS_ALLOC, size | num_waves << 14);
}

void evergreen_set_gds(struct r600_pipe_compute *pipe, uint32_t addr, uint32_t size)
{
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_GDS, 0);
  
  evergreen_reg_set(res, R_028728_GDS_ORDERED_WAVE_PER_SE, 1);
  evergreen_reg_set(res, R_028720_GDS_ADDR_BASE, addr);
  evergreen_reg_set(res, R_028724_GDS_ADDR_SIZE, size);
}

void evergreen_set_export(struct r600_pipe_compute *pipe, struct r600_resource* bo, int offset, int size)
{
  #define SX_MEMORY_EXPORT_BASE 0x9010
  #define SX_MEMORY_EXPORT_SIZE 0x9014

  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_EXPORT, 0);

  evergreen_reg_set(res, SX_MEMORY_EXPORT_SIZE, size);

  if (size)
  {
    evergreen_reg_set(res, SX_MEMORY_EXPORT_BASE, offset);
    res->bo = bo;
    res->usage = RADEON_USAGE_WRITE;
    res->coher_bo_size = size;
    res->flags = 0;
  }
}

void evergreen_set_loop_const(struct r600_pipe_compute *pipe, int id, int count, int init, int inc)
{
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_LOOP, id);

  assert(id < 32);
  assert(count <= 0xFFF);
  assert(init <= 0xFF);
  assert(inc <= 0xFF);
  
  evergreen_reg_set(res, R_03A200_SQ_LOOP_CONST_0 + 160*4 + id*4, count | init << 12 | inc << 24);
}

void evergreen_set_tmp_ring(struct r600_pipe_compute *pipe, struct r600_resource* bo, int offset, int size, int se)
{
  #define SQ_LSTMP_RING_BASE 0x00008e10
  #define SQ_LSTMP_RING_SIZE 0x00008e14
  #define GRBM_GFX_INDEX                                  0x802C
  #define         INSTANCE_INDEX(x)                       ((x) << 0)
  #define         SE_INDEX(x)                             ((x) << 16)
  #define         INSTANCE_BROADCAST_WRITES               (1 << 30)
  #define         SE_BROADCAST_WRITES                     (1 << 31)

  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_TMPRING, se);

  evergreen_reg_set(res, GRBM_GFX_INDEX,INSTANCE_INDEX(0) | SE_INDEX(se) | INSTANCE_BROADCAST_WRITES);
  evergreen_reg_set(res, SQ_LSTMP_RING_SIZE, size);

  if (size)
  {
    assert(bo);

    evergreen_reg_set(res, SQ_LSTMP_RING_BASE, offset);
    res->bo = bo;
    res->usage = RADEON_USAGE_WRITE;
    res->coher_bo_size = 0;
    res->flags = 0;    
  }

  if (size)
  {
    evergreen_emit_force_reloc(res);
  }
  
  evergreen_reg_set(res, GRBM_GFX_INDEX,INSTANCE_INDEX(0) | SE_INDEX(0) | INSTANCE_BROADCAST_WRITES | SE_BROADCAST_WRITES);
}

void evergreen_set_kms_compute_mode(struct r600_context* ctx, bool compute_mode_flag)
{
  int bb = compute_mode_flag ? 1 : 0;

  if (ctx->ws->set_compute_mode(ctx->ws, bb))
  {
  /* XXX: Ignore this failure for now. */
#if 0
    assert(0 && "cannot set KMS compute mode");
#endif
  }
}


static uint32_t r600_colorformat_endian_swap(uint32_t colorformat)
{
  if (R600_BIG_ENDIAN) {
    switch(colorformat) {
    case V_028C70_COLOR_4_4:
      return ENDIAN_NONE;

    /* 8-bit buffers. */
    case V_028C70_COLOR_8:
      return ENDIAN_NONE;

    /* 16-bit buffers. */
    case V_028C70_COLOR_5_6_5:
    case V_028C70_COLOR_1_5_5_5:
    case V_028C70_COLOR_4_4_4_4:
    case V_028C70_COLOR_16:
    case V_028C70_COLOR_8_8:
      return ENDIAN_8IN16;

    /* 32-bit buffers. */
    case V_028C70_COLOR_8_8_8_8:
    case V_028C70_COLOR_2_10_10_10:
    case V_028C70_COLOR_8_24:
    case V_028C70_COLOR_24_8:
    case V_028C70_COLOR_32_FLOAT:
    case V_028C70_COLOR_16_16_FLOAT:
    case V_028C70_COLOR_16_16:
      return ENDIAN_8IN32;

    /* 64-bit buffers. */
    case V_028C70_COLOR_16_16_16_16:
    case V_028C70_COLOR_16_16_16_16_FLOAT:
      return ENDIAN_8IN16;

    case V_028C70_COLOR_32_32_FLOAT:
    case V_028C70_COLOR_32_32:
    case V_028C70_COLOR_X24_8_32_FLOAT:
      return ENDIAN_8IN32;

    /* 96-bit buffers. */
    case V_028C70_COLOR_32_32_32_FLOAT:
    /* 128-bit buffers. */
    case V_028C70_COLOR_32_32_32_32_FLOAT:
    case V_028C70_COLOR_32_32_32_32:
      return ENDIAN_8IN32;
    default:
      return ENDIAN_NONE; /* Unsupported. */
    }
  } else {
    return ENDIAN_NONE;
  }
}

static unsigned r600_tex_dim(unsigned dim)
{
  switch (dim) {
  default:
  case PIPE_TEXTURE_1D:
    return V_030000_SQ_TEX_DIM_1D;
  case PIPE_TEXTURE_1D_ARRAY:
    return V_030000_SQ_TEX_DIM_1D_ARRAY;
  case PIPE_TEXTURE_2D:
  case PIPE_TEXTURE_RECT:
    return V_030000_SQ_TEX_DIM_2D;
  case PIPE_TEXTURE_2D_ARRAY:
    return V_030000_SQ_TEX_DIM_2D_ARRAY;
  case PIPE_TEXTURE_3D:
    return V_030000_SQ_TEX_DIM_3D;
  case PIPE_TEXTURE_CUBE:
    return V_030000_SQ_TEX_DIM_CUBEMAP;
  }
}

void evergreen_set_vtx_resource(struct r600_pipe_compute *pipe,  struct r600_resource* bo, int id, uint64_t offset, int writable)
{
  assert(id < 16);
  uint32_t sq_vtx_constant_word2, sq_vtx_constant_word3, sq_vtx_constant_word4;
  struct number_type_and_format fmt;

  fmt.format = 0;
  
  assert(bo->b.b.b.height0 <= 1);
  assert(bo->b.b.b.depth0 <= 1);
  
  int e = evergreen_compute_get_gpu_format(&fmt, bo);

  assert(e && "unknown format");
  
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_VERT, id);

  unsigned size = bo->b.b.b.width0;
  unsigned stride = 1;

  size = (size * util_format_get_blockwidth(bo->b.b.b.format) * util_format_get_blocksize(bo->b.b.b.format));
  
  printf("id: %i vtx size: %i byte,  width0: %i elem\n", id, size, bo->b.b.b.width0);

  sq_vtx_constant_word2 =
    S_030008_BASE_ADDRESS_HI(offset >> 32) |
    S_030008_STRIDE(stride) |
    S_030008_DATA_FORMAT(fmt.format) |
    S_030008_NUM_FORMAT_ALL(fmt.num_format_all) |
    S_030008_ENDIAN_SWAP(0);

  printf("%08X %i %i %i %i\n", sq_vtx_constant_word2, offset, stride, fmt.format, fmt.num_format_all);
  
  sq_vtx_constant_word3 =
    S_03000C_DST_SEL_X(0) |
    S_03000C_DST_SEL_Y(1) |
    S_03000C_DST_SEL_Z(2) |
    S_03000C_DST_SEL_W(3);

  sq_vtx_constant_word4 = 0;

  evergreen_emit_raw_value(res, PKT3C(PKT3_SET_RESOURCE, 8, 0));
  evergreen_emit_raw_value(res, (id+816)*32 >> 2);
  evergreen_emit_raw_value(res, (unsigned)((offset) & 0xffffffff));
  evergreen_emit_raw_value(res, size - 1);
  evergreen_emit_raw_value(res, sq_vtx_constant_word2);
  evergreen_emit_raw_value(res, sq_vtx_constant_word3);
  evergreen_emit_raw_value(res, sq_vtx_constant_word4);
  evergreen_emit_raw_value(res, 0);
  evergreen_emit_raw_value(res, 0);
  evergreen_emit_raw_value(res, S_03001C_TYPE(V_03001C_SQ_TEX_VTX_VALID_BUFFER));
  
  res->bo = bo;
  
  if (writable)
  {
    res->usage = RADEON_USAGE_READWRITE;
  }
  else
  {
    res->usage = RADEON_USAGE_READ;
  }
  
  res->coher_bo_size = size;
  res->flags = COMPUTE_RES_TC_FLUSH | COMPUTE_RES_VC_FLUSH;
}

void evergreen_set_tex_resource(struct r600_pipe_compute *pipe, struct r600_pipe_sampler_view* view, int id)
{
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_TEX, id);
  struct r600_resource_texture *tmp = (struct r600_resource_texture*)view->base.texture;
  
  unsigned format, endian;
  uint32_t word4 = 0, yuv_format = 0, pitch = 0;
  unsigned char swizzle[4], array_mode = 0, tile_type = 0;
  unsigned height, depth;

//   if (view == NULL)
//     return NULL;
//   rstate = &view->state;

//   /* initialize base object */
//   view->base = *state;
//   view->base.texture = NULL;
//   pipe_reference(NULL, &texture->reference);
//   view->base.texture = texture;
//   view->base.reference.count = 1;
//   view->base.context = ctx;

  swizzle[0] = 0;
  swizzle[1] = 1;
  swizzle[2] = 2;
  swizzle[3] = 3;

  format = r600_translate_texformat((struct pipe_screen *)pipe->ctx->screen, view->base.format,
            swizzle,
            &word4, &yuv_format);
  
  if (format == ~0) {
    format = 0;
  }

  endian = r600_colorformat_endian_swap(format);

  height = view->base.texture->height0;
  depth = view->base.texture->depth0;

  pitch = align(tmp->pitch_in_blocks[0] *
          util_format_get_blockwidth(tmp->real_format), 8);
  array_mode = tmp->array_mode[0];
  tile_type = tmp->tile_type;

  assert(view->base.texture->target != PIPE_TEXTURE_1D_ARRAY);
  assert(view->base.texture->target != PIPE_TEXTURE_2D_ARRAY);

  evergreen_emit_raw_value(res, PKT3C(PKT3_SET_RESOURCE, 8, 0));
  evergreen_emit_raw_value(res, (id+816)*32 >> 2); ///TODO: check this line
  evergreen_emit_raw_value(res, (S_030000_DIM(r600_tex_dim(view->base.texture->target)) |
        S_030000_PITCH((pitch / 8) - 1) |
        S_030000_NON_DISP_TILING_ORDER(tile_type) |
        S_030000_TEX_WIDTH(view->base.texture->width0 - 1)));
  evergreen_emit_raw_value(res, (S_030004_TEX_HEIGHT(height - 1) |
        S_030004_TEX_DEPTH(depth - 1) |
        S_030004_ARRAY_MODE(array_mode)));
  evergreen_emit_raw_value(res, tmp->offset[0] >> 8);
  evergreen_emit_raw_value(res, tmp->offset[0] >> 8);
  evergreen_emit_raw_value(res, (word4 |
        S_030010_SRF_MODE_ALL(V_030010_SRF_MODE_ZERO_CLAMP_MINUS_ONE) |
        S_030010_ENDIAN_SWAP(endian) |
        S_030010_BASE_LEVEL(0)));
  evergreen_emit_raw_value(res, (S_030014_LAST_LEVEL(0) |
        S_030014_BASE_ARRAY(0) |
        S_030014_LAST_ARRAY(0)));
  evergreen_emit_raw_value(res, (S_030018_MAX_ANISO(4 /* max 16 samples */)));
  evergreen_emit_raw_value(res, S_03001C_TYPE(V_03001C_SQ_TEX_VTX_VALID_TEXTURE) | S_03001C_DATA_FORMAT(format));
  
  res->bo = (struct r600_resource*)view->base.texture;
  
  if (!view->base.writable)
  {
    res->usage = RADEON_USAGE_READ;
  }
  else
  {
    res->usage = RADEON_USAGE_READWRITE;
  }
  
  res->coher_bo_size = tmp->offset[0] + util_format_get_blockwidth(tmp->real_format)*view->base.texture->width0*height*depth;
  res->flags = COMPUTE_RES_TC_FLUSH;
  
  evergreen_emit_force_reloc(res);
  evergreen_emit_force_reloc(res);  
}

static unsigned r600_tex_wrap(unsigned wrap)
{
  switch (wrap) {
  default:
  case PIPE_TEX_WRAP_REPEAT:
    return V_03C000_SQ_TEX_WRAP;
  case PIPE_TEX_WRAP_CLAMP:
    return V_03C000_SQ_TEX_CLAMP_HALF_BORDER;
  case PIPE_TEX_WRAP_CLAMP_TO_EDGE:
    return V_03C000_SQ_TEX_CLAMP_LAST_TEXEL;
  case PIPE_TEX_WRAP_CLAMP_TO_BORDER:
    return V_03C000_SQ_TEX_CLAMP_BORDER;
  case PIPE_TEX_WRAP_MIRROR_REPEAT:
    return V_03C000_SQ_TEX_MIRROR;
  case PIPE_TEX_WRAP_MIRROR_CLAMP:
    return V_03C000_SQ_TEX_MIRROR_ONCE_HALF_BORDER;
  case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE:
    return V_03C000_SQ_TEX_MIRROR_ONCE_LAST_TEXEL;
  case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER:
    return V_03C000_SQ_TEX_MIRROR_ONCE_BORDER;
  }
}

static unsigned r600_tex_filter(unsigned filter)
{
  switch (filter) {
  default:
  case PIPE_TEX_FILTER_NEAREST:
    return V_03C000_SQ_TEX_XY_FILTER_POINT;
  case PIPE_TEX_FILTER_LINEAR:
    return V_03C000_SQ_TEX_XY_FILTER_BILINEAR;
  }
}

void evergreen_set_sampler_resource(struct r600_pipe_compute *pipe, struct compute_sampler_state *sampler, int id)
{
  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_SAMPLER, id);
  
  unsigned aniso_flag_offset = sampler->state.max_anisotropy > 1 ? 2 : 0;

  evergreen_emit_raw_value(res, PKT3C(PKT3_SET_SAMPLER, 3, 0));
  evergreen_emit_raw_value(res, (id + 90)*3);
  evergreen_emit_raw_value(res,
    S_03C000_CLAMP_X(r600_tex_wrap(sampler->state.wrap_s)) |
    S_03C000_CLAMP_Y(r600_tex_wrap(sampler->state.wrap_t)) |
    S_03C000_CLAMP_Z(r600_tex_wrap(sampler->state.wrap_r)) |
    S_03C000_XY_MAG_FILTER(r600_tex_filter(sampler->state.mag_img_filter) | aniso_flag_offset) |
    S_03C000_XY_MIN_FILTER(r600_tex_filter(sampler->state.min_img_filter) | aniso_flag_offset) |
    S_03C000_BORDER_COLOR_TYPE(V_03C000_SQ_TEX_BORDER_COLOR_OPAQUE_BLACK)
  );
  evergreen_emit_raw_value(res,
    S_03C004_MIN_LOD(S_FIXED(CLAMP(sampler->state.min_lod, 0, 15), 8)) |
    S_03C004_MAX_LOD(S_FIXED(CLAMP(sampler->state.max_lod, 0, 15), 8))
  );
  evergreen_emit_raw_value(res,
    S_03C008_LOD_BIAS(S_FIXED(CLAMP(sampler->state.lod_bias, -16, 16), 8)) |
    (sampler->state.seamless_cube_map ? 0 : S_03C008_DISABLE_CUBE_WRAP(1)) |
    S_03C008_TYPE(1)
  ); 
}

void evergreen_set_const_cache(struct r600_pipe_compute *pipe, int cache_id, struct r600_resource* cbo, int size, int offset)
{
  #define SQ_ALU_CONST_BUFFER_SIZE_LS_0 0x00028fc0
  #define SQ_ALU_CONST_CACHE_LS_0 0x00028f40

  struct evergreen_compute_resource* res = get_empty_res(pipe, COMPUTE_RESOURCE_CONST_MEM, cache_id);

  assert(size < 0x200);
  assert((offset & 0xFF) == 0);
  assert(cache_id < 16);

  evergreen_reg_set(res, SQ_ALU_CONST_BUFFER_SIZE_LS_0 + cache_id*4, size);
  evergreen_reg_set(res, SQ_ALU_CONST_CACHE_LS_0 + cache_id*4, offset >> 8);
  res->bo = cbo;
  res->usage = RADEON_USAGE_READ;
  res->coher_bo_size = size;
  res->flags = COMPUTE_RES_SH_FLUSH;
}

extern const struct u_resource_vtbl r600_buffer_vtbl;

struct r600_resource* r600_compute_buffer_alloc_vram(struct r600_screen *screen, int size_in_dw)
{
  assert(size_in_dw);

  struct r600_resource *rbuffer;
  unsigned alignment = 4096;

  rbuffer = util_slab_alloc(&screen->pool_buffers);
  rbuffer->b.b.vtbl = &r600_buffer_vtbl;
  rbuffer->b.b.b.screen = (struct pipe_screen*)screen;
  rbuffer->b.b.b.target = PIPE_BUFFER;
  rbuffer->b.b.b.format = PIPE_FORMAT_R32_UNORM;
  rbuffer->b.b.b.usage = PIPE_USAGE_IMMUTABLE;
  rbuffer->b.b.b.bind = PIPE_BIND_CUSTOM;
  rbuffer->b.b.b.width0 = size_in_dw;
  rbuffer->b.b.b.height0 = 1;
  rbuffer->b.b.b.depth0 = 1;
  rbuffer->b.b.b.array_size = 1;
  rbuffer->b.b.b.flags = 0;
  rbuffer->buf = NULL;
  pipe_reference_init(&rbuffer->b.b.b.reference, 1);
  rbuffer->b.b.b.screen = (struct pipe_screen*)screen;
  rbuffer->b.b.vtbl = &r600_buffer_vtbl;
  rbuffer->b.user_ptr = NULL;

  if (!r600_init_resource(screen, rbuffer, rbuffer->b.b.b.width0, alignment, rbuffer->b.b.b.bind, rbuffer->b.b.b.usage)) {
    FREE(rbuffer);
    return NULL;
  }
  
  return rbuffer;
}
