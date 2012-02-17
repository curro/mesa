/*
 * Copyright 2010 Jerome Glisse <glisse@freedesktop.org>
 *
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
 */

/* TODO:
 *	- fix mask for depth control & cull for query
 */
#include <stdio.h>
#include <errno.h>
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_util.h"
#include "util/u_blitter.h"
#include "util/u_double_list.h"
#include "util/u_transfer.h"
#include "util/u_surface.h"
#include "util/u_pack_color.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_framebuffer.h"
#include "pipebuffer/pb_buffer.h"
#include "r600.h"
#include "evergreend.h"
#include "r600_resource.h"
#include "r600_shader.h"
#include "r600_pipe.h"
#include "r600_formats.h"
#include "evergreen_compute.h"

static uint32_t r600_translate_blend_function(int blend_func)
{
	switch (blend_func) {
	case PIPE_BLEND_ADD:
		return V_028780_COMB_DST_PLUS_SRC;
	case PIPE_BLEND_SUBTRACT:
		return V_028780_COMB_SRC_MINUS_DST;
	case PIPE_BLEND_REVERSE_SUBTRACT:
		return V_028780_COMB_DST_MINUS_SRC;
	case PIPE_BLEND_MIN:
		return V_028780_COMB_MIN_DST_SRC;
	case PIPE_BLEND_MAX:
		return V_028780_COMB_MAX_DST_SRC;
	default:
		R600_ERR("Unknown blend function %d\n", blend_func);
		assert(0);
		break;
	}
	return 0;
}

static uint32_t r600_translate_blend_factor(int blend_fact)
{
	switch (blend_fact) {
	case PIPE_BLENDFACTOR_ONE:
		return V_028780_BLEND_ONE;
	case PIPE_BLENDFACTOR_SRC_COLOR:
		return V_028780_BLEND_SRC_COLOR;
	case PIPE_BLENDFACTOR_SRC_ALPHA:
		return V_028780_BLEND_SRC_ALPHA;
	case PIPE_BLENDFACTOR_DST_ALPHA:
		return V_028780_BLEND_DST_ALPHA;
	case PIPE_BLENDFACTOR_DST_COLOR:
		return V_028780_BLEND_DST_COLOR;
	case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE:
		return V_028780_BLEND_SRC_ALPHA_SATURATE;
	case PIPE_BLENDFACTOR_CONST_COLOR:
		return V_028780_BLEND_CONST_COLOR;
	case PIPE_BLENDFACTOR_CONST_ALPHA:
		return V_028780_BLEND_CONST_ALPHA;
	case PIPE_BLENDFACTOR_ZERO:
		return V_028780_BLEND_ZERO;
	case PIPE_BLENDFACTOR_INV_SRC_COLOR:
		return V_028780_BLEND_ONE_MINUS_SRC_COLOR;
	case PIPE_BLENDFACTOR_INV_SRC_ALPHA:
		return V_028780_BLEND_ONE_MINUS_SRC_ALPHA;
	case PIPE_BLENDFACTOR_INV_DST_ALPHA:
		return V_028780_BLEND_ONE_MINUS_DST_ALPHA;
	case PIPE_BLENDFACTOR_INV_DST_COLOR:
		return V_028780_BLEND_ONE_MINUS_DST_COLOR;
	case PIPE_BLENDFACTOR_INV_CONST_COLOR:
		return V_028780_BLEND_ONE_MINUS_CONST_COLOR;
	case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
		return V_028780_BLEND_ONE_MINUS_CONST_ALPHA;
	case PIPE_BLENDFACTOR_SRC1_COLOR:
		return V_028780_BLEND_SRC1_COLOR;
	case PIPE_BLENDFACTOR_SRC1_ALPHA:
		return V_028780_BLEND_SRC1_ALPHA;
	case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
		return V_028780_BLEND_INV_SRC1_COLOR;
	case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
		return V_028780_BLEND_INV_SRC1_ALPHA;
	default:
		R600_ERR("Bad blend factor %d not supported!\n", blend_fact);
		assert(0);
		break;
	}
	return 0;
}

static uint32_t r600_translate_stencil_op(int s_op)
{
	switch (s_op) {
	case PIPE_STENCIL_OP_KEEP:
		return V_028800_STENCIL_KEEP;
	case PIPE_STENCIL_OP_ZERO:
		return V_028800_STENCIL_ZERO;
	case PIPE_STENCIL_OP_REPLACE:
		return V_028800_STENCIL_REPLACE;
	case PIPE_STENCIL_OP_INCR:
		return V_028800_STENCIL_INCR;
	case PIPE_STENCIL_OP_DECR:
		return V_028800_STENCIL_DECR;
	case PIPE_STENCIL_OP_INCR_WRAP:
		return V_028800_STENCIL_INCR_WRAP;
	case PIPE_STENCIL_OP_DECR_WRAP:
		return V_028800_STENCIL_DECR_WRAP;
	case PIPE_STENCIL_OP_INVERT:
		return V_028800_STENCIL_INVERT;
	default:
		R600_ERR("Unknown stencil op %d", s_op);
		assert(0);
		break;
	}
	return 0;
}

static uint32_t r600_translate_fill(uint32_t func)
{
	switch(func) {
	case PIPE_POLYGON_MODE_FILL:
		return 2;
	case PIPE_POLYGON_MODE_LINE:
		return 1;
	case PIPE_POLYGON_MODE_POINT:
		return 0;
	default:
		assert(0);
		return 0;
	}
}

/* translates straight */
static uint32_t r600_translate_ds_func(int func)
{
	return func;
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

static unsigned r600_tex_mipfilter(unsigned filter)
{
	switch (filter) {
	case PIPE_TEX_MIPFILTER_NEAREST:
		return V_03C000_SQ_TEX_Z_FILTER_POINT;
	case PIPE_TEX_MIPFILTER_LINEAR:
		return V_03C000_SQ_TEX_Z_FILTER_LINEAR;
	default:
	case PIPE_TEX_MIPFILTER_NONE:
		return V_03C000_SQ_TEX_Z_FILTER_NONE;
	}
}

static unsigned r600_tex_compare(unsigned compare)
{
	switch (compare) {
	default:
	case PIPE_FUNC_NEVER:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_NEVER;
	case PIPE_FUNC_LESS:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_LESS;
	case PIPE_FUNC_EQUAL:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_EQUAL;
	case PIPE_FUNC_LEQUAL:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_LESSEQUAL;
	case PIPE_FUNC_GREATER:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_GREATER;
	case PIPE_FUNC_NOTEQUAL:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_NOTEQUAL;
	case PIPE_FUNC_GEQUAL:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_GREATEREQUAL;
	case PIPE_FUNC_ALWAYS:
		return V_03C000_SQ_TEX_DEPTH_COMPARE_ALWAYS;
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

static uint32_t r600_translate_dbformat(enum pipe_format format)
{
	switch (format) {
	case PIPE_FORMAT_Z16_UNORM:
		return V_028040_Z_16;
	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
		return V_028040_Z_24;
	case PIPE_FORMAT_Z32_FLOAT:
	case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
		return V_028040_Z_32_FLOAT;
	default:
		return ~0U;
	}
}

static uint32_t r600_translate_colorswap(enum pipe_format format)
{
	switch (format) {
	/* 8-bit buffers. */
	case PIPE_FORMAT_L4A4_UNORM:
	case PIPE_FORMAT_A4R4_UNORM:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_A8_UNORM:
	case PIPE_FORMAT_A8_UINT:
	case PIPE_FORMAT_A8_SINT:
	case PIPE_FORMAT_R4A4_UNORM:
		return V_028C70_SWAP_ALT_REV;
	case PIPE_FORMAT_I8_UNORM:
	case PIPE_FORMAT_L8_UNORM:
	case PIPE_FORMAT_I8_UINT:
	case PIPE_FORMAT_I8_SINT:
	case PIPE_FORMAT_L8_UINT:
	case PIPE_FORMAT_L8_SINT:
	case PIPE_FORMAT_L8_SRGB:
	case PIPE_FORMAT_R8_UNORM:
	case PIPE_FORMAT_R8_SNORM:
	case PIPE_FORMAT_R8_UINT:
	case PIPE_FORMAT_R8_SINT:
		return V_028C70_SWAP_STD;

	/* 16-bit buffers. */
	case PIPE_FORMAT_B5G6R5_UNORM:
		return V_028C70_SWAP_STD_REV;

	case PIPE_FORMAT_B5G5R5A1_UNORM:
	case PIPE_FORMAT_B5G5R5X1_UNORM:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_B4G4R4A4_UNORM:
	case PIPE_FORMAT_B4G4R4X4_UNORM:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_Z16_UNORM:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_L8A8_UNORM:
	case PIPE_FORMAT_L8A8_UINT:
	case PIPE_FORMAT_L8A8_SINT:
	case PIPE_FORMAT_L8A8_SRGB:
		return V_028C70_SWAP_ALT;
	case PIPE_FORMAT_R8G8_UNORM:
	case PIPE_FORMAT_R8G8_UINT:
	case PIPE_FORMAT_R8G8_SINT:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_R16_UNORM:
	case PIPE_FORMAT_R16_UINT:
	case PIPE_FORMAT_R16_SINT:
	case PIPE_FORMAT_R16_FLOAT:
		return V_028C70_SWAP_STD;

	/* 32-bit buffers. */
	case PIPE_FORMAT_A8B8G8R8_SRGB:
		return V_028C70_SWAP_STD_REV;
	case PIPE_FORMAT_B8G8R8A8_SRGB:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_B8G8R8A8_UNORM:
	case PIPE_FORMAT_B8G8R8X8_UNORM:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_A8R8G8B8_UNORM:
	case PIPE_FORMAT_X8R8G8B8_UNORM:
		return V_028C70_SWAP_ALT_REV;
	case PIPE_FORMAT_R8G8B8A8_SNORM:
	case PIPE_FORMAT_R8G8B8A8_UNORM:
	case PIPE_FORMAT_R8G8B8A8_SSCALED:
	case PIPE_FORMAT_R8G8B8A8_USCALED:
	case PIPE_FORMAT_R8G8B8A8_SINT:
	case PIPE_FORMAT_R8G8B8A8_UINT:
	case PIPE_FORMAT_R8G8B8X8_UNORM:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_A8B8G8R8_UNORM:
	case PIPE_FORMAT_X8B8G8R8_UNORM:
	/* case PIPE_FORMAT_R8SG8SB8UX8U_NORM: */
		return V_028C70_SWAP_STD_REV;

	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_X8Z24_UNORM:
	case PIPE_FORMAT_S8_UINT_Z24_UNORM:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_R10G10B10A2_UNORM:
	case PIPE_FORMAT_R10G10B10X2_SNORM:
	case PIPE_FORMAT_R10SG10SB10SA2U_NORM:
		return V_028C70_SWAP_STD;

	case PIPE_FORMAT_B10G10R10A2_UNORM:
	case PIPE_FORMAT_B10G10R10A2_UINT:
		return V_028C70_SWAP_ALT;

	case PIPE_FORMAT_R11G11B10_FLOAT:
	case PIPE_FORMAT_R32_FLOAT:
	case PIPE_FORMAT_R32_UINT:
	case PIPE_FORMAT_R32_SINT:
	case PIPE_FORMAT_Z32_FLOAT:
	case PIPE_FORMAT_R16G16_FLOAT:
	case PIPE_FORMAT_R16G16_UNORM:
	case PIPE_FORMAT_R16G16_UINT:
	case PIPE_FORMAT_R16G16_SINT:
		return V_028C70_SWAP_STD;

	/* 64-bit buffers. */
	case PIPE_FORMAT_R32G32_FLOAT:
	case PIPE_FORMAT_R32G32_UINT:
	case PIPE_FORMAT_R32G32_SINT:
	case PIPE_FORMAT_R16G16B16A16_UNORM:
	case PIPE_FORMAT_R16G16B16A16_SNORM:
	case PIPE_FORMAT_R16G16B16A16_USCALED:
	case PIPE_FORMAT_R16G16B16A16_SSCALED:
	case PIPE_FORMAT_R16G16B16A16_UINT:
	case PIPE_FORMAT_R16G16B16A16_SINT:
	case PIPE_FORMAT_R16G16B16A16_FLOAT:
	case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:

	/* 128-bit buffers. */
	case PIPE_FORMAT_R32G32B32A32_FLOAT:
	case PIPE_FORMAT_R32G32B32A32_SNORM:
	case PIPE_FORMAT_R32G32B32A32_UNORM:
	case PIPE_FORMAT_R32G32B32A32_SSCALED:
	case PIPE_FORMAT_R32G32B32A32_USCALED:
	case PIPE_FORMAT_R32G32B32A32_SINT:
	case PIPE_FORMAT_R32G32B32A32_UINT:
		return V_028C70_SWAP_STD;
	default:
		R600_ERR("unsupported colorswap format %d\n", format);
		return ~0U;
	}
	return ~0U;
}

static uint32_t r600_translate_colorformat(enum pipe_format format)
{
	switch (format) {
	/* 8-bit buffers. */
	case PIPE_FORMAT_A8_UNORM:
	case PIPE_FORMAT_A8_UINT:
	case PIPE_FORMAT_A8_SINT:
	case PIPE_FORMAT_I8_UNORM:
	case PIPE_FORMAT_I8_UINT:
	case PIPE_FORMAT_I8_SINT:
	case PIPE_FORMAT_L8_UNORM:
	case PIPE_FORMAT_L8_UINT:
	case PIPE_FORMAT_L8_SINT:
	case PIPE_FORMAT_L8_SRGB:
	case PIPE_FORMAT_R8_UNORM:
	case PIPE_FORMAT_R8_SNORM:
	case PIPE_FORMAT_R8_UINT:
	case PIPE_FORMAT_R8_SINT:
		return V_028C70_COLOR_8;

	/* 16-bit buffers. */
	case PIPE_FORMAT_B5G6R5_UNORM:
		return V_028C70_COLOR_5_6_5;

	case PIPE_FORMAT_B5G5R5A1_UNORM:
	case PIPE_FORMAT_B5G5R5X1_UNORM:
		return V_028C70_COLOR_1_5_5_5;

	case PIPE_FORMAT_B4G4R4A4_UNORM:
	case PIPE_FORMAT_B4G4R4X4_UNORM:
		return V_028C70_COLOR_4_4_4_4;

	case PIPE_FORMAT_Z16_UNORM:
		return V_028C70_COLOR_16;

	case PIPE_FORMAT_L8A8_UNORM:
	case PIPE_FORMAT_L8A8_UINT:
	case PIPE_FORMAT_L8A8_SINT:
	case PIPE_FORMAT_L8A8_SRGB:
	case PIPE_FORMAT_R8G8_UNORM:
	case PIPE_FORMAT_R8G8_UINT:
	case PIPE_FORMAT_R8G8_SINT:
		return V_028C70_COLOR_8_8;

	case PIPE_FORMAT_R16_UNORM:
	case PIPE_FORMAT_R16_UINT:
	case PIPE_FORMAT_R16_SINT:
		return V_028C70_COLOR_16;

	case PIPE_FORMAT_R16_FLOAT:
		return V_028C70_COLOR_16_FLOAT;

	/* 32-bit buffers. */
	case PIPE_FORMAT_A8B8G8R8_SRGB:
	case PIPE_FORMAT_A8B8G8R8_UNORM:
	case PIPE_FORMAT_A8R8G8B8_UNORM:
	case PIPE_FORMAT_B8G8R8A8_SRGB:
	case PIPE_FORMAT_B8G8R8A8_UNORM:
	case PIPE_FORMAT_B8G8R8X8_UNORM:
	case PIPE_FORMAT_R8G8B8A8_SNORM:
	case PIPE_FORMAT_R8G8B8A8_UNORM:
	case PIPE_FORMAT_R8G8B8X8_UNORM:
	case PIPE_FORMAT_R8SG8SB8UX8U_NORM:
	case PIPE_FORMAT_X8B8G8R8_UNORM:
	case PIPE_FORMAT_X8R8G8B8_UNORM:
	case PIPE_FORMAT_R8G8B8_UNORM:
	case PIPE_FORMAT_R8G8B8A8_SSCALED:
	case PIPE_FORMAT_R8G8B8A8_USCALED:
	case PIPE_FORMAT_R8G8B8A8_SINT:
	case PIPE_FORMAT_R8G8B8A8_UINT:
		return V_028C70_COLOR_8_8_8_8;

	case PIPE_FORMAT_R10G10B10A2_UNORM:
	case PIPE_FORMAT_R10G10B10X2_SNORM:
	case PIPE_FORMAT_B10G10R10A2_UNORM:
	case PIPE_FORMAT_B10G10R10A2_UINT:
	case PIPE_FORMAT_R10SG10SB10SA2U_NORM:
		return V_028C70_COLOR_2_10_10_10;

	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
		return V_028C70_COLOR_8_24;

	case PIPE_FORMAT_X8Z24_UNORM:
	case PIPE_FORMAT_S8_UINT_Z24_UNORM:
		return V_028C70_COLOR_24_8;

	case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
		return V_028C70_COLOR_X24_8_32_FLOAT;

	case PIPE_FORMAT_R32_UINT:
	case PIPE_FORMAT_R32_SINT:
		return V_028C70_COLOR_32;

	case PIPE_FORMAT_R32_FLOAT:
	case PIPE_FORMAT_Z32_FLOAT:
		return V_028C70_COLOR_32_FLOAT;

	case PIPE_FORMAT_R16G16_FLOAT:
		return V_028C70_COLOR_16_16_FLOAT;

	case PIPE_FORMAT_R16G16_SSCALED:
	case PIPE_FORMAT_R16G16_UNORM:
	case PIPE_FORMAT_R16G16_UINT:
	case PIPE_FORMAT_R16G16_SINT:
		return V_028C70_COLOR_16_16;

	case PIPE_FORMAT_R11G11B10_FLOAT:
		return V_028C70_COLOR_10_11_11_FLOAT;

	/* 64-bit buffers. */
	case PIPE_FORMAT_R16G16B16_USCALED:
	case PIPE_FORMAT_R16G16B16_SSCALED:
	case PIPE_FORMAT_R16G16B16A16_UINT:
	case PIPE_FORMAT_R16G16B16A16_SINT:
	case PIPE_FORMAT_R16G16B16A16_USCALED:
	case PIPE_FORMAT_R16G16B16A16_SSCALED:
	case PIPE_FORMAT_R16G16B16A16_UNORM:
	case PIPE_FORMAT_R16G16B16A16_SNORM:
		return V_028C70_COLOR_16_16_16_16;

	case PIPE_FORMAT_R16G16B16_FLOAT:
	case PIPE_FORMAT_R16G16B16A16_FLOAT:
		return V_028C70_COLOR_16_16_16_16_FLOAT;

	case PIPE_FORMAT_R32G32_FLOAT:
		return V_028C70_COLOR_32_32_FLOAT;

	case PIPE_FORMAT_R32G32_USCALED:
	case PIPE_FORMAT_R32G32_SSCALED:
	case PIPE_FORMAT_R32G32_SINT:
	case PIPE_FORMAT_R32G32_UINT:
		return V_028C70_COLOR_32_32;

	/* 96-bit buffers. */
	case PIPE_FORMAT_R32G32B32_FLOAT:
		return V_028C70_COLOR_32_32_32_FLOAT;

	/* 128-bit buffers. */
	case PIPE_FORMAT_R32G32B32A32_SNORM:
	case PIPE_FORMAT_R32G32B32A32_UNORM:
	case PIPE_FORMAT_R32G32B32A32_SSCALED:
	case PIPE_FORMAT_R32G32B32A32_USCALED:
	case PIPE_FORMAT_R32G32B32A32_SINT:
	case PIPE_FORMAT_R32G32B32A32_UINT:
		return V_028C70_COLOR_32_32_32_32;
	case PIPE_FORMAT_R32G32B32A32_FLOAT:
		return V_028C70_COLOR_32_32_32_32_FLOAT;

	/* YUV buffers. */
	case PIPE_FORMAT_UYVY:
	case PIPE_FORMAT_YUYV:
	default:
		return ~0U; /* Unsupported. */
	}
}

static uint32_t r600_colorformat_endian_swap(uint32_t colorformat)
{
	if (R600_BIG_ENDIAN) {
		switch(colorformat) {

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

static bool r600_is_sampler_format_supported(struct pipe_screen *screen, enum pipe_format format)
{
	return r600_translate_texformat(screen, format, NULL, NULL, NULL) != ~0U;
}

static bool r600_is_colorbuffer_format_supported(enum pipe_format format)
{
	return r600_translate_colorformat(format) != ~0U &&
		r600_translate_colorswap(format) != ~0U;
}

static bool r600_is_zs_format_supported(enum pipe_format format)
{
	return r600_translate_dbformat(format) != ~0U;
}

boolean evergreen_is_format_supported(struct pipe_screen *screen,
				      enum pipe_format format,
				      enum pipe_texture_target target,
				      unsigned sample_count,
				      unsigned usage)
{
	unsigned retval = 0;

	if (target >= PIPE_MAX_TEXTURE_TYPES) {
		R600_ERR("r600: unsupported texture type %d\n", target);
		return FALSE;
	}

	if (!util_format_is_supported(format, usage))
		return FALSE;

	/* Multisample */
	if (sample_count > 1)
		return FALSE;

	if ((usage & PIPE_BIND_SAMPLER_VIEW) &&
	    r600_is_sampler_format_supported(screen, format)) {
		retval |= PIPE_BIND_SAMPLER_VIEW;
	}

	if ((usage & (PIPE_BIND_RENDER_TARGET |
		      PIPE_BIND_DISPLAY_TARGET |
		      PIPE_BIND_SCANOUT |
		      PIPE_BIND_SHARED)) &&
	    r600_is_colorbuffer_format_supported(format)) {
		retval |= usage &
			  (PIPE_BIND_RENDER_TARGET |
			   PIPE_BIND_DISPLAY_TARGET |
			   PIPE_BIND_SCANOUT |
			   PIPE_BIND_SHARED);
	}

	if ((usage & PIPE_BIND_DEPTH_STENCIL) &&
	    r600_is_zs_format_supported(format)) {
		retval |= PIPE_BIND_DEPTH_STENCIL;
	}

	if ((usage & PIPE_BIND_VERTEX_BUFFER) &&
	    r600_is_vertex_format_supported(format)) {
		retval |= PIPE_BIND_VERTEX_BUFFER;
	}

	if (usage & PIPE_BIND_TRANSFER_READ)
		retval |= PIPE_BIND_TRANSFER_READ;
	if (usage & PIPE_BIND_TRANSFER_WRITE)
		retval |= PIPE_BIND_TRANSFER_WRITE;

	return retval == usage;
}

static void evergreen_set_blend_color(struct pipe_context *ctx,
					const struct pipe_blend_color *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);

	if (rstate == NULL)
		return;

	rstate->id = R600_PIPE_STATE_BLEND_COLOR;
	r600_pipe_state_add_reg(rstate, R_028414_CB_BLEND_RED, fui(state->color[0]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028418_CB_BLEND_GREEN, fui(state->color[1]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02841C_CB_BLEND_BLUE, fui(state->color[2]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028420_CB_BLEND_ALPHA, fui(state->color[3]), NULL, 0);

	free(rctx->states[R600_PIPE_STATE_BLEND_COLOR]);
	rctx->states[R600_PIPE_STATE_BLEND_COLOR] = rstate;
	r600_context_pipe_state_set(rctx, rstate);
}

static void *evergreen_create_blend_state(struct pipe_context *ctx,
					const struct pipe_blend_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_blend *blend = CALLOC_STRUCT(r600_pipe_blend);
	struct r600_pipe_state *rstate;
	uint32_t color_control, target_mask;
	/* FIXME there is more then 8 framebuffer */
	unsigned blend_cntl[8];

	if (blend == NULL) {
		return NULL;
	}

	rstate = &blend->rstate;

	rstate->id = R600_PIPE_STATE_BLEND;

	target_mask = 0;
	color_control = S_028808_MODE(1);
	if (state->logicop_enable) {
		color_control |= (state->logicop_func << 16) | (state->logicop_func << 20);
	} else {
		color_control |= (0xcc << 16);
	}
	/* we pretend 8 buffer are used, CB_SHADER_MASK will disable unused one */
	if (state->independent_blend_enable) {
		for (int i = 0; i < 8; i++) {
			target_mask |= (state->rt[i].colormask << (4 * i));
		}
	} else {
		for (int i = 0; i < 8; i++) {
			target_mask |= (state->rt[0].colormask << (4 * i));
		}
	}
	blend->cb_target_mask = target_mask;
	
	r600_pipe_state_add_reg(rstate, R_028808_CB_COLOR_CONTROL,
				color_control, NULL, 0);

	if (rctx->chip_class != CAYMAN)
		r600_pipe_state_add_reg(rstate, R_028C3C_PA_SC_AA_MASK, ~0, NULL, 0);
	else {
		r600_pipe_state_add_reg(rstate, CM_R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0, ~0, NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028C3C_PA_SC_AA_MASK_X0Y1_X1Y1, ~0, NULL, 0);
	}

	for (int i = 0; i < 8; i++) {
		/* state->rt entries > 0 only written if independent blending */
		const int j = state->independent_blend_enable ? i : 0;

		unsigned eqRGB = state->rt[j].rgb_func;
		unsigned srcRGB = state->rt[j].rgb_src_factor;
		unsigned dstRGB = state->rt[j].rgb_dst_factor;
		unsigned eqA = state->rt[j].alpha_func;
		unsigned srcA = state->rt[j].alpha_src_factor;
		unsigned dstA = state->rt[j].alpha_dst_factor;

		blend_cntl[i] = 0;
		if (!state->rt[j].blend_enable)
			continue;

		blend_cntl[i] |= S_028780_BLEND_CONTROL_ENABLE(1);
		blend_cntl[i] |= S_028780_COLOR_COMB_FCN(r600_translate_blend_function(eqRGB));
		blend_cntl[i] |= S_028780_COLOR_SRCBLEND(r600_translate_blend_factor(srcRGB));
		blend_cntl[i] |= S_028780_COLOR_DESTBLEND(r600_translate_blend_factor(dstRGB));

		if (srcA != srcRGB || dstA != dstRGB || eqA != eqRGB) {
			blend_cntl[i] |= S_028780_SEPARATE_ALPHA_BLEND(1);
			blend_cntl[i] |= S_028780_ALPHA_COMB_FCN(r600_translate_blend_function(eqA));
			blend_cntl[i] |= S_028780_ALPHA_SRCBLEND(r600_translate_blend_factor(srcA));
			blend_cntl[i] |= S_028780_ALPHA_DESTBLEND(r600_translate_blend_factor(dstA));
		}
	}
	for (int i = 0; i < 8; i++) {
		r600_pipe_state_add_reg(rstate, R_028780_CB_BLEND0_CONTROL + i * 4, blend_cntl[i], NULL, 0);
	}

	return rstate;
}

static void *evergreen_create_dsa_state(struct pipe_context *ctx,
				   const struct pipe_depth_stencil_alpha_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_dsa *dsa = CALLOC_STRUCT(r600_pipe_dsa);
	unsigned db_depth_control, alpha_test_control, alpha_ref;
	unsigned db_render_override, db_render_control;
	struct r600_pipe_state *rstate;

	if (dsa == NULL) {
		return NULL;
	}

	dsa->valuemask[0] = state->stencil[0].valuemask;
	dsa->valuemask[1] = state->stencil[1].valuemask;
	dsa->writemask[0] = state->stencil[0].writemask;
	dsa->writemask[1] = state->stencil[1].writemask;

	rstate = &dsa->rstate;

	rstate->id = R600_PIPE_STATE_DSA;
	db_depth_control = S_028800_Z_ENABLE(state->depth.enabled) |
		S_028800_Z_WRITE_ENABLE(state->depth.writemask) |
		S_028800_ZFUNC(state->depth.func);

	/* stencil */
	if (state->stencil[0].enabled) {
		db_depth_control |= S_028800_STENCIL_ENABLE(1);
		db_depth_control |= S_028800_STENCILFUNC(r600_translate_ds_func(state->stencil[0].func));
		db_depth_control |= S_028800_STENCILFAIL(r600_translate_stencil_op(state->stencil[0].fail_op));
		db_depth_control |= S_028800_STENCILZPASS(r600_translate_stencil_op(state->stencil[0].zpass_op));
		db_depth_control |= S_028800_STENCILZFAIL(r600_translate_stencil_op(state->stencil[0].zfail_op));

		if (state->stencil[1].enabled) {
			db_depth_control |= S_028800_BACKFACE_ENABLE(1);
			db_depth_control |= S_028800_STENCILFUNC_BF(r600_translate_ds_func(state->stencil[1].func));
			db_depth_control |= S_028800_STENCILFAIL_BF(r600_translate_stencil_op(state->stencil[1].fail_op));
			db_depth_control |= S_028800_STENCILZPASS_BF(r600_translate_stencil_op(state->stencil[1].zpass_op));
			db_depth_control |= S_028800_STENCILZFAIL_BF(r600_translate_stencil_op(state->stencil[1].zfail_op));
		}
	}

	/* alpha */
	alpha_test_control = 0;
	alpha_ref = 0;
	if (state->alpha.enabled) {
		alpha_test_control = S_028410_ALPHA_FUNC(state->alpha.func);
		alpha_test_control |= S_028410_ALPHA_TEST_ENABLE(1);
		alpha_ref = fui(state->alpha.ref_value);
	}
	dsa->alpha_ref = alpha_ref;

	/* misc */
	db_render_control = 0;
	db_render_override = S_02800C_FORCE_HIZ_ENABLE(V_02800C_FORCE_DISABLE) |
		S_02800C_FORCE_HIS_ENABLE0(V_02800C_FORCE_DISABLE) |
		S_02800C_FORCE_HIS_ENABLE1(V_02800C_FORCE_DISABLE);
	/* TODO db_render_override depends on query */
	r600_pipe_state_add_reg(rstate, R_028028_DB_STENCIL_CLEAR, 0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02802C_DB_DEPTH_CLEAR, 0x3F800000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028410_SX_ALPHA_TEST_CONTROL, alpha_test_control, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0286DC_SPI_FOG_CNTL, 0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028800_DB_DEPTH_CONTROL, db_depth_control, NULL, 0);
	/* The DB_SHADER_CONTROL mask is 0xFFFFFFBC since Z_EXPORT_ENABLE,
	 * STENCIL_EXPORT_ENABLE and KILL_ENABLE are controlled by
	 * evergreen_pipe_shader_ps().*/
	r600_pipe_state_add_reg(rstate, R_028000_DB_RENDER_CONTROL, db_render_control, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02800C_DB_RENDER_OVERRIDE, db_render_override, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AC0_DB_SRESULTS_COMPARE_STATE0, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AC4_DB_SRESULTS_COMPARE_STATE1, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AC8_DB_PRELOAD_CONTROL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028B70_DB_ALPHA_TO_MASK, 0x0000AA00, NULL, 0);
	dsa->db_render_override = db_render_override;

	return rstate;
}

static void *evergreen_create_rs_state(struct pipe_context *ctx,
					const struct pipe_rasterizer_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_rasterizer *rs = CALLOC_STRUCT(r600_pipe_rasterizer);
	struct r600_pipe_state *rstate;
	unsigned tmp;
	unsigned prov_vtx = 1, polygon_dual_mode;
	unsigned clip_rule;
	float psize_min, psize_max;

	if (rs == NULL) {
		return NULL;
	}

	polygon_dual_mode = (state->fill_front != PIPE_POLYGON_MODE_FILL ||
				state->fill_back != PIPE_POLYGON_MODE_FILL);

	if (state->flatshade_first)
		prov_vtx = 0;

	rstate = &rs->rstate;
	rs->flatshade = state->flatshade;
	rs->sprite_coord_enable = state->sprite_coord_enable;
	rs->two_side = state->light_twoside;
	rs->clip_plane_enable = state->clip_plane_enable;
	rs->pa_sc_line_stipple = state->line_stipple_enable ?
				S_028A0C_LINE_PATTERN(state->line_stipple_pattern) |
				S_028A0C_REPEAT_COUNT(state->line_stipple_factor) : 0;
	rs->pa_su_sc_mode_cntl =
		S_028814_PROVOKING_VTX_LAST(prov_vtx) |
		S_028814_CULL_FRONT(state->rasterizer_discard || (state->cull_face & PIPE_FACE_FRONT) ? 1 : 0) |
		S_028814_CULL_BACK(state->rasterizer_discard || (state->cull_face & PIPE_FACE_BACK) ? 1 : 0) |
		S_028814_FACE(!state->front_ccw) |
		S_028814_POLY_OFFSET_FRONT_ENABLE(state->offset_tri) |
		S_028814_POLY_OFFSET_BACK_ENABLE(state->offset_tri) |
		S_028814_POLY_OFFSET_PARA_ENABLE(state->offset_tri) |
		S_028814_POLY_MODE(polygon_dual_mode) |
		S_028814_POLYMODE_FRONT_PTYPE(r600_translate_fill(state->fill_front)) |
		S_028814_POLYMODE_BACK_PTYPE(r600_translate_fill(state->fill_back));
	rs->pa_cl_clip_cntl =
		S_028810_PS_UCP_MODE(3) |
		S_028810_ZCLIP_NEAR_DISABLE(!state->depth_clip) |
		S_028810_ZCLIP_FAR_DISABLE(!state->depth_clip) |
		S_028810_DX_LINEAR_ATTR_CLIP_ENA(1);

	clip_rule = state->scissor ? 0xAAAA : 0xFFFF;

	/* offset */
	rs->offset_units = state->offset_units;
	rs->offset_scale = state->offset_scale * 12.0f;

	rstate->id = R600_PIPE_STATE_RASTERIZER;
	tmp = S_0286D4_FLAT_SHADE_ENA(1);
	if (state->sprite_coord_enable) {
		tmp |= S_0286D4_PNT_SPRITE_ENA(1) |
			S_0286D4_PNT_SPRITE_OVRD_X(2) |
			S_0286D4_PNT_SPRITE_OVRD_Y(3) |
			S_0286D4_PNT_SPRITE_OVRD_Z(0) |
			S_0286D4_PNT_SPRITE_OVRD_W(1);
		if (state->sprite_coord_mode != PIPE_SPRITE_COORD_UPPER_LEFT) {
			tmp |= S_0286D4_PNT_SPRITE_TOP_1(1);
		}
	}
	r600_pipe_state_add_reg(rstate, R_0286D4_SPI_INTERP_CONTROL_0, tmp, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028820_PA_CL_NANINF_CNTL, 0x00000000, NULL, 0);
	/* point size 12.4 fixed point */
	tmp = (unsigned)(state->point_size * 8.0);
	r600_pipe_state_add_reg(rstate, R_028A00_PA_SU_POINT_SIZE, S_028A00_HEIGHT(tmp) | S_028A00_WIDTH(tmp), NULL, 0);

	if (state->point_size_per_vertex) {
		psize_min = util_get_min_point_size(state);
		psize_max = 8192;
	} else {
		/* Force the point size to be as if the vertex output was disabled. */
		psize_min = state->point_size;
		psize_max = state->point_size;
	}
	/* Divide by two, because 0.5 = 1 pixel. */
	r600_pipe_state_add_reg(rstate, R_028A04_PA_SU_POINT_MINMAX,
				S_028A04_MIN_SIZE(r600_pack_float_12p4(psize_min/2)) |
				S_028A04_MAX_SIZE(r600_pack_float_12p4(psize_max/2)),
				NULL, 0);

	tmp = (unsigned)state->line_width * 8;
	r600_pipe_state_add_reg(rstate, R_028A08_PA_SU_LINE_CNTL, S_028A08_WIDTH(tmp), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A48_PA_SC_MODE_CNTL_0,
				S_028A48_LINE_STIPPLE_ENABLE(state->line_stipple_enable),
				NULL, 0);

	if (rctx->chip_class == CAYMAN) {
		r600_pipe_state_add_reg(rstate, CM_R_028BDC_PA_SC_LINE_CNTL, 0x00000400, NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028BE4_PA_SU_VTX_CNTL,
					S_028C08_PIX_CENTER_HALF(state->gl_rasterization_rules),
					NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028BE8_PA_CL_GB_VERT_CLIP_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028BEC_PA_CL_GB_VERT_DISC_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028BF0_PA_CL_GB_HORZ_CLIP_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, CM_R_028BF4_PA_CL_GB_HORZ_DISC_ADJ, 0x3F800000, NULL, 0);


	} else {
		r600_pipe_state_add_reg(rstate, R_028C00_PA_SC_LINE_CNTL, 0x00000400, NULL, 0);

		r600_pipe_state_add_reg(rstate, R_028C0C_PA_CL_GB_VERT_CLIP_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_028C10_PA_CL_GB_VERT_DISC_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_028C14_PA_CL_GB_HORZ_CLIP_ADJ, 0x3F800000, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_028C18_PA_CL_GB_HORZ_DISC_ADJ, 0x3F800000, NULL, 0);

		r600_pipe_state_add_reg(rstate, R_028C08_PA_SU_VTX_CNTL,
					S_028C08_PIX_CENTER_HALF(state->gl_rasterization_rules),
					NULL, 0);
	}
	r600_pipe_state_add_reg(rstate, R_028B7C_PA_SU_POLY_OFFSET_CLAMP, fui(state->offset_clamp), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02820C_PA_SC_CLIPRECT_RULE, clip_rule, NULL, 0);
	return rstate;
}

void *evergreen_create_sampler_state(struct pipe_context *ctx,
          const struct pipe_sampler_state *state);

void *evergreen_create_sampler_state(struct pipe_context *ctx,
					const struct pipe_sampler_state *state)
{
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);
	union util_color uc;
	unsigned aniso_flag_offset = state->max_anisotropy > 1 ? 2 : 0;

	if (rstate == NULL) {
		return NULL;
	}

	rstate->id = R600_PIPE_STATE_SAMPLER;
	util_pack_color(state->border_color.f, PIPE_FORMAT_B8G8R8A8_UNORM, &uc);
	r600_pipe_state_add_reg_noblock(rstate, R_03C000_SQ_TEX_SAMPLER_WORD0_0,
			S_03C000_CLAMP_X(r600_tex_wrap(state->wrap_s)) |
			S_03C000_CLAMP_Y(r600_tex_wrap(state->wrap_t)) |
			S_03C000_CLAMP_Z(r600_tex_wrap(state->wrap_r)) |
			S_03C000_XY_MAG_FILTER(r600_tex_filter(state->mag_img_filter) | aniso_flag_offset) |
			S_03C000_XY_MIN_FILTER(r600_tex_filter(state->min_img_filter) | aniso_flag_offset) |
			S_03C000_MIP_FILTER(r600_tex_mipfilter(state->min_mip_filter)) |
			S_03C000_MAX_ANISO(r600_tex_aniso_filter(state->max_anisotropy)) |
			S_03C000_DEPTH_COMPARE_FUNCTION(r600_tex_compare(state->compare_func)) |
			S_03C000_BORDER_COLOR_TYPE(uc.ui ? V_03C000_SQ_TEX_BORDER_COLOR_REGISTER : 0), NULL, 0);
	r600_pipe_state_add_reg_noblock(rstate, R_03C004_SQ_TEX_SAMPLER_WORD1_0,
			S_03C004_MIN_LOD(S_FIXED(CLAMP(state->min_lod, 0, 15), 8)) |
			S_03C004_MAX_LOD(S_FIXED(CLAMP(state->max_lod, 0, 15), 8)),
			NULL, 0);
	r600_pipe_state_add_reg_noblock(rstate, R_03C008_SQ_TEX_SAMPLER_WORD2_0,
					S_03C008_LOD_BIAS(S_FIXED(CLAMP(state->lod_bias, -16, 16), 8)) |
					(state->seamless_cube_map ? 0 : S_03C008_DISABLE_CUBE_WRAP(1)) |
					S_03C008_TYPE(1),
					NULL, 0);

	if (uc.ui) {
		r600_pipe_state_add_reg_noblock(rstate, R_00A404_TD_PS_SAMPLER0_BORDER_RED, fui(state->border_color.f[0]), NULL, 0);
		r600_pipe_state_add_reg_noblock(rstate, R_00A408_TD_PS_SAMPLER0_BORDER_GREEN, fui(state->border_color.f[1]), NULL, 0);
		r600_pipe_state_add_reg_noblock(rstate, R_00A40C_TD_PS_SAMPLER0_BORDER_BLUE, fui(state->border_color.f[2]), NULL, 0);
		r600_pipe_state_add_reg_noblock(rstate, R_00A410_TD_PS_SAMPLER0_BORDER_ALPHA, fui(state->border_color.f[3]), NULL, 0);
	}
	return rstate;
}

static struct pipe_sampler_view *evergreen_create_sampler_view(struct pipe_context *ctx,
							struct pipe_resource *texture,
							const struct pipe_sampler_view *state)
{
	struct r600_pipe_sampler_view *view = CALLOC_STRUCT(r600_pipe_sampler_view);
	struct r600_pipe_resource_state *rstate;
	struct r600_resource_texture *tmp = (struct r600_resource_texture*)texture;
	unsigned format, endian;
	uint32_t word4 = 0, yuv_format = 0, pitch = 0;
	unsigned char swizzle[4], array_mode = 0, tile_type = 0;
	unsigned height, depth;

	if (view == NULL)
		return NULL;
	rstate = &view->state;

	/* initialize base object */
	view->base = *state;
	view->base.texture = NULL;
	pipe_reference(NULL, &texture->reference);
	view->base.texture = texture;
	view->base.reference.count = 1;
	view->base.context = ctx;

	swizzle[0] = state->swizzle_r;
	swizzle[1] = state->swizzle_g;
	swizzle[2] = state->swizzle_b;
	swizzle[3] = state->swizzle_a;

	format = r600_translate_texformat(ctx->screen, state->format,
					  swizzle,
					  &word4, &yuv_format);
	if (format == ~0) {
		format = 0;
	}

	if (tmp->depth && !tmp->is_flushing_texture) {
		r600_texture_depth_flush(ctx, texture, TRUE);
		tmp = tmp->flushed_depth_texture;
	}

	endian = r600_colorformat_endian_swap(format);

	height = texture->height0;
	depth = texture->depth0;

	pitch = align(tmp->pitch_in_blocks[0] *
		      util_format_get_blockwidth(state->format), 8);
	array_mode = tmp->array_mode[0];
	tile_type = tmp->tile_type;

	if (texture->target == PIPE_TEXTURE_1D_ARRAY) {
	        height = 1;
		depth = texture->array_size;
	} else if (texture->target == PIPE_TEXTURE_2D_ARRAY) {
		depth = texture->array_size;
	}

	rstate->bo[0] = &tmp->resource;
	rstate->bo[1] = &tmp->resource;
	rstate->bo_usage[0] = RADEON_USAGE_READ;
	rstate->bo_usage[1] = RADEON_USAGE_READ;

	rstate->val[0] = (S_030000_DIM(r600_tex_dim(texture->target)) |
			  S_030000_PITCH((pitch / 8) - 1) |
			  S_030000_NON_DISP_TILING_ORDER(tile_type) |
			  S_030000_TEX_WIDTH(texture->width0 - 1));
	rstate->val[1] = (S_030004_TEX_HEIGHT(height - 1) |
			  S_030004_TEX_DEPTH(depth - 1) |
			  S_030004_ARRAY_MODE(array_mode));
	rstate->val[2] = (tmp->offset[0] + r600_resource_va(ctx->screen, texture)) >> 8;
	rstate->val[3] = (tmp->offset[1] + r600_resource_va(ctx->screen, texture)) >> 8;
	rstate->val[4] = (word4 |
			  S_030010_SRF_MODE_ALL(V_030010_SRF_MODE_ZERO_CLAMP_MINUS_ONE) |
			  S_030010_ENDIAN_SWAP(endian) |
			  S_030010_BASE_LEVEL(state->u.tex.first_level));
	rstate->val[5] = (S_030014_LAST_LEVEL(state->u.tex.last_level) |
			  S_030014_BASE_ARRAY(state->u.tex.first_layer) |
			  S_030014_LAST_ARRAY(state->u.tex.last_layer));
	rstate->val[6] = (S_030018_MAX_ANISO(4 /* max 16 samples */));
	rstate->val[7] = (S_03001C_DATA_FORMAT(format) |
			  S_03001C_TYPE(V_03001C_SQ_TEX_VTX_VALID_TEXTURE));

	return &view->base;
}

static void evergreen_set_vs_sampler_view(struct pipe_context *ctx, unsigned count,
					struct pipe_sampler_view **views)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_sampler_view **resource = (struct r600_pipe_sampler_view **)views;

	for (int i = 0; i < count; i++) {
		if (resource[i]) {
			evergreen_context_pipe_state_set_vs_resource(rctx, &resource[i]->state,
								     i + R600_MAX_CONST_BUFFERS);
		}
	}
}

static void evergreen_set_ps_sampler_view(struct pipe_context *ctx, unsigned count,
					struct pipe_sampler_view **views)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_sampler_view **resource = (struct r600_pipe_sampler_view **)views;
	int i;
	int has_depth = 0;

	for (i = 0; i < count; i++) {
		if (&rctx->ps_samplers.views[i]->base != views[i]) {
			if (resource[i]) {
				if (((struct r600_resource_texture *)resource[i]->base.texture)->depth)
					has_depth = 1;
				evergreen_context_pipe_state_set_ps_resource(rctx, &resource[i]->state,
									     i + R600_MAX_CONST_BUFFERS);
			} else
				evergreen_context_pipe_state_set_ps_resource(rctx, NULL,
									     i + R600_MAX_CONST_BUFFERS);

			pipe_sampler_view_reference(
				(struct pipe_sampler_view **)&rctx->ps_samplers.views[i],
				views[i]);
		} else {
			if (resource[i]) {
				if (((struct r600_resource_texture *)resource[i]->base.texture)->depth)
					has_depth = 1;
			}
		}
	}
	for (i = count; i < NUM_TEX_UNITS; i++) {
		if (rctx->ps_samplers.views[i]) {
			evergreen_context_pipe_state_set_ps_resource(rctx, NULL,
								     i + R600_MAX_CONST_BUFFERS);
			pipe_sampler_view_reference((struct pipe_sampler_view **)&rctx->ps_samplers.views[i], NULL);
		}
	}
	rctx->have_depth_texture = has_depth;
	rctx->ps_samplers.n_views = count;
}

static void evergreen_bind_ps_sampler(struct pipe_context *ctx, unsigned count, void **states)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state **rstates = (struct r600_pipe_state **)states;

	if (count)
		r600_inval_texture_cache(rctx);

	memcpy(rctx->ps_samplers.samplers, states, sizeof(void*) * count);
	rctx->ps_samplers.n_samplers = count;

	for (int i = 0; i < count; i++) {
		evergreen_context_pipe_state_set_ps_sampler(rctx, rstates[i], i);
	}
}

static void evergreen_bind_vs_sampler(struct pipe_context *ctx, unsigned count, void **states)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state **rstates = (struct r600_pipe_state **)states;

	if (count)
		r600_inval_texture_cache(rctx);

	for (int i = 0; i < count; i++) {
		evergreen_context_pipe_state_set_vs_sampler(rctx, rstates[i], i);
	}
}

static void evergreen_set_clip_state(struct pipe_context *ctx,
				const struct pipe_clip_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);
	struct pipe_resource *cbuf;

	if (rstate == NULL)
		return;

	rctx->clip = *state;
	rstate->id = R600_PIPE_STATE_CLIP;
	for (int i = 0; i < 6; i++) {
		r600_pipe_state_add_reg(rstate,
					R_0285BC_PA_CL_UCP0_X + i * 16,
					fui(state->ucp[i][0]), NULL, 0);
		r600_pipe_state_add_reg(rstate,
					R_0285C0_PA_CL_UCP0_Y + i * 16,
					fui(state->ucp[i][1]) , NULL, 0);
		r600_pipe_state_add_reg(rstate,
					R_0285C4_PA_CL_UCP0_Z + i * 16,
					fui(state->ucp[i][2]), NULL, 0);
		r600_pipe_state_add_reg(rstate,
					R_0285C8_PA_CL_UCP0_W + i * 16,
					fui(state->ucp[i][3]), NULL, 0);
	}

	free(rctx->states[R600_PIPE_STATE_CLIP]);
	rctx->states[R600_PIPE_STATE_CLIP] = rstate;
	r600_context_pipe_state_set(rctx, rstate);

	cbuf = pipe_user_buffer_create(ctx->screen,
                                   state->ucp,
                                   4*4*8, /* 8*4 floats */
                                   PIPE_BIND_CONSTANT_BUFFER);
	r600_set_constant_buffer(ctx, PIPE_SHADER_VERTEX, 1, cbuf);
	pipe_resource_reference(&cbuf, NULL);
}

static void evergreen_set_polygon_stipple(struct pipe_context *ctx,
					 const struct pipe_poly_stipple *state)
{
}

static void evergreen_set_sample_mask(struct pipe_context *pipe, unsigned sample_mask)
{
}

static void evergreen_set_scissor_state(struct pipe_context *ctx,
					const struct pipe_scissor_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);
	uint32_t tl, br;

	if (rstate == NULL)
		return;

	rstate->id = R600_PIPE_STATE_SCISSOR;
	tl = S_028240_TL_X(state->minx) | S_028240_TL_Y(state->miny);
	br = S_028244_BR_X(state->maxx) | S_028244_BR_Y(state->maxy);
	r600_pipe_state_add_reg(rstate,
				R_028210_PA_SC_CLIPRECT_0_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028214_PA_SC_CLIPRECT_0_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028218_PA_SC_CLIPRECT_1_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_02821C_PA_SC_CLIPRECT_1_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028220_PA_SC_CLIPRECT_2_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028224_PA_SC_CLIPRECT_2_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028228_PA_SC_CLIPRECT_3_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_02822C_PA_SC_CLIPRECT_3_BR, br,
				NULL, 0);

	free(rctx->states[R600_PIPE_STATE_SCISSOR]);
	rctx->states[R600_PIPE_STATE_SCISSOR] = rstate;
	r600_context_pipe_state_set(rctx, rstate);
}

static void evergreen_set_viewport_state(struct pipe_context *ctx,
					const struct pipe_viewport_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);

	if (rstate == NULL)
		return;

	rctx->viewport = *state;
	rstate->id = R600_PIPE_STATE_VIEWPORT;
	r600_pipe_state_add_reg(rstate, R_0282D0_PA_SC_VPORT_ZMIN_0, 0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0282D4_PA_SC_VPORT_ZMAX_0, 0x3F800000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02843C_PA_CL_VPORT_XSCALE_0, fui(state->scale[0]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028444_PA_CL_VPORT_YSCALE_0, fui(state->scale[1]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02844C_PA_CL_VPORT_ZSCALE_0, fui(state->scale[2]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028440_PA_CL_VPORT_XOFFSET_0, fui(state->translate[0]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028448_PA_CL_VPORT_YOFFSET_0, fui(state->translate[1]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028450_PA_CL_VPORT_ZOFFSET_0, fui(state->translate[2]), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028818_PA_CL_VTE_CNTL, 0x0000043F, NULL, 0);

	free(rctx->states[R600_PIPE_STATE_VIEWPORT]);
	rctx->states[R600_PIPE_STATE_VIEWPORT] = rstate;
	r600_context_pipe_state_set(rctx, rstate);
}

static void evergreen_cb(struct r600_context *rctx, struct r600_pipe_state *rstate,
			const struct pipe_framebuffer_state *state, int cb)
{
	struct r600_resource_texture *rtex;
	struct r600_surface *surf;
	unsigned level = state->cbufs[cb]->u.tex.level;
	unsigned pitch, slice;
	unsigned color_info;
	unsigned format, swap, ntype, endian;
	uint64_t offset;
	unsigned tile_type;
	const struct util_format_description *desc;
	int i;
	unsigned blend_clamp = 0, blend_bypass = 0;

	surf = (struct r600_surface *)state->cbufs[cb];
	rtex = (struct r600_resource_texture*)state->cbufs[cb]->texture;

	if (rtex->depth)
		rctx->have_depth_fb = TRUE;

	if (rtex->depth && !rtex->is_flushing_texture) {
	        r600_texture_depth_flush(&rctx->context, state->cbufs[cb]->texture, TRUE);
		rtex = rtex->flushed_depth_texture;
	}

	/* XXX quite sure for dx10+ hw don't need any offset hacks */
	offset = r600_texture_get_offset(rtex,
					 level, state->cbufs[cb]->u.tex.first_layer);
	pitch = rtex->pitch_in_blocks[level] / 8 - 1;
	slice = rtex->pitch_in_blocks[level] * surf->aligned_height / 64 - 1;
	desc = util_format_description(surf->base.format);
	for (i = 0; i < 4; i++) {
		if (desc->channel[i].type != UTIL_FORMAT_TYPE_VOID) {
			break;
		}
	}

	ntype = V_028C70_NUMBER_UNORM;
	if (desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB)
		ntype = V_028C70_NUMBER_SRGB;
	else if (desc->channel[i].type == UTIL_FORMAT_TYPE_SIGNED) {
		if (desc->channel[i].normalized)
			ntype = V_028C70_NUMBER_SNORM;
		else if (desc->channel[i].pure_integer)
			ntype = V_028C70_NUMBER_SINT;
	} else if (desc->channel[i].type == UTIL_FORMAT_TYPE_UNSIGNED) {
		if (desc->channel[i].normalized)
			ntype = V_028C70_NUMBER_UNORM;
		else if (desc->channel[i].pure_integer)
			ntype = V_028C70_NUMBER_UINT;
	}

	format = r600_translate_colorformat(surf->base.format);
	swap = r600_translate_colorswap(surf->base.format);
	if (rtex->resource.b.b.b.usage == PIPE_USAGE_STAGING) {
		endian = ENDIAN_NONE;
	} else {
		endian = r600_colorformat_endian_swap(format);
	}

	/* blend clamp should be set for all NORM/SRGB types */
	if (ntype == V_028C70_NUMBER_UNORM || ntype == V_028C70_NUMBER_SNORM ||
	    ntype == V_028C70_NUMBER_SRGB)
		blend_clamp = 1;

	/* set blend bypass according to docs if SINT/UINT or
	   8/24 COLOR variants */
	if (ntype == V_028C70_NUMBER_UINT || ntype == V_028C70_NUMBER_SINT ||
	    format == V_028C70_COLOR_8_24 || format == V_028C70_COLOR_24_8 ||
	    format == V_028C70_COLOR_X24_8_32_FLOAT) {
		blend_clamp = 0;
		blend_bypass = 1;
	}

	color_info = S_028C70_FORMAT(format) |
		S_028C70_COMP_SWAP(swap) |
		S_028C70_ARRAY_MODE(rtex->array_mode[level]) |
		S_028C70_BLEND_CLAMP(blend_clamp) |
		S_028C70_BLEND_BYPASS(blend_bypass) |
		S_028C70_NUMBER_TYPE(ntype) |
		S_028C70_ENDIAN(endian);

	/* EXPORT_NORM is an optimzation that can be enabled for better
	 * performance in certain cases.
	 * EXPORT_NORM can be enabled if:
	 * - 11-bit or smaller UNORM/SNORM/SRGB
	 * - 16-bit or smaller FLOAT
	 */
	/* FIXME: This should probably be the same for all CBs if we want
	 * useful alpha tests. */
	if (desc->colorspace != UTIL_FORMAT_COLORSPACE_ZS &&
	    ((desc->channel[i].size < 12 &&
	      desc->channel[i].type != UTIL_FORMAT_TYPE_FLOAT &&
	      ntype != V_028C70_NUMBER_UINT && ntype != V_028C70_NUMBER_SINT) ||
	     (desc->channel[i].size < 17 &&
	      desc->channel[i].type == UTIL_FORMAT_TYPE_FLOAT))) {
		color_info |= S_028C70_SOURCE_FORMAT(V_028C70_EXPORT_4C_16BPC);
		rctx->export_16bpc = true;
	} else {
		rctx->export_16bpc = false;
	}
	rctx->alpha_ref_dirty = true;

	if (rtex->array_mode[level] > V_028C70_ARRAY_LINEAR_ALIGNED) {
		tile_type = rtex->tile_type;
	} else /* workaround for linear buffers */
		tile_type = 1;

	offset += r600_resource_va(rctx->context.screen, state->cbufs[cb]->texture);
	offset >>= 8;

	/* FIXME handle enabling of CB beyond BASE8 which has different offset */
	r600_pipe_state_add_reg(rstate,
				R_028C60_CB_COLOR0_BASE + cb * 0x3C,
				offset, &rtex->resource, RADEON_USAGE_READWRITE);
	r600_pipe_state_add_reg(rstate,
				R_028C78_CB_COLOR0_DIM + cb * 0x3C,
				0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028C70_CB_COLOR0_INFO + cb * 0x3C,
				color_info, &rtex->resource, RADEON_USAGE_READWRITE);
	r600_pipe_state_add_reg(rstate,
				R_028C64_CB_COLOR0_PITCH + cb * 0x3C,
				S_028C64_PITCH_TILE_MAX(pitch),
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028C68_CB_COLOR0_SLICE + cb * 0x3C,
				S_028C68_SLICE_TILE_MAX(slice),
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028C6C_CB_COLOR0_VIEW + cb * 0x3C,
				0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028C74_CB_COLOR0_ATTRIB + cb * 0x3C,
				S_028C74_NON_DISP_TILING_ORDER(tile_type),
				&rtex->resource, RADEON_USAGE_READWRITE);
}

static void evergreen_db(struct r600_context *rctx, struct r600_pipe_state *rstate,
			 const struct pipe_framebuffer_state *state)
{
	struct r600_resource_texture *rtex;
	struct r600_surface *surf;
	unsigned level, first_layer, pitch, slice, format, array_mode;
	uint64_t offset;

	if (state->zsbuf == NULL)
		return;

	surf = (struct r600_surface *)state->zsbuf;
	level = surf->base.u.tex.level;
	rtex = (struct r600_resource_texture*)surf->base.texture;

	/* XXX remove this once tiling is properly supported */
	array_mode = rtex->array_mode[level] ? rtex->array_mode[level] :
					       V_028C70_ARRAY_1D_TILED_THIN1;

	first_layer = surf->base.u.tex.first_layer;
	offset = r600_texture_get_offset(rtex, level, first_layer);
	pitch = rtex->pitch_in_blocks[level] / 8 - 1;
	slice = rtex->pitch_in_blocks[level] * surf->aligned_height / 64 - 1;
	format = r600_translate_dbformat(rtex->real_format);

	offset += r600_resource_va(rctx->context.screen, surf->base.texture);
	offset >>= 8;

	r600_pipe_state_add_reg(rstate, R_028048_DB_Z_READ_BASE,
				offset, &rtex->resource, RADEON_USAGE_READWRITE);
	r600_pipe_state_add_reg(rstate, R_028050_DB_Z_WRITE_BASE,
				offset, &rtex->resource, RADEON_USAGE_READWRITE);
	r600_pipe_state_add_reg(rstate, R_028008_DB_DEPTH_VIEW, 0x00000000, NULL, 0);

	if (rtex->stencil) {
		uint64_t stencil_offset =
			r600_texture_get_offset(rtex->stencil, level, first_layer);

		stencil_offset += r600_resource_va(rctx->context.screen, (void*)rtex->stencil);
		stencil_offset >>= 8;

		r600_pipe_state_add_reg(rstate, R_02804C_DB_STENCIL_READ_BASE,
					stencil_offset, &rtex->stencil->resource, RADEON_USAGE_READWRITE);
		r600_pipe_state_add_reg(rstate, R_028054_DB_STENCIL_WRITE_BASE,
					stencil_offset, &rtex->stencil->resource, RADEON_USAGE_READWRITE);
		r600_pipe_state_add_reg(rstate, R_028044_DB_STENCIL_INFO,
					1, &rtex->stencil->resource, RADEON_USAGE_READWRITE);
	} else {
		r600_pipe_state_add_reg(rstate, R_028044_DB_STENCIL_INFO,
					0, NULL, RADEON_USAGE_READWRITE);
	}

	r600_pipe_state_add_reg(rstate, R_028040_DB_Z_INFO,
				S_028040_ARRAY_MODE(array_mode) | S_028040_FORMAT(format),
				&rtex->resource, RADEON_USAGE_READWRITE);
	r600_pipe_state_add_reg(rstate, R_028058_DB_DEPTH_SIZE,
				S_028058_PITCH_TILE_MAX(pitch),
				NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02805C_DB_DEPTH_SLICE,
				S_02805C_SLICE_TILE_MAX(slice),
				NULL, 0);
}

static void evergreen_set_framebuffer_state(struct pipe_context *ctx,
					const struct pipe_framebuffer_state *state)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = CALLOC_STRUCT(r600_pipe_state);
	uint32_t shader_mask, tl, br;
	int tl_x, tl_y, br_x, br_y;

	if (rstate == NULL)
		return;

	r600_flush_framebuffer(rctx, false);

	/* unreference old buffer and reference new one */
	rstate->id = R600_PIPE_STATE_FRAMEBUFFER;

	util_copy_framebuffer_state(&rctx->framebuffer, state);

	/* build states */
	rctx->have_depth_fb = 0;
	rctx->nr_cbufs = state->nr_cbufs;
	for (int i = 0; i < state->nr_cbufs; i++) {
		evergreen_cb(rctx, rstate, state, i);
	}
	if (state->zsbuf) {
		evergreen_db(rctx, rstate, state);
	}

	shader_mask = 0;
	for (int i = 0; i < state->nr_cbufs; i++) {
		shader_mask |= 0xf << (i * 4);
	}
	tl_x = 0;
	tl_y = 0;
	br_x = state->width;
	br_y = state->height;
	/* EG hw workaround */
	if (br_x == 0)
		tl_x = 1;
	if (br_y == 0)
		tl_y = 1;
	/* cayman hw workaround */
	if (rctx->chip_class == CAYMAN) {
		if (br_x == 1 && br_y == 1)
			br_x = 2;
	}
	tl = S_028240_TL_X(tl_x) | S_028240_TL_Y(tl_y);
	br = S_028244_BR_X(br_x) | S_028244_BR_Y(br_y);

	r600_pipe_state_add_reg(rstate,
				R_028240_PA_SC_GENERIC_SCISSOR_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028244_PA_SC_GENERIC_SCISSOR_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028250_PA_SC_VPORT_SCISSOR_0_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028254_PA_SC_VPORT_SCISSOR_0_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028030_PA_SC_SCREEN_SCISSOR_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028034_PA_SC_SCREEN_SCISSOR_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028204_PA_SC_WINDOW_SCISSOR_TL, tl,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028208_PA_SC_WINDOW_SCISSOR_BR, br,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028200_PA_SC_WINDOW_OFFSET, 0x00000000,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028230_PA_SC_EDGERULE, 0xAAAAAAAA,
				NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02823C_CB_SHADER_MASK,
				shader_mask, NULL, 0);


	if (rctx->chip_class == CAYMAN) {
		r600_pipe_state_add_reg(rstate, CM_R_028BE0_PA_SC_AA_CONFIG,
					0x00000000, NULL, 0);
	} else {
		r600_pipe_state_add_reg(rstate, R_028C04_PA_SC_AA_CONFIG,
					0x00000000, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_028C1C_PA_SC_AA_SAMPLE_LOCS_0,
					0x00000000, NULL, 0);
	}

	free(rctx->states[R600_PIPE_STATE_FRAMEBUFFER]);
	rctx->states[R600_PIPE_STATE_FRAMEBUFFER] = rstate;
	r600_context_pipe_state_set(rctx, rstate);

	if (state->zsbuf) {
		evergreen_polygon_offset_update(rctx);
	}
}

void evergreen_init_state_functions(struct r600_context *rctx)
{


	rctx->context.create_blend_state = evergreen_create_blend_state;
	rctx->context.create_depth_stencil_alpha_state = evergreen_create_dsa_state;
	rctx->context.create_fs_state = r600_create_shader_state;
	rctx->context.create_rasterizer_state = evergreen_create_rs_state;
	rctx->context.create_sampler_state = evergreen_create_sampler_state;
	rctx->context.create_sampler_view = evergreen_create_sampler_view;
	rctx->context.create_vertex_elements_state = r600_create_vertex_elements;
	rctx->context.create_vs_state = r600_create_shader_state;
	rctx->context.bind_blend_state = r600_bind_blend_state;
	rctx->context.bind_depth_stencil_alpha_state = r600_bind_dsa_state;
	rctx->context.bind_fragment_sampler_states = evergreen_bind_ps_sampler;
	rctx->context.bind_fs_state = r600_bind_ps_shader;
	rctx->context.bind_rasterizer_state = r600_bind_rs_state;
	rctx->context.bind_vertex_elements_state = r600_bind_vertex_elements;
	rctx->context.bind_vertex_sampler_states = evergreen_bind_vs_sampler;
	rctx->context.bind_vs_state = r600_bind_vs_shader;
	rctx->context.delete_blend_state = r600_delete_state;
	rctx->context.delete_depth_stencil_alpha_state = r600_delete_state;
	rctx->context.delete_fs_state = r600_delete_ps_shader;
	rctx->context.delete_rasterizer_state = r600_delete_rs_state;
	rctx->context.delete_sampler_state = r600_delete_state;
	rctx->context.delete_vertex_elements_state = r600_delete_vertex_element;
	rctx->context.delete_vs_state = r600_delete_vs_shader;
	rctx->context.set_blend_color = evergreen_set_blend_color;
	rctx->context.set_clip_state = evergreen_set_clip_state;
	rctx->context.set_constant_buffer = r600_set_constant_buffer;
	rctx->context.set_fragment_sampler_views = evergreen_set_ps_sampler_view;
	rctx->context.set_framebuffer_state = evergreen_set_framebuffer_state;
	rctx->context.set_polygon_stipple = evergreen_set_polygon_stipple;
	rctx->context.set_sample_mask = evergreen_set_sample_mask;
	rctx->context.set_scissor_state = evergreen_set_scissor_state;
	rctx->context.set_stencil_ref = r600_set_pipe_stencil_ref;
	rctx->context.set_vertex_buffers = r600_set_vertex_buffers;
	rctx->context.set_index_buffer = r600_set_index_buffer;
	rctx->context.set_vertex_sampler_views = evergreen_set_vs_sampler_view;
	rctx->context.set_viewport_state = evergreen_set_viewport_state;
	rctx->context.sampler_view_destroy = r600_sampler_view_destroy;
	rctx->context.redefine_user_buffer = u_default_redefine_user_buffer;
	rctx->context.texture_barrier = r600_texture_barrier;
	rctx->context.create_stream_output_target = r600_create_so_target;
	rctx->context.stream_output_target_destroy = r600_so_target_destroy;
	
	evergreen_init_compute_state_functions(rctx);
	rctx->context.set_stream_output_targets = r600_set_so_targets;
}

static void cayman_init_config(struct r600_context *rctx)
{
  	struct r600_pipe_state *rstate = &rctx->config;
	unsigned tmp;

	tmp = 0x00000000;
	tmp |= S_008C00_EXPORT_SRC_C(1);
	r600_pipe_state_add_reg(rstate, R_008C00_SQ_CONFIG, tmp, NULL, 0);

	/* always set the temp clauses */
	r600_pipe_state_add_reg(rstate, R_008C04_SQ_GPR_RESOURCE_MGMT_1, S_008C04_NUM_CLAUSE_TEMP_GPRS(4), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_008C10_SQ_GLOBAL_GPR_RESOURCE_MGMT_1, 0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_008C14_SQ_GLOBAL_GPR_RESOURCE_MGMT_2, 0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_008D8C_SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, (1 << 8), NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028A4C_PA_SC_MODE_CNTL_1, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028A10_VGT_OUTPUT_PATH_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A14_VGT_HOS_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A18_VGT_HOS_MAX_TESS_LEVEL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A1C_VGT_HOS_MIN_TESS_LEVEL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A20_VGT_HOS_REUSE_DEPTH, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A24_VGT_GROUP_PRIM_TYPE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A28_VGT_GROUP_FIRST_DECR, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A2C_VGT_GROUP_DECR, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A30_VGT_GROUP_VECT_0_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A34_VGT_GROUP_VECT_1_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A38_VGT_GROUP_VECT_0_FMT_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A3C_VGT_GROUP_VECT_1_FMT_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A40_VGT_GS_MODE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028B94_VGT_STRMOUT_CONFIG, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028B98_VGT_STRMOUT_BUFFER_CONFIG, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, CM_R_028AA8_IA_MULTI_VGT_PARAM, S_028AA8_SWITCH_ON_EOP(1) | S_028AA8_PARTIAL_VS_WAVE_ON(1) | S_028AA8_PRIMGROUP_SIZE(63), NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AB4_VGT_REUSE_OFF, 0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AB8_VGT_VTX_CNT_EN, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_008A14_PA_CL_ENHANCE, (3 << 1) | 1, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028380_SQ_VTX_SEMANTIC_0, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028384_SQ_VTX_SEMANTIC_1, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028388_SQ_VTX_SEMANTIC_2, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02838C_SQ_VTX_SEMANTIC_3, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028390_SQ_VTX_SEMANTIC_4, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028394_SQ_VTX_SEMANTIC_5, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028398_SQ_VTX_SEMANTIC_6, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02839C_SQ_VTX_SEMANTIC_7, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A0_SQ_VTX_SEMANTIC_8, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A4_SQ_VTX_SEMANTIC_9, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A8_SQ_VTX_SEMANTIC_10, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283AC_SQ_VTX_SEMANTIC_11, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B0_SQ_VTX_SEMANTIC_12, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B4_SQ_VTX_SEMANTIC_13, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B8_SQ_VTX_SEMANTIC_14, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283BC_SQ_VTX_SEMANTIC_15, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C0_SQ_VTX_SEMANTIC_16, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C4_SQ_VTX_SEMANTIC_17, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C8_SQ_VTX_SEMANTIC_18, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283CC_SQ_VTX_SEMANTIC_19, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D0_SQ_VTX_SEMANTIC_20, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D4_SQ_VTX_SEMANTIC_21, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D8_SQ_VTX_SEMANTIC_22, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283DC_SQ_VTX_SEMANTIC_23, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E0_SQ_VTX_SEMANTIC_24, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E4_SQ_VTX_SEMANTIC_25, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E8_SQ_VTX_SEMANTIC_26, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283EC_SQ_VTX_SEMANTIC_27, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F0_SQ_VTX_SEMANTIC_28, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F4_SQ_VTX_SEMANTIC_29, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F8_SQ_VTX_SEMANTIC_30, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283FC_SQ_VTX_SEMANTIC_31, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028810_PA_CL_CLIP_CNTL, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, CM_R_028BD4_PA_SC_CENTROID_PRIORITY_0, 0x76543210, NULL, 0);
	r600_pipe_state_add_reg(rstate, CM_R_028BD8_PA_SC_CENTROID_PRIORITY_1, 0xfedcba98, NULL, 0);

	r600_pipe_state_add_reg(rstate, CM_R_0288E8_SQ_LDS_ALLOC, 0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0288EC_SQ_LDS_ALLOC_PS, 0, NULL, 0);

	r600_pipe_state_add_reg(rstate, CM_R_028804_DB_EQAA, 0x110000, NULL, 0);
	r600_context_pipe_state_set(rctx, rstate);
}

void evergreen_init_config(struct r600_context *rctx)
{
	struct r600_pipe_state *rstate = &rctx->config;
	int ps_prio;
	int vs_prio;
	int gs_prio;
	int es_prio;
	int hs_prio, cs_prio, ls_prio;
	int num_ps_gprs;
	int num_vs_gprs;
	int num_gs_gprs;
	int num_es_gprs;
	int num_hs_gprs;
	int num_ls_gprs;
	int num_temp_gprs;
	int num_ps_threads;
	int num_vs_threads;
	int num_gs_threads;
	int num_es_threads;
	int num_hs_threads;
	int num_ls_threads;
	int num_ps_stack_entries;
	int num_vs_stack_entries;
	int num_gs_stack_entries;
	int num_es_stack_entries;
	int num_hs_stack_entries;
	int num_ls_stack_entries;
	enum radeon_family family;
	unsigned tmp;

	family = rctx->family;

	if (rctx->chip_class == CAYMAN) {
		cayman_init_config(rctx);
		return;
	}
		
	ps_prio = 0;
	vs_prio = 1;
	gs_prio = 2;
	es_prio = 3;
	hs_prio = 0;
	ls_prio = 0;
	cs_prio = 0;

	switch (family) {
	case CHIP_CEDAR:
	default:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 96;
		num_vs_threads = 16;
		num_gs_threads = 16;
		num_es_threads = 16;
		num_hs_threads = 16;
		num_ls_threads = 16;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	case CHIP_REDWOOD:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 20;
		num_gs_threads = 20;
		num_es_threads = 20;
		num_hs_threads = 20;
		num_ls_threads = 20;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	case CHIP_JUNIPER:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 20;
		num_gs_threads = 20;
		num_es_threads = 20;
		num_hs_threads = 20;
		num_ls_threads = 20;
		num_ps_stack_entries = 85;
		num_vs_stack_entries = 85;
		num_gs_stack_entries = 85;
		num_es_stack_entries = 85;
		num_hs_stack_entries = 85;
		num_ls_stack_entries = 85;
		break;
	case CHIP_CYPRESS:
	case CHIP_HEMLOCK:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 20;
		num_gs_threads = 20;
		num_es_threads = 20;
		num_hs_threads = 20;
		num_ls_threads = 20;
		num_ps_stack_entries = 85;
		num_vs_stack_entries = 85;
		num_gs_stack_entries = 85;
		num_es_stack_entries = 85;
		num_hs_stack_entries = 85;
		num_ls_stack_entries = 85;
		break;
	case CHIP_PALM:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 96;
		num_vs_threads = 16;
		num_gs_threads = 16;
		num_es_threads = 16;
		num_hs_threads = 16;
		num_ls_threads = 16;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	case CHIP_SUMO:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 96;
		num_vs_threads = 25;
		num_gs_threads = 25;
		num_es_threads = 25;
		num_hs_threads = 25;
		num_ls_threads = 25;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	case CHIP_SUMO2:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 96;
		num_vs_threads = 25;
		num_gs_threads = 25;
		num_es_threads = 25;
		num_hs_threads = 25;
		num_ls_threads = 25;
		num_ps_stack_entries = 85;
		num_vs_stack_entries = 85;
		num_gs_stack_entries = 85;
		num_es_stack_entries = 85;
		num_hs_stack_entries = 85;
		num_ls_stack_entries = 85;
		break;
	case CHIP_BARTS:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 20;
		num_gs_threads = 20;
		num_es_threads = 20;
		num_hs_threads = 20;
		num_ls_threads = 20;
		num_ps_stack_entries = 85;
		num_vs_stack_entries = 85;
		num_gs_stack_entries = 85;
		num_es_stack_entries = 85;
		num_hs_stack_entries = 85;
		num_ls_stack_entries = 85;
		break;
	case CHIP_TURKS:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 20;
		num_gs_threads = 20;
		num_es_threads = 20;
		num_hs_threads = 20;
		num_ls_threads = 20;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	case CHIP_CAICOS:
		num_ps_gprs = 93;
		num_vs_gprs = 46;
		num_temp_gprs = 4;
		num_gs_gprs = 31;
		num_es_gprs = 31;
		num_hs_gprs = 23;
		num_ls_gprs = 23;
		num_ps_threads = 128;
		num_vs_threads = 10;
		num_gs_threads = 10;
		num_es_threads = 10;
		num_hs_threads = 10;
		num_ls_threads = 10;
		num_ps_stack_entries = 42;
		num_vs_stack_entries = 42;
		num_gs_stack_entries = 42;
		num_es_stack_entries = 42;
		num_hs_stack_entries = 42;
		num_ls_stack_entries = 42;
		break;
	}

	tmp = 0x00000000;
	switch (family) {
	case CHIP_CEDAR:
	case CHIP_PALM:
	case CHIP_SUMO:
	case CHIP_SUMO2:
	case CHIP_CAICOS:
		break;
	default:
		tmp |= S_008C00_VC_ENABLE(1);
		break;
	}
	tmp |= S_008C00_EXPORT_SRC_C(1);
	tmp |= S_008C00_CS_PRIO(cs_prio);
	tmp |= S_008C00_LS_PRIO(ls_prio);
	tmp |= S_008C00_HS_PRIO(hs_prio);
	tmp |= S_008C00_PS_PRIO(ps_prio);
	tmp |= S_008C00_VS_PRIO(vs_prio);
	tmp |= S_008C00_GS_PRIO(gs_prio);
	tmp |= S_008C00_ES_PRIO(es_prio);
	r600_pipe_state_add_reg(rstate, R_008C00_SQ_CONFIG, tmp, NULL, 0);

	/* enable dynamic GPR resource management */
	if (rctx->screen->info.drm_minor >= 7) {
		/* always set temp clauses */
		r600_pipe_state_add_reg(rstate, R_008C04_SQ_GPR_RESOURCE_MGMT_1,
					S_008C04_NUM_CLAUSE_TEMP_GPRS(num_temp_gprs), NULL, 0);
		r600_pipe_state_add_reg(rstate, R_008C10_SQ_GLOBAL_GPR_RESOURCE_MGMT_1, 0, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_008C14_SQ_GLOBAL_GPR_RESOURCE_MGMT_2, 0, NULL, 0);
		r600_pipe_state_add_reg(rstate, R_008D8C_SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, (1 << 8), NULL, 0);
		r600_pipe_state_add_reg(rstate, R_028838_SQ_DYN_GPR_RESOURCE_LIMIT_1,
					S_028838_PS_GPRS(0x1e) |
					S_028838_VS_GPRS(0x1e) |
					S_028838_GS_GPRS(0x1e) |
					S_028838_ES_GPRS(0x1e) |
					S_028838_HS_GPRS(0x1e) |
					S_028838_LS_GPRS(0x1e), NULL, 0); /* workaround for hw issues with dyn gpr - must set all limits to 240 instead of 0, 0x1e == 240 / 8*/
	} else {
		tmp = 0;
		tmp |= S_008C04_NUM_PS_GPRS(num_ps_gprs);
		tmp |= S_008C04_NUM_VS_GPRS(num_vs_gprs);
		tmp |= S_008C04_NUM_CLAUSE_TEMP_GPRS(num_temp_gprs);
		r600_pipe_state_add_reg(rstate, R_008C04_SQ_GPR_RESOURCE_MGMT_1, tmp, NULL, 0);

		tmp = 0;
		tmp |= S_008C08_NUM_GS_GPRS(num_gs_gprs);
		tmp |= S_008C08_NUM_ES_GPRS(num_es_gprs);
		r600_pipe_state_add_reg(rstate, R_008C08_SQ_GPR_RESOURCE_MGMT_2, tmp, NULL, 0);

		tmp = 0;
		tmp |= S_008C0C_NUM_HS_GPRS(num_hs_gprs);
		tmp |= S_008C0C_NUM_HS_GPRS(num_ls_gprs);
		r600_pipe_state_add_reg(rstate, R_008C0C_SQ_GPR_RESOURCE_MGMT_3, tmp, NULL, 0);
	}

	tmp = 0;
	tmp |= S_008C18_NUM_PS_THREADS(num_ps_threads);
	tmp |= S_008C18_NUM_VS_THREADS(num_vs_threads);
	tmp |= S_008C18_NUM_GS_THREADS(num_gs_threads);
	tmp |= S_008C18_NUM_ES_THREADS(num_es_threads);
	r600_pipe_state_add_reg(rstate, R_008C18_SQ_THREAD_RESOURCE_MGMT_1, tmp, NULL, 0);

	tmp = 0;
	tmp |= S_008C1C_NUM_HS_THREADS(num_hs_threads);
	tmp |= S_008C1C_NUM_LS_THREADS(num_ls_threads);
	r600_pipe_state_add_reg(rstate, R_008C1C_SQ_THREAD_RESOURCE_MGMT_2, tmp, NULL, 0);

	tmp = 0;
	tmp |= S_008C20_NUM_PS_STACK_ENTRIES(num_ps_stack_entries);
	tmp |= S_008C20_NUM_VS_STACK_ENTRIES(num_vs_stack_entries);
	r600_pipe_state_add_reg(rstate, R_008C20_SQ_STACK_RESOURCE_MGMT_1, tmp, NULL, 0);

	tmp = 0;
	tmp |= S_008C24_NUM_GS_STACK_ENTRIES(num_gs_stack_entries);
	tmp |= S_008C24_NUM_ES_STACK_ENTRIES(num_es_stack_entries);
	r600_pipe_state_add_reg(rstate, R_008C24_SQ_STACK_RESOURCE_MGMT_2, tmp, NULL, 0);

	tmp = 0;
	tmp |= S_008C28_NUM_HS_STACK_ENTRIES(num_hs_stack_entries);
	tmp |= S_008C28_NUM_LS_STACK_ENTRIES(num_ls_stack_entries);
	r600_pipe_state_add_reg(rstate, R_008C28_SQ_STACK_RESOURCE_MGMT_3, tmp, NULL, 0);

	tmp = 0;
	tmp |= S_008E2C_NUM_PS_LDS(0x1000);
	tmp |= S_008E2C_NUM_LS_LDS(0x1000);
	r600_pipe_state_add_reg(rstate, R_008E2C_SQ_LDS_RESOURCE_MGMT, tmp, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_009100_SPI_CONFIG_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_00913C_SPI_CONFIG_CNTL_1, S_00913C_VTX_DONE_DELAY(4), NULL, 0);

#if 0
	r600_pipe_state_add_reg(rstate, R_028350_SX_MISC, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_008D8C_SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, 0x0, NULL, 0);
#endif
	r600_pipe_state_add_reg(rstate, R_028A4C_PA_SC_MODE_CNTL_1, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028900_SQ_ESGS_RING_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028904_SQ_GSVS_RING_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028908_SQ_ESTMP_RING_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02890C_SQ_GSTMP_RING_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028910_SQ_VSTMP_RING_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028914_SQ_PSTMP_RING_ITEMSIZE, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_02891C_SQ_GS_VERT_ITEMSIZE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028920_SQ_GS_VERT_ITEMSIZE_1, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028924_SQ_GS_VERT_ITEMSIZE_2, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028928_SQ_GS_VERT_ITEMSIZE_3, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028A10_VGT_OUTPUT_PATH_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A14_VGT_HOS_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A18_VGT_HOS_MAX_TESS_LEVEL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A1C_VGT_HOS_MIN_TESS_LEVEL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A20_VGT_HOS_REUSE_DEPTH, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A24_VGT_GROUP_PRIM_TYPE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A28_VGT_GROUP_FIRST_DECR, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A2C_VGT_GROUP_DECR, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A30_VGT_GROUP_VECT_0_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A34_VGT_GROUP_VECT_1_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A38_VGT_GROUP_VECT_0_FMT_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A3C_VGT_GROUP_VECT_1_FMT_CNTL, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028A40_VGT_GS_MODE, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028B94_VGT_STRMOUT_CONFIG, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028B98_VGT_STRMOUT_BUFFER_CONFIG, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AB4_VGT_REUSE_OFF, 0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028AB8_VGT_VTX_CNT_EN, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_008A14_PA_CL_ENHANCE, (3 << 1) | 1, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028380_SQ_VTX_SEMANTIC_0, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028384_SQ_VTX_SEMANTIC_1, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028388_SQ_VTX_SEMANTIC_2, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02838C_SQ_VTX_SEMANTIC_3, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028390_SQ_VTX_SEMANTIC_4, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028394_SQ_VTX_SEMANTIC_5, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_028398_SQ_VTX_SEMANTIC_6, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02839C_SQ_VTX_SEMANTIC_7, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A0_SQ_VTX_SEMANTIC_8, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A4_SQ_VTX_SEMANTIC_9, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283A8_SQ_VTX_SEMANTIC_10, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283AC_SQ_VTX_SEMANTIC_11, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B0_SQ_VTX_SEMANTIC_12, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B4_SQ_VTX_SEMANTIC_13, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283B8_SQ_VTX_SEMANTIC_14, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283BC_SQ_VTX_SEMANTIC_15, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C0_SQ_VTX_SEMANTIC_16, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C4_SQ_VTX_SEMANTIC_17, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283C8_SQ_VTX_SEMANTIC_18, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283CC_SQ_VTX_SEMANTIC_19, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D0_SQ_VTX_SEMANTIC_20, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D4_SQ_VTX_SEMANTIC_21, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283D8_SQ_VTX_SEMANTIC_22, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283DC_SQ_VTX_SEMANTIC_23, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E0_SQ_VTX_SEMANTIC_24, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E4_SQ_VTX_SEMANTIC_25, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283E8_SQ_VTX_SEMANTIC_26, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283EC_SQ_VTX_SEMANTIC_27, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F0_SQ_VTX_SEMANTIC_28, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F4_SQ_VTX_SEMANTIC_29, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283F8_SQ_VTX_SEMANTIC_30, 0x0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0283FC_SQ_VTX_SEMANTIC_31, 0x0, NULL, 0);

	r600_pipe_state_add_reg(rstate, R_028810_PA_CL_CLIP_CNTL, 0x0, NULL, 0);

	r600_context_pipe_state_set(rctx, rstate);
}

void evergreen_polygon_offset_update(struct r600_context *rctx)
{
	struct r600_pipe_state state;

	state.id = R600_PIPE_STATE_POLYGON_OFFSET;
	state.nregs = 0;
	if (rctx->rasterizer && rctx->framebuffer.zsbuf) {
		float offset_units = rctx->rasterizer->offset_units;
		unsigned offset_db_fmt_cntl = 0, depth;

		switch (rctx->framebuffer.zsbuf->texture->format) {
		case PIPE_FORMAT_Z24X8_UNORM:
		case PIPE_FORMAT_Z24_UNORM_S8_UINT:
			depth = -24;
			offset_units *= 2.0f;
			break;
		case PIPE_FORMAT_Z32_FLOAT:
		case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
			depth = -23;
			offset_units *= 1.0f;
			offset_db_fmt_cntl |= S_028B78_POLY_OFFSET_DB_IS_FLOAT_FMT(1);
			break;
		case PIPE_FORMAT_Z16_UNORM:
			depth = -16;
			offset_units *= 4.0f;
			break;
		default:
			return;
		}
		/* FIXME some of those reg can be computed with cso */
		offset_db_fmt_cntl |= S_028B78_POLY_OFFSET_NEG_NUM_DB_BITS(depth);
		r600_pipe_state_add_reg(&state,
				R_028B80_PA_SU_POLY_OFFSET_FRONT_SCALE,
				fui(rctx->rasterizer->offset_scale), NULL, 0);
		r600_pipe_state_add_reg(&state,
				R_028B84_PA_SU_POLY_OFFSET_FRONT_OFFSET,
				fui(offset_units), NULL, 0);
		r600_pipe_state_add_reg(&state,
				R_028B88_PA_SU_POLY_OFFSET_BACK_SCALE,
				fui(rctx->rasterizer->offset_scale), NULL, 0);
		r600_pipe_state_add_reg(&state,
				R_028B8C_PA_SU_POLY_OFFSET_BACK_OFFSET,
				fui(offset_units), NULL, 0);
		r600_pipe_state_add_reg(&state,
				R_028B78_PA_SU_POLY_OFFSET_DB_FMT_CNTL,
				offset_db_fmt_cntl, NULL, 0);
		r600_context_pipe_state_set(rctx, &state);
	}
}

void evergreen_pipe_shader_ps(struct pipe_context *ctx, struct r600_pipe_shader *shader)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = &shader->rstate;
	struct r600_shader *rshader = &shader->shader;
	unsigned i, exports_ps, num_cout, spi_ps_in_control_0, spi_input_z, spi_ps_in_control_1, db_shader_control;
	int pos_index = -1, face_index = -1;
	int ninterp = 0;
	boolean have_linear = FALSE, have_centroid = FALSE, have_perspective = FALSE;
	unsigned spi_baryc_cntl, sid, tmp, idx = 0;

	rstate->nregs = 0;

	db_shader_control = S_02880C_Z_ORDER(V_02880C_EARLY_Z_THEN_LATE_Z);
	for (i = 0; i < rshader->ninput; i++) {
		/* evergreen NUM_INTERP only contains values interpolated into the LDS,
		   POSITION goes via GPRs from the SC so isn't counted */
		if (rshader->input[i].name == TGSI_SEMANTIC_POSITION)
			pos_index = i;
		else if (rshader->input[i].name == TGSI_SEMANTIC_FACE)
			face_index = i;
		else {
			ninterp++;
			if (rshader->input[i].interpolate == TGSI_INTERPOLATE_LINEAR)
				have_linear = TRUE;
			if (rshader->input[i].interpolate == TGSI_INTERPOLATE_PERSPECTIVE)
				have_perspective = TRUE;
			if (rshader->input[i].centroid)
				have_centroid = TRUE;
		}

		sid = rshader->input[i].spi_sid;

		if (sid) {

			tmp = S_028644_SEMANTIC(sid);

			if (rshader->input[i].name == TGSI_SEMANTIC_POSITION ||
				rshader->input[i].interpolate == TGSI_INTERPOLATE_CONSTANT ||
				(rshader->input[i].interpolate == TGSI_INTERPOLATE_COLOR &&
					rctx->rasterizer && rctx->rasterizer->flatshade)) {
				tmp |= S_028644_FLAT_SHADE(1);
			}

			if (rshader->input[i].name == TGSI_SEMANTIC_GENERIC &&
					(rctx->sprite_coord_enable & (1 << rshader->input[i].sid))) {
				tmp |= S_028644_PT_SPRITE_TEX(1);
			}

			r600_pipe_state_add_reg(rstate, R_028644_SPI_PS_INPUT_CNTL_0 + idx * 4,
					tmp, NULL, 0);

			idx++;
		}
	}

	for (i = 0; i < rshader->noutput; i++) {
		if (rshader->output[i].name == TGSI_SEMANTIC_POSITION)
			db_shader_control |= S_02880C_Z_EXPORT_ENABLE(1);
		if (rshader->output[i].name == TGSI_SEMANTIC_STENCIL)
			db_shader_control |= S_02880C_STENCIL_EXPORT_ENABLE(1);
	}
	if (rshader->uses_kill)
		db_shader_control |= S_02880C_KILL_ENABLE(1);

	exports_ps = 0;
	num_cout = 0;
	for (i = 0; i < rshader->noutput; i++) {
		if (rshader->output[i].name == TGSI_SEMANTIC_POSITION ||
		    rshader->output[i].name == TGSI_SEMANTIC_STENCIL)
			exports_ps |= 1;
		else if (rshader->output[i].name == TGSI_SEMANTIC_COLOR) {
			if (rshader->fs_write_all)
				num_cout = rshader->nr_cbufs;
			else
				num_cout++;
		}
	}
	exports_ps |= S_02884C_EXPORT_COLORS(num_cout);
	if (!exports_ps) {
		/* always at least export 1 component per pixel */
		exports_ps = 2;
	}

	if (ninterp == 0) {
		ninterp = 1;
		have_perspective = TRUE;
	}

	if (!have_perspective && !have_linear)
		have_perspective = TRUE;

	spi_ps_in_control_0 = S_0286CC_NUM_INTERP(ninterp) |
		              S_0286CC_PERSP_GRADIENT_ENA(have_perspective) |
		              S_0286CC_LINEAR_GRADIENT_ENA(have_linear);
	spi_input_z = 0;
	if (pos_index != -1) {
		spi_ps_in_control_0 |=  S_0286CC_POSITION_ENA(1) |
			S_0286CC_POSITION_CENTROID(rshader->input[pos_index].centroid) |
			S_0286CC_POSITION_ADDR(rshader->input[pos_index].gpr);
		spi_input_z |= 1;
	}

	spi_ps_in_control_1 = 0;
	if (face_index != -1) {
		spi_ps_in_control_1 |= S_0286D0_FRONT_FACE_ENA(1) |
			S_0286D0_FRONT_FACE_ADDR(rshader->input[face_index].gpr);
	}

	spi_baryc_cntl = 0;
	if (have_perspective)
		spi_baryc_cntl |= S_0286E0_PERSP_CENTER_ENA(1) |
				  S_0286E0_PERSP_CENTROID_ENA(have_centroid);
	if (have_linear)
		spi_baryc_cntl |= S_0286E0_LINEAR_CENTER_ENA(1) |
				  S_0286E0_LINEAR_CENTROID_ENA(have_centroid);

	r600_pipe_state_add_reg(rstate, R_0286CC_SPI_PS_IN_CONTROL_0,
				spi_ps_in_control_0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0286D0_SPI_PS_IN_CONTROL_1,
				spi_ps_in_control_1, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0286E4_SPI_PS_IN_CONTROL_2,
				0, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0286D8_SPI_INPUT_Z, spi_input_z, NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_0286E0_SPI_BARYC_CNTL,
				spi_baryc_cntl,
				NULL, 0);

	r600_pipe_state_add_reg(rstate,
				R_028840_SQ_PGM_START_PS,
				r600_resource_va(ctx->screen, (void *)shader->bo) >> 8,
				shader->bo, RADEON_USAGE_READ);
	r600_pipe_state_add_reg(rstate,
				R_028844_SQ_PGM_RESOURCES_PS,
				S_028844_NUM_GPRS(rshader->bc.ngpr) |
				S_028844_PRIME_CACHE_ON_DRAW(1) |
				S_028844_STACK_SIZE(rshader->bc.nstack),
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028848_SQ_PGM_RESOURCES_2_PS,
				S_028848_SINGLE_ROUND(V_SQ_ROUND_TO_ZERO),
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_02884C_SQ_PGM_EXPORTS_PS,
				exports_ps, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_02880C_DB_SHADER_CONTROL,
				db_shader_control,
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_03A200_SQ_LOOP_CONST_0, 0x01000FFF,
				NULL, 0);

	shader->sprite_coord_enable = rctx->sprite_coord_enable;
	if (rctx->rasterizer)
		shader->flatshade = rctx->rasterizer->flatshade;
}

void evergreen_pipe_shader_vs(struct pipe_context *ctx, struct r600_pipe_shader *shader)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = &shader->rstate;
	struct r600_shader *rshader = &shader->shader;
	unsigned spi_vs_out_id[10] = {};
	unsigned i, tmp, nparams = 0;

	/* clear previous register */
	rstate->nregs = 0;

	for (i = 0; i < rshader->noutput; i++) {
		if (rshader->output[i].spi_sid) {
			tmp = rshader->output[i].spi_sid << ((nparams & 3) * 8);
			spi_vs_out_id[nparams / 4] |= tmp;
			nparams++;
		}
	}

	for (i = 0; i < 10; i++) {
		r600_pipe_state_add_reg(rstate,
					R_02861C_SPI_VS_OUT_ID_0 + i * 4,
					spi_vs_out_id[i], NULL, 0);
	}

	/* Certain attributes (position, psize, etc.) don't count as params.
	 * VS is required to export at least one param and r600_shader_from_tgsi()
	 * takes care of adding a dummy export.
	 */
	if (nparams < 1)
		nparams = 1;

	r600_pipe_state_add_reg(rstate,
			R_0286C4_SPI_VS_OUT_CONFIG,
			S_0286C4_VS_EXPORT_COUNT(nparams - 1),
			NULL, 0);
	r600_pipe_state_add_reg(rstate,
			R_028860_SQ_PGM_RESOURCES_VS,
			S_028860_NUM_GPRS(rshader->bc.ngpr) |
			S_028860_STACK_SIZE(rshader->bc.nstack),
			NULL, 0);
	r600_pipe_state_add_reg(rstate,
				R_028864_SQ_PGM_RESOURCES_2_VS,
				S_028864_SINGLE_ROUND(V_SQ_ROUND_TO_ZERO),
				NULL, 0);
	r600_pipe_state_add_reg(rstate,
			R_02885C_SQ_PGM_START_VS,
			r600_resource_va(ctx->screen, (void *)shader->bo) >> 8,
			shader->bo, RADEON_USAGE_READ);

	r600_pipe_state_add_reg(rstate,
				R_03A200_SQ_LOOP_CONST_0 + (32 * 4), 0x01000FFF,
				NULL, 0);

	shader->pa_cl_vs_out_cntl =
		S_02881C_VS_OUT_CCDIST0_VEC_ENA((rshader->clip_dist_write & 0x0F) != 0) |
		S_02881C_VS_OUT_CCDIST1_VEC_ENA((rshader->clip_dist_write & 0xF0) != 0) |
		S_02881C_VS_OUT_MISC_VEC_ENA(rshader->vs_out_misc_write) |
		S_02881C_USE_VTX_POINT_SIZE(rshader->vs_out_point_size);
}

void evergreen_fetch_shader(struct pipe_context *ctx,
			    struct r600_vertex_element *ve)
{
	struct r600_context *rctx = (struct r600_context *)ctx;
	struct r600_pipe_state *rstate = &ve->rstate;
	rstate->id = R600_PIPE_STATE_FETCH_SHADER;
	rstate->nregs = 0;
	r600_pipe_state_add_reg(rstate, R_0288A8_SQ_PGM_RESOURCES_FS,
				0x00000000, NULL, 0);
	r600_pipe_state_add_reg(rstate, R_0288A4_SQ_PGM_START_FS,
				r600_resource_va(ctx->screen, (void *)ve->fetch_shader) >> 8,
				ve->fetch_shader, RADEON_USAGE_READ);
}

void *evergreen_create_db_flush_dsa(struct r600_context *rctx)
{
	struct pipe_depth_stencil_alpha_state dsa;
	struct r600_pipe_state *rstate;

	memset(&dsa, 0, sizeof(dsa));

	rstate = rctx->context.create_depth_stencil_alpha_state(&rctx->context, &dsa);
	r600_pipe_state_add_reg(rstate,
				R_028000_DB_RENDER_CONTROL,
				S_028000_DEPTH_COPY_ENABLE(1) |
				S_028000_STENCIL_COPY_ENABLE(1) |
				S_028000_COPY_CENTROID(1),
				NULL, 0);
	return rstate;
}

void evergreen_pipe_init_buffer_resource(struct r600_context *rctx,
					 struct r600_pipe_resource_state *rstate)
{
	rstate->id = R600_PIPE_STATE_RESOURCE;

	rstate->val[0] = 0;
	rstate->bo[0] = NULL;
	rstate->val[1] = 0;
	rstate->val[2] = S_030008_ENDIAN_SWAP(r600_endian_swap(32));
	rstate->val[3] = S_03000C_DST_SEL_X(V_03000C_SQ_SEL_X) |
	  S_03000C_DST_SEL_Y(V_03000C_SQ_SEL_Y) |
	  S_03000C_DST_SEL_Z(V_03000C_SQ_SEL_Z) |
	  S_03000C_DST_SEL_W(V_03000C_SQ_SEL_W);
	rstate->val[4] = 0;
	rstate->val[5] = 0;
	rstate->val[6] = 0;
	rstate->val[7] = 0xc0000000;
}


void evergreen_pipe_mod_buffer_resource(struct pipe_context *ctx,
					struct r600_pipe_resource_state *rstate,
					struct r600_resource *rbuffer,
					unsigned offset, unsigned stride,
					enum radeon_bo_usage usage)
{
	uint64_t va;

	va = r600_resource_va(ctx->screen, (void *)rbuffer);
	rstate->bo[0] = rbuffer;
	rstate->bo_usage[0] = usage;
	rstate->val[0] = (offset + va) & 0xFFFFFFFFUL;
	rstate->val[1] = rbuffer->buf->size - offset - 1;
	rstate->val[2] = S_030008_ENDIAN_SWAP(r600_endian_swap(32)) |
			 S_030008_STRIDE(stride) |
			 (((va + offset) >> 32UL) & 0xFF);
}
