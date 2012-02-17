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
 *
 * Authors:
 *      Jerome Glisse
 */
#include "r600_hw_context_priv.h"
#include "r600_pipe.h"
#include "r600d.h"
#include "util/u_memory.h"
#include <errno.h>

#define GROUP_FORCE_NEW_BLOCK	0

/* Get backends mask */
void r600_get_backend_mask(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	struct r600_resource *buffer;
	uint32_t *results;
	unsigned num_backends = ctx->screen->info.r600_num_backends;
	unsigned i, mask = 0;

	/* if backend_map query is supported by the kernel */
	if (ctx->screen->info.r600_backend_map_valid) {
		unsigned num_tile_pipes = ctx->screen->info.r600_num_tile_pipes;
		unsigned backend_map = ctx->screen->info.r600_backend_map;
		unsigned item_width, item_mask;

		if (ctx->chip_class >= EVERGREEN) {
			item_width = 4;
			item_mask = 0x7;
		} else {
			item_width = 2;
			item_mask = 0x3;
		}

		while(num_tile_pipes--) {
			i = backend_map & item_mask;
			mask |= (1<<i);
			backend_map >>= item_width;
		}
		if (mask != 0) {
			ctx->backend_mask = mask;
			return;
		}
	}

	/* otherwise backup path for older kernels */

	/* create buffer for event data */
	buffer = (struct r600_resource*)
		pipe_buffer_create(&ctx->screen->screen, PIPE_BIND_CUSTOM,
				   PIPE_USAGE_STAGING, ctx->max_db*16);
	if (!buffer)
		goto err;

	/* initialize buffer with zeroes */
	results = ctx->ws->buffer_map(buffer->buf, ctx->cs, PIPE_TRANSFER_WRITE);
	if (results) {
		memset(results, 0, ctx->max_db * 4 * 4);
		ctx->ws->buffer_unmap(buffer->buf);

		/* emit EVENT_WRITE for ZPASS_DONE */
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 2, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_ZPASS_DONE) | EVENT_INDEX(1);
		cs->buf[cs->cdw++] = 0;
		cs->buf[cs->cdw++] = 0;

		cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
		cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, buffer, RADEON_USAGE_WRITE);

		/* analyze results */
		results = ctx->ws->buffer_map(buffer->buf, ctx->cs, PIPE_TRANSFER_READ);
		if (results) {
			for(i = 0; i < ctx->max_db; i++) {
				/* at least highest bit will be set if backend is used */
				if (results[i*4 + 1])
					mask |= (1<<i);
			}
			ctx->ws->buffer_unmap(buffer->buf);
		}
	}

	pipe_resource_reference((struct pipe_resource**)&buffer, NULL);

	if (mask != 0) {
		ctx->backend_mask = mask;
		return;
	}

err:
	/* fallback to old method - set num_backends lower bits to 1 */
	ctx->backend_mask = (~((uint32_t)0))>>(32-num_backends);
	return;
}

static inline void r600_context_ps_partial_flush(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;

	if (!(ctx->flags & R600_CONTEXT_DRAW_PENDING))
		return;

	cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 0, 0);
	cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_PS_PARTIAL_FLUSH) | EVENT_INDEX(4);

	ctx->flags &= ~R600_CONTEXT_DRAW_PENDING;
}

void r600_init_cs(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;

	/* R6xx requires this packet at the start of each command buffer */
	if (ctx->family < CHIP_RV770) {
		cs->buf[cs->cdw++] = PKT3(PKT3_START_3D_CMDBUF, 0, 0);
		cs->buf[cs->cdw++] = 0x00000000;
	}
	/* All asics require this one */
	cs->buf[cs->cdw++] = PKT3(PKT3_CONTEXT_CONTROL, 1, 0);
	cs->buf[cs->cdw++] = 0x80000000;
	cs->buf[cs->cdw++] = 0x80000000;

	ctx->init_dwords = cs->cdw;
}

static void r600_init_block(struct r600_context *ctx,
			    struct r600_block *block,
			    const struct r600_reg *reg, int index, int nreg,
			    unsigned opcode, unsigned offset_base)
{
	int i = index;
	int j, n = nreg;

	/* initialize block */
	if (opcode == PKT3_SET_RESOURCE) {
		block->flags = BLOCK_FLAG_RESOURCE;
		block->status |= R600_BLOCK_STATUS_RESOURCE_DIRTY; /* dirty all blocks at start */
	} else {
		block->flags = 0;
		block->status |= R600_BLOCK_STATUS_DIRTY; /* dirty all blocks at start */
	}
	block->start_offset = reg[i].offset;
	block->pm4[block->pm4_ndwords++] = PKT3(opcode, n, 0);
	block->pm4[block->pm4_ndwords++] = (block->start_offset - offset_base) >> 2;
	block->reg = &block->pm4[block->pm4_ndwords];
	block->pm4_ndwords += n;
	block->nreg = n;
	block->nreg_dirty = n;
	LIST_INITHEAD(&block->list);
	LIST_INITHEAD(&block->enable_list);

	for (j = 0; j < n; j++) {
		if (reg[i+j].flags & REG_FLAG_DIRTY_ALWAYS) {
			block->flags |= REG_FLAG_DIRTY_ALWAYS;
		}
		if (reg[i+j].flags & REG_FLAG_ENABLE_ALWAYS) {
			if (!(block->status & R600_BLOCK_STATUS_ENABLED)) {
				block->status |= R600_BLOCK_STATUS_ENABLED;
				LIST_ADDTAIL(&block->enable_list, &ctx->enable_list);
				LIST_ADDTAIL(&block->list,&ctx->dirty);
			}
		}
		if (reg[i+j].flags & REG_FLAG_FLUSH_CHANGE) {
			block->flags |= REG_FLAG_FLUSH_CHANGE;
		}

		if (reg[i+j].flags & REG_FLAG_NEED_BO) {
			block->nbo++;
			assert(block->nbo < R600_BLOCK_MAX_BO);
			block->pm4_bo_index[j] = block->nbo;
			block->pm4[block->pm4_ndwords++] = PKT3(PKT3_NOP, 0, 0);
			block->pm4[block->pm4_ndwords++] = 0x00000000;
			block->reloc[block->nbo].bo_pm4_index = block->pm4_ndwords - 1;
		}
		if ((ctx->family > CHIP_R600) &&
		    (ctx->family < CHIP_RV770) && reg[i+j].flags & REG_FLAG_RV6XX_SBU) {
			block->pm4[block->pm4_ndwords++] = PKT3(PKT3_SURFACE_BASE_UPDATE, 0, 0);
			block->pm4[block->pm4_ndwords++] = reg[i+j].sbu_flags;
		}
	}
	/* check that we stay in limit */
	assert(block->pm4_ndwords < R600_BLOCK_MAX_REG);
}

int r600_context_add_block(struct r600_context *ctx, const struct r600_reg *reg, unsigned nreg,
			   unsigned opcode, unsigned offset_base)
{
	struct r600_block *block;
	struct r600_range *range;
	int offset;

	for (unsigned i = 0, n = 0; i < nreg; i += n) {
		/* ignore new block balise */
		if (reg[i].offset == GROUP_FORCE_NEW_BLOCK) {
			n = 1;
			continue;
		}

		/* ignore regs not on R600 on R600 */
		if ((reg[i].flags & REG_FLAG_NOT_R600) && ctx->family == CHIP_R600) {
			n = 1;
			continue;
		}

		/* register that need relocation are in their own group */
		/* find number of consecutive registers */
		n = 0;
		offset = reg[i].offset;
		while (reg[i + n].offset == offset) {
			n++;
			offset += 4;
			if ((n + i) >= nreg)
				break;
			if (n >= (R600_BLOCK_MAX_REG - 2))
				break;
		}

		/* allocate new block */
		block = calloc(1, sizeof(struct r600_block));
		if (block == NULL) {
			return -ENOMEM;
		}
		ctx->nblocks++;
		for (int j = 0; j < n; j++) {
			range = &ctx->range[CTX_RANGE_ID(reg[i + j].offset)];
			/* create block table if it doesn't exist */
			if (!range->blocks)
				range->blocks = calloc(1 << HASH_SHIFT, sizeof(void *));
			if (!range->blocks)
				return -1;

			range->blocks[CTX_BLOCK_ID(reg[i + j].offset)] = block;
		}

		r600_init_block(ctx, block, reg, i, n, opcode, offset_base);

	}
	return 0;
}

/* R600/R700 configuration */
static const struct r600_reg r600_config_reg_list[] = {
	{R_008958_VGT_PRIMITIVE_TYPE, 0, 0},
	{R_008C00_SQ_CONFIG, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008C04_SQ_GPR_RESOURCE_MGMT_1, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008C08_SQ_GPR_RESOURCE_MGMT_2, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008C0C_SQ_THREAD_RESOURCE_MGMT, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008C10_SQ_STACK_RESOURCE_MGMT_1, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008C14_SQ_STACK_RESOURCE_MGMT_2, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_008D8C_SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_009508_TA_CNTL_AUX, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_009714_VC_ENHANCE, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_009830_DB_DEBUG, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
	{R_009838_DB_WATERMARKS, REG_FLAG_ENABLE_ALWAYS | REG_FLAG_FLUSH_CHANGE, 0},
};

static const struct r600_reg r600_ctl_const_list[] = {
	{R_03CFF0_SQ_VTX_BASE_VTX_LOC, 0, 0},
	{R_03CFF4_SQ_VTX_START_INST_LOC, 0, 0},
};

static const struct r600_reg r600_context_reg_list[] = {
	{R_028350_SX_MISC, 0, 0},
	{R_0286C8_SPI_THREAD_GROUPING, 0, 0},
	{R_0288A8_SQ_ESGS_RING_ITEMSIZE, 0, 0},
	{R_0288AC_SQ_GSVS_RING_ITEMSIZE, 0, 0},
	{R_0288B0_SQ_ESTMP_RING_ITEMSIZE, 0, 0},
	{R_0288B4_SQ_GSTMP_RING_ITEMSIZE, 0, 0},
	{R_0288B8_SQ_VSTMP_RING_ITEMSIZE, 0, 0},
	{R_0288BC_SQ_PSTMP_RING_ITEMSIZE, 0, 0},
	{R_0288C0_SQ_FBUF_RING_ITEMSIZE, 0, 0},
	{R_0288C4_SQ_REDUC_RING_ITEMSIZE, 0, 0},
	{R_0288C8_SQ_GS_VERT_ITEMSIZE, 0, 0},
	{R_028A10_VGT_OUTPUT_PATH_CNTL, 0, 0},
	{R_028A14_VGT_HOS_CNTL, 0, 0},
	{R_028A18_VGT_HOS_MAX_TESS_LEVEL, 0, 0},
	{R_028A1C_VGT_HOS_MIN_TESS_LEVEL, 0, 0},
	{R_028A20_VGT_HOS_REUSE_DEPTH, 0, 0},
	{R_028A24_VGT_GROUP_PRIM_TYPE, 0, 0},
	{R_028A28_VGT_GROUP_FIRST_DECR, 0, 0},
	{R_028A2C_VGT_GROUP_DECR, 0, 0},
	{R_028A30_VGT_GROUP_VECT_0_CNTL, 0, 0},
	{R_028A34_VGT_GROUP_VECT_1_CNTL, 0, 0},
	{R_028A38_VGT_GROUP_VECT_0_FMT_CNTL, 0, 0},
	{R_028A3C_VGT_GROUP_VECT_1_FMT_CNTL, 0, 0},
	{R_028A40_VGT_GS_MODE, 0, 0},
	{R_028A4C_PA_SC_MODE_CNTL, 0, 0},
	{R_028AB0_VGT_STRMOUT_EN, 0, 0},
	{R_028AB4_VGT_REUSE_OFF, 0, 0},
	{R_028AB8_VGT_VTX_CNT_EN, 0, 0},
	{R_028B20_VGT_STRMOUT_BUFFER_EN, 0, 0},
	{R_028028_DB_STENCIL_CLEAR, 0, 0},
	{R_02802C_DB_DEPTH_CLEAR, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028040_CB_COLOR0_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(0)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280A0_CB_COLOR0_INFO, REG_FLAG_NEED_BO, 0},
	{R_028060_CB_COLOR0_SIZE, 0, 0},
	{R_028080_CB_COLOR0_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280E0_CB_COLOR0_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280C0_CB_COLOR0_TILE, REG_FLAG_NEED_BO, 0},
	{R_028100_CB_COLOR0_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028044_CB_COLOR1_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(1)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280A4_CB_COLOR1_INFO, REG_FLAG_NEED_BO, 0},
	{R_028064_CB_COLOR1_SIZE, 0, 0},
	{R_028084_CB_COLOR1_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280E4_CB_COLOR1_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280C4_CB_COLOR1_TILE, REG_FLAG_NEED_BO, 0},
	{R_028104_CB_COLOR1_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028048_CB_COLOR2_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(2)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280A8_CB_COLOR2_INFO, REG_FLAG_NEED_BO, 0},
	{R_028068_CB_COLOR2_SIZE, 0, 0},
	{R_028088_CB_COLOR2_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280E8_CB_COLOR2_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280C8_CB_COLOR2_TILE, REG_FLAG_NEED_BO, 0},
	{R_028108_CB_COLOR2_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_02804C_CB_COLOR3_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(3)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280AC_CB_COLOR3_INFO, REG_FLAG_NEED_BO, 0},
	{R_02806C_CB_COLOR3_SIZE, 0, 0},
	{R_02808C_CB_COLOR3_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280EC_CB_COLOR3_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280CC_CB_COLOR3_TILE, REG_FLAG_NEED_BO, 0},
	{R_02810C_CB_COLOR3_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028050_CB_COLOR4_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(4)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280B0_CB_COLOR4_INFO, REG_FLAG_NEED_BO, 0},
	{R_028070_CB_COLOR4_SIZE, 0, 0},
	{R_028090_CB_COLOR4_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280F0_CB_COLOR4_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280D0_CB_COLOR4_TILE, REG_FLAG_NEED_BO, 0},
	{R_028110_CB_COLOR4_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028054_CB_COLOR5_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(5)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280B4_CB_COLOR5_INFO, REG_FLAG_NEED_BO, 0},
	{R_028074_CB_COLOR5_SIZE, 0, 0},
	{R_028094_CB_COLOR5_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280F4_CB_COLOR5_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280D4_CB_COLOR5_TILE, REG_FLAG_NEED_BO, 0},
	{R_028114_CB_COLOR5_MASK, 0, 0},
	{R_028058_CB_COLOR6_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(6)},
	{R_0280B8_CB_COLOR6_INFO, REG_FLAG_NEED_BO, 0},
	{R_028078_CB_COLOR6_SIZE, 0, 0},
	{R_028098_CB_COLOR6_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280F8_CB_COLOR6_FRAG, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280D8_CB_COLOR6_TILE, REG_FLAG_NEED_BO, 0},
	{R_028118_CB_COLOR6_MASK, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_02805C_CB_COLOR7_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_COLOR(7)},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0280BC_CB_COLOR7_INFO, REG_FLAG_NEED_BO, 0},
	{R_02807C_CB_COLOR7_SIZE, 0, 0},
	{R_02809C_CB_COLOR7_VIEW, 0, 0},
	{R_0280FC_CB_COLOR7_FRAG, REG_FLAG_NEED_BO, 0},
	{R_0280DC_CB_COLOR7_TILE, REG_FLAG_NEED_BO, 0},
	{R_02811C_CB_COLOR7_MASK, 0, 0},
	{R_028120_CB_CLEAR_RED, 0, 0},
	{R_028124_CB_CLEAR_GREEN, 0, 0},
	{R_028128_CB_CLEAR_BLUE, 0, 0},
	{R_02812C_CB_CLEAR_ALPHA, 0, 0},
	{R_028140_ALU_CONST_BUFFER_SIZE_PS_0, REG_FLAG_DIRTY_ALWAYS, 0},
	{R_028144_ALU_CONST_BUFFER_SIZE_PS_1, REG_FLAG_DIRTY_ALWAYS, 0},
	{R_028180_ALU_CONST_BUFFER_SIZE_VS_0, REG_FLAG_DIRTY_ALWAYS, 0},
	{R_028184_ALU_CONST_BUFFER_SIZE_VS_1, REG_FLAG_DIRTY_ALWAYS, 0},
	{R_028940_ALU_CONST_CACHE_PS_0, REG_FLAG_NEED_BO, 0},
	{R_028944_ALU_CONST_CACHE_PS_1, REG_FLAG_NEED_BO, 0},
	{R_028980_ALU_CONST_CACHE_VS_0, REG_FLAG_NEED_BO, 0},
	{R_028984_ALU_CONST_CACHE_VS_1, REG_FLAG_NEED_BO, 0},
	{R_02823C_CB_SHADER_MASK, 0, 0},
	{R_028238_CB_TARGET_MASK, 0, 0},
	{R_028410_SX_ALPHA_TEST_CONTROL, 0, 0},
	{R_028414_CB_BLEND_RED, 0, 0},
	{R_028418_CB_BLEND_GREEN, 0, 0},
	{R_02841C_CB_BLEND_BLUE, 0, 0},
	{R_028420_CB_BLEND_ALPHA, 0, 0},
	{R_028424_CB_FOG_RED, 0, 0},
	{R_028428_CB_FOG_GREEN, 0, 0},
	{R_02842C_CB_FOG_BLUE, 0, 0},
	{R_028430_DB_STENCILREFMASK, 0, 0},
	{R_028434_DB_STENCILREFMASK_BF, 0, 0},
	{R_028438_SX_ALPHA_REF, 0, 0},
	{R_0286DC_SPI_FOG_CNTL, 0, 0},
	{R_0286E0_SPI_FOG_FUNC_SCALE, 0, 0},
	{R_0286E4_SPI_FOG_FUNC_BIAS, 0, 0},
	{R_028780_CB_BLEND0_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_028784_CB_BLEND1_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_028788_CB_BLEND2_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_02878C_CB_BLEND3_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_028790_CB_BLEND4_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_028794_CB_BLEND5_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_028798_CB_BLEND6_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_02879C_CB_BLEND7_CONTROL, REG_FLAG_NOT_R600, 0},
	{R_0287A0_CB_SHADER_CONTROL, 0, 0},
	{R_028800_DB_DEPTH_CONTROL, 0, 0},
	{R_028804_CB_BLEND_CONTROL, 0, 0},
	{R_028808_CB_COLOR_CONTROL, 0, 0},
	{R_02880C_DB_SHADER_CONTROL, 0, 0},
	{R_028C04_PA_SC_AA_CONFIG, 0, 0},
	{R_028C1C_PA_SC_AA_SAMPLE_LOCS_MCTX, 0, 0},
	{R_028C20_PA_SC_AA_SAMPLE_LOCS_8S_WD1_MCTX, 0, 0},
	{R_028C30_CB_CLRCMP_CONTROL, 0, 0},
	{R_028C34_CB_CLRCMP_SRC, 0, 0},
	{R_028C38_CB_CLRCMP_DST, 0, 0},
	{R_028C3C_CB_CLRCMP_MSK, 0, 0},
	{R_028C48_PA_SC_AA_MASK, 0, 0},
	{R_028D2C_DB_SRESULTS_COMPARE_STATE1, 0, 0},
	{R_028D44_DB_ALPHA_TO_MASK, 0, 0},
	{R_02800C_DB_DEPTH_BASE, REG_FLAG_NEED_BO|REG_FLAG_RV6XX_SBU, SURFACE_BASE_UPDATE_DEPTH},
	{R_028000_DB_DEPTH_SIZE, 0, 0},
	{R_028004_DB_DEPTH_VIEW, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028010_DB_DEPTH_INFO, REG_FLAG_NEED_BO, 0},
	{R_028D0C_DB_RENDER_CONTROL, 0, 0},
	{R_028D10_DB_RENDER_OVERRIDE, 0, 0},
	{R_028D24_DB_HTILE_SURFACE, 0, 0},
	{R_028D30_DB_PRELOAD_CONTROL, 0, 0},
	{R_028D34_DB_PREFETCH_LIMIT, 0, 0},
	{R_028030_PA_SC_SCREEN_SCISSOR_TL, 0, 0},
	{R_028034_PA_SC_SCREEN_SCISSOR_BR, 0, 0},
	{R_028200_PA_SC_WINDOW_OFFSET, 0, 0},
	{R_028204_PA_SC_WINDOW_SCISSOR_TL, 0, 0},
	{R_028208_PA_SC_WINDOW_SCISSOR_BR, 0, 0},
	{R_02820C_PA_SC_CLIPRECT_RULE, 0, 0},
	{R_028210_PA_SC_CLIPRECT_0_TL, 0, 0},
	{R_028214_PA_SC_CLIPRECT_0_BR, 0, 0},
	{R_028218_PA_SC_CLIPRECT_1_TL, 0, 0},
	{R_02821C_PA_SC_CLIPRECT_1_BR, 0, 0},
	{R_028220_PA_SC_CLIPRECT_2_TL, 0, 0},
	{R_028224_PA_SC_CLIPRECT_2_BR, 0, 0},
	{R_028228_PA_SC_CLIPRECT_3_TL, 0, 0},
	{R_02822C_PA_SC_CLIPRECT_3_BR, 0, 0},
	{R_028230_PA_SC_EDGERULE, 0, 0},
	{R_028240_PA_SC_GENERIC_SCISSOR_TL, 0, 0},
	{R_028244_PA_SC_GENERIC_SCISSOR_BR, 0, 0},
	{R_028250_PA_SC_VPORT_SCISSOR_0_TL, 0, 0},
	{R_028254_PA_SC_VPORT_SCISSOR_0_BR, 0, 0},
	{R_0282D0_PA_SC_VPORT_ZMIN_0, 0, 0},
	{R_0282D4_PA_SC_VPORT_ZMAX_0, 0, 0},
	{R_02843C_PA_CL_VPORT_XSCALE_0, 0, 0},
	{R_028440_PA_CL_VPORT_XOFFSET_0, 0, 0},
	{R_028444_PA_CL_VPORT_YSCALE_0, 0, 0},
	{R_028448_PA_CL_VPORT_YOFFSET_0, 0, 0},
	{R_02844C_PA_CL_VPORT_ZSCALE_0, 0, 0},
	{R_028450_PA_CL_VPORT_ZOFFSET_0, 0, 0},
	{R_0286D4_SPI_INTERP_CONTROL_0, 0, 0},
	{R_028810_PA_CL_CLIP_CNTL, 0, 0},
	{R_028814_PA_SU_SC_MODE_CNTL, 0, 0},
	{R_028818_PA_CL_VTE_CNTL, 0, 0},
	{R_02881C_PA_CL_VS_OUT_CNTL, 0, 0},
	{R_028820_PA_CL_NANINF_CNTL, 0, 0},
	{R_028A00_PA_SU_POINT_SIZE, 0, 0},
	{R_028A04_PA_SU_POINT_MINMAX, 0, 0},
	{R_028A08_PA_SU_LINE_CNTL, 0, 0},
	{R_028A0C_PA_SC_LINE_STIPPLE, 0, 0},
	{R_028A48_PA_SC_MPASS_PS_CNTL, 0, 0},
	{R_028C00_PA_SC_LINE_CNTL, 0, 0},
	{R_028C08_PA_SU_VTX_CNTL, 0, 0},
	{R_028C0C_PA_CL_GB_VERT_CLIP_ADJ, 0, 0},
	{R_028C10_PA_CL_GB_VERT_DISC_ADJ, 0, 0},
	{R_028C14_PA_CL_GB_HORZ_CLIP_ADJ, 0, 0},
	{R_028C18_PA_CL_GB_HORZ_DISC_ADJ, 0, 0},
	{R_028DF8_PA_SU_POLY_OFFSET_DB_FMT_CNTL, 0, 0},
	{R_028DFC_PA_SU_POLY_OFFSET_CLAMP, 0, 0},
	{R_028E00_PA_SU_POLY_OFFSET_FRONT_SCALE, 0, 0},
	{R_028E04_PA_SU_POLY_OFFSET_FRONT_OFFSET, 0, 0},
	{R_028E08_PA_SU_POLY_OFFSET_BACK_SCALE, 0, 0},
	{R_028E0C_PA_SU_POLY_OFFSET_BACK_OFFSET, 0, 0},
	{R_028E20_PA_CL_UCP0_X, 0, 0},
	{R_028E24_PA_CL_UCP0_Y, 0, 0},
	{R_028E28_PA_CL_UCP0_Z, 0, 0},
	{R_028E2C_PA_CL_UCP0_W, 0, 0},
	{R_028E30_PA_CL_UCP1_X, 0, 0},
	{R_028E34_PA_CL_UCP1_Y, 0, 0},
	{R_028E38_PA_CL_UCP1_Z, 0, 0},
	{R_028E3C_PA_CL_UCP1_W, 0, 0},
	{R_028E40_PA_CL_UCP2_X, 0, 0},
	{R_028E44_PA_CL_UCP2_Y, 0, 0},
	{R_028E48_PA_CL_UCP2_Z, 0, 0},
	{R_028E4C_PA_CL_UCP2_W, 0, 0},
	{R_028E50_PA_CL_UCP3_X, 0, 0},
	{R_028E54_PA_CL_UCP3_Y, 0, 0},
	{R_028E58_PA_CL_UCP3_Z, 0, 0},
	{R_028E5C_PA_CL_UCP3_W, 0, 0},
	{R_028E60_PA_CL_UCP4_X, 0, 0},
	{R_028E64_PA_CL_UCP4_Y, 0, 0},
	{R_028E68_PA_CL_UCP4_Z, 0, 0},
	{R_028E6C_PA_CL_UCP4_W, 0, 0},
	{R_028E70_PA_CL_UCP5_X, 0, 0},
	{R_028E74_PA_CL_UCP5_Y, 0, 0},
	{R_028E78_PA_CL_UCP5_Z, 0, 0},
	{R_028E7C_PA_CL_UCP5_W, 0, 0},
	{R_028380_SQ_VTX_SEMANTIC_0, 0, 0},
	{R_028384_SQ_VTX_SEMANTIC_1, 0, 0},
	{R_028388_SQ_VTX_SEMANTIC_2, 0, 0},
	{R_02838C_SQ_VTX_SEMANTIC_3, 0, 0},
	{R_028390_SQ_VTX_SEMANTIC_4, 0, 0},
	{R_028394_SQ_VTX_SEMANTIC_5, 0, 0},
	{R_028398_SQ_VTX_SEMANTIC_6, 0, 0},
	{R_02839C_SQ_VTX_SEMANTIC_7, 0, 0},
	{R_0283A0_SQ_VTX_SEMANTIC_8, 0, 0},
	{R_0283A4_SQ_VTX_SEMANTIC_9, 0, 0},
	{R_0283A8_SQ_VTX_SEMANTIC_10, 0, 0},
	{R_0283AC_SQ_VTX_SEMANTIC_11, 0, 0},
	{R_0283B0_SQ_VTX_SEMANTIC_12, 0, 0},
	{R_0283B4_SQ_VTX_SEMANTIC_13, 0, 0},
	{R_0283B8_SQ_VTX_SEMANTIC_14, 0, 0},
	{R_0283BC_SQ_VTX_SEMANTIC_15, 0, 0},
	{R_0283C0_SQ_VTX_SEMANTIC_16, 0, 0},
	{R_0283C4_SQ_VTX_SEMANTIC_17, 0, 0},
	{R_0283C8_SQ_VTX_SEMANTIC_18, 0, 0},
	{R_0283CC_SQ_VTX_SEMANTIC_19, 0, 0},
	{R_0283D0_SQ_VTX_SEMANTIC_20, 0, 0},
	{R_0283D4_SQ_VTX_SEMANTIC_21, 0, 0},
	{R_0283D8_SQ_VTX_SEMANTIC_22, 0, 0},
	{R_0283DC_SQ_VTX_SEMANTIC_23, 0, 0},
	{R_0283E0_SQ_VTX_SEMANTIC_24, 0, 0},
	{R_0283E4_SQ_VTX_SEMANTIC_25, 0, 0},
	{R_0283E8_SQ_VTX_SEMANTIC_26, 0, 0},
	{R_0283EC_SQ_VTX_SEMANTIC_27, 0, 0},
	{R_0283F0_SQ_VTX_SEMANTIC_28, 0, 0},
	{R_0283F4_SQ_VTX_SEMANTIC_29, 0, 0},
	{R_0283F8_SQ_VTX_SEMANTIC_30, 0, 0},
	{R_0283FC_SQ_VTX_SEMANTIC_31, 0, 0},
	{R_028614_SPI_VS_OUT_ID_0, 0, 0},
	{R_028618_SPI_VS_OUT_ID_1, 0, 0},
	{R_02861C_SPI_VS_OUT_ID_2, 0, 0},
	{R_028620_SPI_VS_OUT_ID_3, 0, 0},
	{R_028624_SPI_VS_OUT_ID_4, 0, 0},
	{R_028628_SPI_VS_OUT_ID_5, 0, 0},
	{R_02862C_SPI_VS_OUT_ID_6, 0, 0},
	{R_028630_SPI_VS_OUT_ID_7, 0, 0},
	{R_028634_SPI_VS_OUT_ID_8, 0, 0},
	{R_028638_SPI_VS_OUT_ID_9, 0, 0},
	{R_0286C4_SPI_VS_OUT_CONFIG, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028858_SQ_PGM_START_VS, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028868_SQ_PGM_RESOURCES_VS, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028894_SQ_PGM_START_FS, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_0288A4_SQ_PGM_RESOURCES_FS, 0, 0},
	{R_0288D0_SQ_PGM_CF_OFFSET_VS, 0, 0},
	{R_0288DC_SQ_PGM_CF_OFFSET_FS, 0, 0},
	{R_028644_SPI_PS_INPUT_CNTL_0, 0, 0},
	{R_028648_SPI_PS_INPUT_CNTL_1, 0, 0},
	{R_02864C_SPI_PS_INPUT_CNTL_2, 0, 0},
	{R_028650_SPI_PS_INPUT_CNTL_3, 0, 0},
	{R_028654_SPI_PS_INPUT_CNTL_4, 0, 0},
	{R_028658_SPI_PS_INPUT_CNTL_5, 0, 0},
	{R_02865C_SPI_PS_INPUT_CNTL_6, 0, 0},
	{R_028660_SPI_PS_INPUT_CNTL_7, 0, 0},
	{R_028664_SPI_PS_INPUT_CNTL_8, 0, 0},
	{R_028668_SPI_PS_INPUT_CNTL_9, 0, 0},
	{R_02866C_SPI_PS_INPUT_CNTL_10, 0, 0},
	{R_028670_SPI_PS_INPUT_CNTL_11, 0, 0},
	{R_028674_SPI_PS_INPUT_CNTL_12, 0, 0},
	{R_028678_SPI_PS_INPUT_CNTL_13, 0, 0},
	{R_02867C_SPI_PS_INPUT_CNTL_14, 0, 0},
	{R_028680_SPI_PS_INPUT_CNTL_15, 0, 0},
	{R_028684_SPI_PS_INPUT_CNTL_16, 0, 0},
	{R_028688_SPI_PS_INPUT_CNTL_17, 0, 0},
	{R_02868C_SPI_PS_INPUT_CNTL_18, 0, 0},
	{R_028690_SPI_PS_INPUT_CNTL_19, 0, 0},
	{R_028694_SPI_PS_INPUT_CNTL_20, 0, 0},
	{R_028698_SPI_PS_INPUT_CNTL_21, 0, 0},
	{R_02869C_SPI_PS_INPUT_CNTL_22, 0, 0},
	{R_0286A0_SPI_PS_INPUT_CNTL_23, 0, 0},
	{R_0286A4_SPI_PS_INPUT_CNTL_24, 0, 0},
	{R_0286A8_SPI_PS_INPUT_CNTL_25, 0, 0},
	{R_0286AC_SPI_PS_INPUT_CNTL_26, 0, 0},
	{R_0286B0_SPI_PS_INPUT_CNTL_27, 0, 0},
	{R_0286B4_SPI_PS_INPUT_CNTL_28, 0, 0},
	{R_0286B8_SPI_PS_INPUT_CNTL_29, 0, 0},
	{R_0286BC_SPI_PS_INPUT_CNTL_30, 0, 0},
	{R_0286C0_SPI_PS_INPUT_CNTL_31, 0, 0},
	{R_0286CC_SPI_PS_IN_CONTROL_0, 0, 0},
	{R_0286D0_SPI_PS_IN_CONTROL_1, 0, 0},
	{R_0286D8_SPI_INPUT_Z, 0, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028840_SQ_PGM_START_PS, REG_FLAG_NEED_BO, 0},
	{GROUP_FORCE_NEW_BLOCK, 0, 0},
	{R_028850_SQ_PGM_RESOURCES_PS, 0, 0},
	{R_028854_SQ_PGM_EXPORTS_PS, 0, 0},
	{R_0288CC_SQ_PGM_CF_OFFSET_PS, 0, 0},
	{R_028400_VGT_MAX_VTX_INDX, 0, 0},
	{R_028404_VGT_MIN_VTX_INDX, 0, 0},
	{R_028408_VGT_INDX_OFFSET, 0, 0},
	{R_02840C_VGT_MULTI_PRIM_IB_RESET_INDX, 0, 0},
	{R_028A84_VGT_PRIMITIVEID_EN, 0, 0},
	{R_028A94_VGT_MULTI_PRIM_IB_RESET_EN, 0, 0},
	{R_028AA0_VGT_INSTANCE_STEP_RATE_0, 0, 0},
	{R_028AA4_VGT_INSTANCE_STEP_RATE_1, 0, 0},
};

/* SHADER RESOURCE R600/R700 */
int r600_resource_init(struct r600_context *ctx, struct r600_range *range, unsigned offset, unsigned nblocks, unsigned stride, struct r600_reg *reg, int nreg, unsigned offset_base)
{
	int i;
	struct r600_block *block;
	range->blocks = calloc(nblocks, sizeof(struct r600_block *));
	if (range->blocks == NULL)
		return -ENOMEM;

	reg[0].offset += offset;
	for (i = 0; i < nblocks; i++) {
		block = calloc(1, sizeof(struct r600_block));
		if (block == NULL) {
			return -ENOMEM;
		}
		ctx->nblocks++;
		range->blocks[i] = block;
		r600_init_block(ctx, block, reg, 0, nreg, PKT3_SET_RESOURCE, offset_base);

		reg[0].offset += stride;
	}
	return 0;
}

      
static int r600_resource_range_init(struct r600_context *ctx, struct r600_range *range, unsigned offset, unsigned nblocks, unsigned stride)
{
	struct r600_reg r600_shader_resource[] = {
		{R_038000_RESOURCE0_WORD0, REG_FLAG_NEED_BO, 0},
		{R_038004_RESOURCE0_WORD1, REG_FLAG_NEED_BO, 0},
		{R_038008_RESOURCE0_WORD2, 0, 0},
		{R_03800C_RESOURCE0_WORD3, 0, 0},
		{R_038010_RESOURCE0_WORD4, 0, 0},
		{R_038014_RESOURCE0_WORD5, 0, 0},
		{R_038018_RESOURCE0_WORD6, 0, 0},
	};
	unsigned nreg = Elements(r600_shader_resource);

	return r600_resource_init(ctx, range, offset, nblocks, stride, r600_shader_resource, nreg, R600_RESOURCE_OFFSET);
}

/* SHADER SAMPLER R600/R700 */
static int r600_state_sampler_init(struct r600_context *ctx, uint32_t offset)
{
	struct r600_reg r600_shader_sampler[] = {
		{R_03C000_SQ_TEX_SAMPLER_WORD0_0, 0, 0},
		{R_03C004_SQ_TEX_SAMPLER_WORD1_0, 0, 0},
		{R_03C008_SQ_TEX_SAMPLER_WORD2_0, 0, 0},
	};
	unsigned nreg = Elements(r600_shader_sampler);

	for (int i = 0; i < nreg; i++) {
		r600_shader_sampler[i].offset += offset;
	}
	return r600_context_add_block(ctx, r600_shader_sampler, nreg, PKT3_SET_SAMPLER, R600_SAMPLER_OFFSET);
}

/* SHADER SAMPLER BORDER R600/R700 */
static int r600_state_sampler_border_init(struct r600_context *ctx, uint32_t offset)
{
	struct r600_reg r600_shader_sampler_border[] = {
		{R_00A400_TD_PS_SAMPLER0_BORDER_RED, 0, 0},
		{R_00A404_TD_PS_SAMPLER0_BORDER_GREEN, 0, 0},
		{R_00A408_TD_PS_SAMPLER0_BORDER_BLUE, 0, 0},
		{R_00A40C_TD_PS_SAMPLER0_BORDER_ALPHA, 0, 0},
	};
	unsigned nreg = Elements(r600_shader_sampler_border);

	for (int i = 0; i < nreg; i++) {
		r600_shader_sampler_border[i].offset += offset;
	}
	return r600_context_add_block(ctx, r600_shader_sampler_border, nreg, PKT3_SET_CONFIG_REG, R600_CONFIG_REG_OFFSET);
}

static int r600_loop_const_init(struct r600_context *ctx, uint32_t offset)
{
	unsigned nreg = 32;
	struct r600_reg r600_loop_consts[32];
	int i;

	for (i = 0; i < nreg; i++) {
		r600_loop_consts[i].offset = R600_LOOP_CONST_OFFSET + ((offset + i) * 4);
		r600_loop_consts[i].flags = REG_FLAG_DIRTY_ALWAYS;
		r600_loop_consts[i].sbu_flags = 0;
	}
	return r600_context_add_block(ctx, r600_loop_consts, nreg, PKT3_SET_LOOP_CONST, R600_LOOP_CONST_OFFSET);
}

static void r600_free_resource_range(struct r600_context *ctx, struct r600_range *range, int nblocks)
{
	struct r600_block *block;
	int i;
	for (i = 0; i < nblocks; i++) {
		block = range->blocks[i];
		if (block) {
			for (int k = 1; k <= block->nbo; k++)
				pipe_resource_reference((struct pipe_resource**)&block->reloc[k].bo, NULL);
			free(block);
		}
	}
	free(range->blocks);

}

/* initialize */
void r600_context_fini(struct r600_context *ctx)
{
	struct r600_block *block;
	struct r600_range *range;

	for (int i = 0; i < NUM_RANGES; i++) {
		if (!ctx->range[i].blocks)
			continue;
		for (int j = 0; j < (1 << HASH_SHIFT); j++) {
			block = ctx->range[i].blocks[j];
			if (block) {
				for (int k = 0, offset = block->start_offset; k < block->nreg; k++, offset += 4) {
					range = &ctx->range[CTX_RANGE_ID(offset)];
					range->blocks[CTX_BLOCK_ID(offset)] = NULL;
				}
				for (int k = 1; k <= block->nbo; k++) {
					pipe_resource_reference((struct pipe_resource**)&block->reloc[k].bo, NULL);
				}
				free(block);
			}
		}
		free(ctx->range[i].blocks);
	}
	r600_free_resource_range(ctx, &ctx->ps_resources, ctx->num_ps_resources);
	r600_free_resource_range(ctx, &ctx->vs_resources, ctx->num_vs_resources);
	r600_free_resource_range(ctx, &ctx->fs_resources, ctx->num_fs_resources);
	free(ctx->range);
	free(ctx->blocks);
	ctx->ws->cs_destroy(ctx->cs);
}

static void r600_add_resource_block(struct r600_context *ctx, struct r600_range *range, int num_blocks, int *index)
{
	int c = *index;
	for (int j = 0; j < num_blocks; j++) {
		if (!range->blocks[j])
			continue;

		ctx->blocks[c++] = range->blocks[j];
	}
	*index = c;
}

int r600_setup_block_table(struct r600_context *ctx)
{
	/* setup block table */
	int c = 0;
	ctx->blocks = calloc(ctx->nblocks, sizeof(void*));
	if (!ctx->blocks)
		return -ENOMEM;
	for (int i = 0; i < NUM_RANGES; i++) {
		if (!ctx->range[i].blocks)
			continue;
		for (int j = 0, add; j < (1 << HASH_SHIFT); j++) {
			if (!ctx->range[i].blocks[j])
				continue;

			add = 1;
			for (int k = 0; k < c; k++) {
				if (ctx->blocks[k] == ctx->range[i].blocks[j]) {
					add = 0;
					break;
				}
			}
			if (add) {
				assert(c < ctx->nblocks);
				ctx->blocks[c++] = ctx->range[i].blocks[j];
				j += (ctx->range[i].blocks[j]->nreg) - 1;
			}
		}
	}

	r600_add_resource_block(ctx, &ctx->ps_resources, ctx->num_ps_resources, &c);
	r600_add_resource_block(ctx, &ctx->vs_resources, ctx->num_vs_resources, &c);
	r600_add_resource_block(ctx, &ctx->fs_resources, ctx->num_fs_resources, &c);
	return 0;
}

int r600_context_init(struct r600_context *ctx)
{
	int r;

	LIST_INITHEAD(&ctx->active_query_list);

	/* init dirty list */
	LIST_INITHEAD(&ctx->dirty);
	LIST_INITHEAD(&ctx->resource_dirty);
	LIST_INITHEAD(&ctx->enable_list);

	ctx->range = calloc(NUM_RANGES, sizeof(struct r600_range));
	if (!ctx->range) {
		r = -ENOMEM;
		goto out_err;
	}

	/* add blocks */
	r = r600_context_add_block(ctx, r600_config_reg_list,
				   Elements(r600_config_reg_list), PKT3_SET_CONFIG_REG, R600_CONFIG_REG_OFFSET);
	if (r)
		goto out_err;
	r = r600_context_add_block(ctx, r600_context_reg_list,
				   Elements(r600_context_reg_list), PKT3_SET_CONTEXT_REG, R600_CONTEXT_REG_OFFSET);
	if (r)
		goto out_err;
	r = r600_context_add_block(ctx, r600_ctl_const_list,
				   Elements(r600_ctl_const_list), PKT3_SET_CTL_CONST, R600_CTL_CONST_OFFSET);
	if (r)
		goto out_err;

	/* PS SAMPLER BORDER */
	for (int j = 0, offset = 0; j < 18; j++, offset += 0x10) {
		r = r600_state_sampler_border_init(ctx, offset);
		if (r)
			goto out_err;
	}

	/* VS SAMPLER BORDER */
	for (int j = 0, offset = 0x200; j < 18; j++, offset += 0x10) {
		r = r600_state_sampler_border_init(ctx, offset);
		if (r)
			goto out_err;
	}
	/* PS SAMPLER */
	for (int j = 0, offset = 0; j < 18; j++, offset += 0xC) {
		r = r600_state_sampler_init(ctx, offset);
		if (r)
			goto out_err;
	}
	/* VS SAMPLER */
	for (int j = 0, offset = 0xD8; j < 18; j++, offset += 0xC) {
		r = r600_state_sampler_init(ctx, offset);
		if (r)
			goto out_err;
	}

	ctx->num_ps_resources = 160;
	ctx->num_vs_resources = 160;
	ctx->num_fs_resources = 16;
	r = r600_resource_range_init(ctx, &ctx->ps_resources, 0, 160, 0x1c);
	if (r)
		goto out_err;
	r = r600_resource_range_init(ctx, &ctx->vs_resources, 0x1180, 160, 0x1c);
	if (r)
		goto out_err;
	r = r600_resource_range_init(ctx, &ctx->fs_resources, 0x2300, 16, 0x1c);
	if (r)
		goto out_err;

	/* PS loop const */
	r600_loop_const_init(ctx, 0);
	/* VS loop const */
	r600_loop_const_init(ctx, 32);

	r = r600_setup_block_table(ctx);
	if (r)
		goto out_err;

	ctx->cs = ctx->ws->cs_create(ctx->ws);

	r600_init_cs(ctx);
	ctx->max_db = 4;
	return 0;
out_err:
	r600_context_fini(ctx);
	return r;
}

void r600_need_cs_space(struct r600_context *ctx, unsigned num_dw,
			boolean count_draw_in)
{
	struct r600_atom *state;

	/* The number of dwords we already used in the CS so far. */
	num_dw += ctx->cs->cdw;

	if (count_draw_in) {
		/* The number of dwords all the dirty states would take. */
		LIST_FOR_EACH_ENTRY(state, &ctx->dirty_states, head) {
			num_dw += state->num_dw;
		}

		num_dw += ctx->pm4_dirty_cdwords;

		/* The upper-bound of how much a draw command would take. */
		num_dw += R600_MAX_DRAW_CS_DWORDS;
	}

	/* Count in queries_suspend. */
	num_dw += ctx->num_cs_dw_queries_suspend;

	/* Count in streamout_end at the end of CS. */
	num_dw += ctx->num_cs_dw_streamout_end;

	/* Count in render_condition(NULL) at the end of CS. */
	if (ctx->predicate_drawing) {
		num_dw += 3;
	}

	/* Count in framebuffer cache flushes at the end of CS. */
	num_dw += 7; /* one SURFACE_SYNC and CACHE_FLUSH_AND_INV (r6xx-only) */

	/* Save 16 dwords for the fence mechanism. */
	num_dw += 16;

	/* Flush if there's not enough space. */
	if (num_dw > RADEON_MAX_CMDBUF_DWORDS) {
		r600_flush(&ctx->context, NULL, RADEON_FLUSH_ASYNC);
	}
}

void r600_context_dirty_block(struct r600_context *ctx,
			      struct r600_block *block,
			      int dirty, int index)
{
	if ((index + 1) > block->nreg_dirty)
		block->nreg_dirty = index + 1;

	if ((dirty != (block->status & R600_BLOCK_STATUS_DIRTY)) || !(block->status & R600_BLOCK_STATUS_ENABLED)) {
		block->status |= R600_BLOCK_STATUS_DIRTY;
		ctx->pm4_dirty_cdwords += block->pm4_ndwords;
		if (!(block->status & R600_BLOCK_STATUS_ENABLED)) {
			block->status |= R600_BLOCK_STATUS_ENABLED;
			LIST_ADDTAIL(&block->enable_list, &ctx->enable_list);
		}
		LIST_ADDTAIL(&block->list,&ctx->dirty);

		if (block->flags & REG_FLAG_FLUSH_CHANGE) {
			r600_context_ps_partial_flush(ctx);
		}
	}
}

void r600_context_pipe_state_set(struct r600_context *ctx, struct r600_pipe_state *state)
{
	struct r600_block *block;
	int dirty;
	for (int i = 0; i < state->nregs; i++) {
		unsigned id, reloc_id;
		struct r600_pipe_reg *reg = &state->regs[i];

		block = reg->block;
		id = reg->id;

		dirty = block->status & R600_BLOCK_STATUS_DIRTY;

		if (reg->value != block->reg[id]) {
			block->reg[id] = reg->value;
			dirty |= R600_BLOCK_STATUS_DIRTY;
		}
		if (block->flags & REG_FLAG_DIRTY_ALWAYS)
			dirty |= R600_BLOCK_STATUS_DIRTY;
		if (block->pm4_bo_index[id]) {
			/* find relocation */
			reloc_id = block->pm4_bo_index[id];
			pipe_resource_reference((struct pipe_resource**)&block->reloc[reloc_id].bo, &reg->bo->b.b.b);
			block->reloc[reloc_id].bo_usage = reg->bo_usage;
			/* always force dirty for relocs for now */
			dirty |= R600_BLOCK_STATUS_DIRTY;
		}

		if (dirty)
			r600_context_dirty_block(ctx, block, dirty, id);
	}
}

static void r600_context_dirty_resource_block(struct r600_context *ctx,
					      struct r600_block *block,
					      int dirty, int index)
{
	block->nreg_dirty = index + 1;

	if ((dirty != (block->status & R600_BLOCK_STATUS_RESOURCE_DIRTY)) || !(block->status & R600_BLOCK_STATUS_ENABLED)) {
		block->status |= R600_BLOCK_STATUS_RESOURCE_DIRTY;
		ctx->pm4_dirty_cdwords += block->pm4_ndwords;
		if (!(block->status & R600_BLOCK_STATUS_ENABLED)) {
			block->status |= R600_BLOCK_STATUS_ENABLED;
			LIST_ADDTAIL(&block->enable_list, &ctx->enable_list);
		}
		LIST_ADDTAIL(&block->list,&ctx->resource_dirty);
	}
}

void r600_context_pipe_state_set_resource(struct r600_context *ctx, struct r600_pipe_resource_state *state, struct r600_block *block)
{
	int dirty;
	int num_regs = ctx->chip_class >= EVERGREEN ? 8 : 7;
	boolean is_vertex;

	if (state == NULL) {
		block->status &= ~(R600_BLOCK_STATUS_ENABLED | R600_BLOCK_STATUS_RESOURCE_DIRTY);
		pipe_resource_reference((struct pipe_resource**)&block->reloc[1].bo, NULL);
		pipe_resource_reference((struct pipe_resource**)&block->reloc[2].bo, NULL);
		LIST_DELINIT(&block->list);
		LIST_DELINIT(&block->enable_list);
		return;
	}

	is_vertex = ((state->val[num_regs-1] & 0xc0000000) == 0xc0000000);
	dirty = block->status & R600_BLOCK_STATUS_RESOURCE_DIRTY;

	if (memcmp(block->reg, state->val, num_regs*4)) {
		memcpy(block->reg, state->val, num_regs * 4);
		dirty |= R600_BLOCK_STATUS_RESOURCE_DIRTY;
	}

	/* if no BOs on block, force dirty */
	if (!block->reloc[1].bo || !block->reloc[2].bo)
		dirty |= R600_BLOCK_STATUS_RESOURCE_DIRTY;

	if (!dirty) {
		if (is_vertex) {
			if (block->reloc[1].bo->buf != state->bo[0]->buf)
				dirty |= R600_BLOCK_STATUS_RESOURCE_DIRTY;
		} else {
			if ((block->reloc[1].bo->buf != state->bo[0]->buf) ||
			    (block->reloc[2].bo->buf != state->bo[1]->buf))
				dirty |= R600_BLOCK_STATUS_RESOURCE_DIRTY;
		}
	}

	if (dirty) {
		if (is_vertex) {
			/* VERTEX RESOURCE, we preted there is 2 bo to relocate so
			 * we have single case btw VERTEX & TEXTURE resource
			 */
			pipe_resource_reference((struct pipe_resource**)&block->reloc[1].bo, &state->bo[0]->b.b.b);
			block->reloc[1].bo_usage = state->bo_usage[0];
			pipe_resource_reference((struct pipe_resource**)&block->reloc[2].bo, NULL);
		} else {
			/* TEXTURE RESOURCE */
			pipe_resource_reference((struct pipe_resource**)&block->reloc[1].bo, &state->bo[0]->b.b.b);
			block->reloc[1].bo_usage = state->bo_usage[0];
			pipe_resource_reference((struct pipe_resource**)&block->reloc[2].bo, &state->bo[1]->b.b.b);
			block->reloc[2].bo_usage = state->bo_usage[1];
		}

		if (is_vertex)
			block->status |= R600_BLOCK_STATUS_RESOURCE_VERTEX;
		else
			block->status &= ~R600_BLOCK_STATUS_RESOURCE_VERTEX;
	
		r600_context_dirty_resource_block(ctx, block, dirty, num_regs - 1);
	}
}

void r600_context_pipe_state_set_ps_resource(struct r600_context *ctx, struct r600_pipe_resource_state *state, unsigned rid)
{
	struct r600_block *block = ctx->ps_resources.blocks[rid];

	r600_context_pipe_state_set_resource(ctx, state, block);
}

void r600_context_pipe_state_set_vs_resource(struct r600_context *ctx, struct r600_pipe_resource_state *state, unsigned rid)
{
	struct r600_block *block = ctx->vs_resources.blocks[rid];

	r600_context_pipe_state_set_resource(ctx, state, block);
}

void r600_context_pipe_state_set_fs_resource(struct r600_context *ctx, struct r600_pipe_resource_state *state, unsigned rid)
{
	struct r600_block *block = ctx->fs_resources.blocks[rid];

	r600_context_pipe_state_set_resource(ctx, state, block);
}

static inline void r600_context_pipe_state_set_sampler(struct r600_context *ctx, struct r600_pipe_state *state, unsigned offset)
{
	struct r600_range *range;
	struct r600_block *block;
	int i;
	int dirty;

	range = &ctx->range[CTX_RANGE_ID(offset)];
	block = range->blocks[CTX_BLOCK_ID(offset)];
	if (state == NULL) {
		block->status &= ~(R600_BLOCK_STATUS_ENABLED | R600_BLOCK_STATUS_DIRTY);
		LIST_DELINIT(&block->list);
		LIST_DELINIT(&block->enable_list);
		return;
	}
	dirty = block->status & R600_BLOCK_STATUS_DIRTY;
	for (i = 0; i < 3; i++) {
		if (block->reg[i] != state->regs[i].value) {
			block->reg[i] = state->regs[i].value;
			dirty |= R600_BLOCK_STATUS_DIRTY;
		}
	}

	if (dirty)
		r600_context_dirty_block(ctx, block, dirty, 2);
}


static inline void r600_context_pipe_state_set_sampler_border(struct r600_context *ctx, struct r600_pipe_state *state, unsigned offset)
{
	struct r600_range *range;
	struct r600_block *block;
	int i;
	int dirty;

	range = &ctx->range[CTX_RANGE_ID(offset)];
	block = range->blocks[CTX_BLOCK_ID(offset)];
	if (state == NULL) {
		block->status &= ~(R600_BLOCK_STATUS_ENABLED | R600_BLOCK_STATUS_DIRTY);
		LIST_DELINIT(&block->list);
		LIST_DELINIT(&block->enable_list);
		return;
	}
	if (state->nregs <= 3) {
		return;
	}
	dirty = block->status & R600_BLOCK_STATUS_DIRTY;
	for (i = 0; i < 4; i++) {
		if (block->reg[i] != state->regs[i + 3].value) {
			block->reg[i] = state->regs[i + 3].value;
			dirty |= R600_BLOCK_STATUS_DIRTY;
		}
	}

	/* We have to flush the shaders before we change the border color
	 * registers, or previous draw commands that haven't completed yet
	 * will end up using the new border color. */
	if (dirty & R600_BLOCK_STATUS_DIRTY)
		r600_context_ps_partial_flush(ctx);
	if (dirty)
		r600_context_dirty_block(ctx, block, dirty, 3);
}

void r600_context_pipe_state_set_ps_sampler(struct r600_context *ctx, struct r600_pipe_state *state, unsigned id)
{
	unsigned offset;

	offset = 0x0003C000 + id * 0xc;
	r600_context_pipe_state_set_sampler(ctx, state, offset);
	offset = 0x0000A400 + id * 0x10;
	r600_context_pipe_state_set_sampler_border(ctx, state, offset);
}

void r600_context_pipe_state_set_vs_sampler(struct r600_context *ctx, struct r600_pipe_state *state, unsigned id)
{
	unsigned offset;

	offset = 0x0003C0D8 + id * 0xc;
	r600_context_pipe_state_set_sampler(ctx, state, offset);
	offset = 0x0000A600 + id * 0x10;
	r600_context_pipe_state_set_sampler_border(ctx, state, offset);
}

struct r600_resource *r600_context_reg_bo(struct r600_context *ctx, unsigned offset)
{
	struct r600_range *range;
	struct r600_block *block;
	unsigned id;

	range = &ctx->range[CTX_RANGE_ID(offset)];
	block = range->blocks[CTX_BLOCK_ID(offset)];
	offset -= block->start_offset;
	id = block->pm4_bo_index[offset >> 2];
	if (block->reloc[id].bo) {
		return block->reloc[id].bo;
	}
	return NULL;
}

void r600_context_block_emit_dirty(struct r600_context *ctx, struct r600_block *block)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	int optional = block->nbo == 0 && !(block->flags & REG_FLAG_DIRTY_ALWAYS);
	int cp_dwords = block->pm4_ndwords, start_dword = 0;
	int new_dwords = 0;
	int nbo = block->nbo;

	if (block->nreg_dirty == 0 && optional) {
		goto out;
	}

	if (nbo) {
		ctx->flags |= R600_CONTEXT_CHECK_EVENT_FLUSH;

		for (int j = 0; j < block->nreg; j++) {
			if (block->pm4_bo_index[j]) {
				/* find relocation */
				struct r600_block_reloc *reloc = &block->reloc[block->pm4_bo_index[j]];
				if (reloc->bo) {
					block->pm4[reloc->bo_pm4_index] =
							r600_context_bo_reloc(ctx, reloc->bo, reloc->bo_usage);
				} else {
					block->pm4[reloc->bo_pm4_index] = 0;
				}
				nbo--;
				if (nbo == 0)
					break;

			}
		}
		ctx->flags &= ~R600_CONTEXT_CHECK_EVENT_FLUSH;
	}

	optional &= (block->nreg_dirty != block->nreg);
	if (optional) {
		new_dwords = block->nreg_dirty;
		start_dword = cs->cdw;
		cp_dwords = new_dwords + 2;
	}
	memcpy(&cs->buf[cs->cdw], block->pm4, cp_dwords * 4);
	cs->cdw += cp_dwords;

	if (optional) {
		uint32_t newword;

		newword = cs->buf[start_dword];
		newword &= PKT_COUNT_C;
		newword |= PKT_COUNT_S(new_dwords);
		cs->buf[start_dword] = newword;
	}
out:
	block->status ^= R600_BLOCK_STATUS_DIRTY;
	block->nreg_dirty = 0;
	LIST_DELINIT(&block->list);
}

void r600_context_block_resource_emit_dirty(struct r600_context *ctx, struct r600_block *block)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	int cp_dwords = block->pm4_ndwords;
	int nbo = block->nbo;

	ctx->flags |= R600_CONTEXT_CHECK_EVENT_FLUSH;

	if (block->status & R600_BLOCK_STATUS_RESOURCE_VERTEX) {
		nbo = 1;
		cp_dwords -= 2; /* don't copy the second NOP */
	}

	for (int j = 0; j < nbo; j++) {
		if (block->pm4_bo_index[j]) {
			/* find relocation */
			struct r600_block_reloc *reloc = &block->reloc[block->pm4_bo_index[j]];
			block->pm4[reloc->bo_pm4_index] =
				r600_context_bo_reloc(ctx, reloc->bo, reloc->bo_usage);
		}
	}
	ctx->flags &= ~R600_CONTEXT_CHECK_EVENT_FLUSH;

	memcpy(&cs->buf[cs->cdw], block->pm4, cp_dwords * 4);
	cs->cdw += cp_dwords;

	block->status ^= R600_BLOCK_STATUS_RESOURCE_DIRTY;
	block->nreg_dirty = 0;
	LIST_DELINIT(&block->list);
}

void r600_context_draw(struct r600_context *ctx, const struct r600_draw *draw)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	unsigned ndwords = 7;
	uint32_t *pm4;

	if (draw->indices) {
		ndwords = 11;
	}
	if (ctx->num_cs_dw_queries_suspend) {
		if (ctx->family >= CHIP_RV770)
			ndwords += 3;
		ndwords += 3;
	}

	/* when increasing ndwords, bump the max limit too */
	assert(ndwords <= R600_MAX_DRAW_CS_DWORDS);

	/* queries need some special values
	 * (this is non-zero if any query is active) */
	if (ctx->num_cs_dw_queries_suspend) {
		if (ctx->family >= CHIP_RV770) {
			pm4 = &cs->buf[cs->cdw];
			pm4[0] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
			pm4[1] = (R_028D0C_DB_RENDER_CONTROL - R600_CONTEXT_REG_OFFSET) >> 2;
			pm4[2] = draw->db_render_control | S_028D0C_R700_PERFECT_ZPASS_COUNTS(1);
			cs->cdw += 3;
			ndwords -= 3;
		}
		pm4 = &cs->buf[cs->cdw];
		pm4[0] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
		pm4[1] = (R_028D10_DB_RENDER_OVERRIDE - R600_CONTEXT_REG_OFFSET) >> 2;
		pm4[2] = draw->db_render_override | S_028D10_NOOP_CULL_DISABLE(1);
		cs->cdw += 3;
		ndwords -= 3;
	}

	/* draw packet */
	pm4 = &cs->buf[cs->cdw];
	pm4[0] = PKT3(PKT3_INDEX_TYPE, 0, ctx->predicate_drawing);
	pm4[1] = draw->vgt_index_type;
	pm4[2] = PKT3(PKT3_NUM_INSTANCES, 0, ctx->predicate_drawing);
	pm4[3] = draw->vgt_num_instances;
	if (draw->indices) {
		pm4[4] = PKT3(PKT3_DRAW_INDEX, 3, ctx->predicate_drawing);
		pm4[5] = draw->indices_bo_offset;
		pm4[6] = 0;
		pm4[7] = draw->vgt_num_indices;
		pm4[8] = draw->vgt_draw_initiator;
		pm4[9] = PKT3(PKT3_NOP, 0, ctx->predicate_drawing);
		pm4[10] = r600_context_bo_reloc(ctx, draw->indices, RADEON_USAGE_READ);
	} else {
		pm4[4] = PKT3(PKT3_DRAW_INDEX_AUTO, 1, ctx->predicate_drawing);
		pm4[5] = draw->vgt_num_indices;
		pm4[6] = draw->vgt_draw_initiator;
	}
	cs->cdw += ndwords;
}

void r600_inval_shader_cache(struct r600_context *ctx)
{
	ctx->atom_surface_sync.flush_flags |= S_0085F0_SH_ACTION_ENA(1);
	r600_atom_dirty(ctx, &ctx->atom_surface_sync.atom);
}

void r600_inval_texture_cache(struct r600_context *ctx)
{
	ctx->atom_surface_sync.flush_flags |= S_0085F0_TC_ACTION_ENA(1);
	r600_atom_dirty(ctx, &ctx->atom_surface_sync.atom);
}

void r600_inval_vertex_cache(struct r600_context *ctx)
{
	if (ctx->family == CHIP_RV610 ||
	    ctx->family == CHIP_RV620 ||
	    ctx->family == CHIP_RS780 ||
	    ctx->family == CHIP_RS880 ||
	    ctx->family == CHIP_RV710 ||
	    ctx->family == CHIP_CEDAR ||
	    ctx->family == CHIP_PALM ||
	    ctx->family == CHIP_SUMO ||
	    ctx->family == CHIP_SUMO2 ||
	    ctx->family == CHIP_CAICOS ||
	    ctx->family == CHIP_CAYMAN) {
		/* Some GPUs don't have the vertex cache and must use the texture cache instead. */
		ctx->atom_surface_sync.flush_flags |= S_0085F0_TC_ACTION_ENA(1);
	} else {
		ctx->atom_surface_sync.flush_flags |= S_0085F0_VC_ACTION_ENA(1);
	}
	r600_atom_dirty(ctx, &ctx->atom_surface_sync.atom);
}

void r600_flush_framebuffer(struct r600_context *ctx, bool flush_now)
{
	if (!(ctx->flags & R600_CONTEXT_DST_CACHES_DIRTY))
		return;

	ctx->atom_surface_sync.flush_flags |=
		r600_get_cb_flush_flags(ctx) |
		(ctx->framebuffer.zsbuf ? S_0085F0_DB_ACTION_ENA(1) | S_0085F0_DB_DEST_BASE_ENA(1) : 0);

	if (flush_now) {
		r600_emit_atom(ctx, &ctx->atom_surface_sync.atom);
	} else {
		r600_atom_dirty(ctx, &ctx->atom_surface_sync.atom);
	}

	/* Also add a complete cache flush to work around broken flushing on R6xx. */
	if (ctx->chip_class == R600) {
		if (flush_now) {
			r600_emit_atom(ctx, &ctx->atom_r6xx_flush_and_inv);
		} else {
			r600_atom_dirty(ctx, &ctx->atom_r6xx_flush_and_inv);
		}
	}

	ctx->flags &= ~R600_CONTEXT_DST_CACHES_DIRTY;
}

void r600_context_flush(struct r600_context *ctx, unsigned flags)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	struct r600_block *enable_block = NULL;
	bool queries_suspended = false;
	bool streamout_suspended = false;

	if (cs->cdw == ctx->init_dwords)
		return;

	/* suspend queries */
	if (ctx->num_cs_dw_queries_suspend) {
		r600_context_queries_suspend(ctx);
		queries_suspended = true;
	}

	if (ctx->num_cs_dw_streamout_end) {
		r600_context_streamout_end(ctx);
		streamout_suspended = true;
	}

	r600_flush_framebuffer(ctx, true);

	/* partial flush is needed to avoid lockups on some chips with user fences */
	cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 0, 0);
	cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_PS_PARTIAL_FLUSH) | EVENT_INDEX(4);

	/* Flush the CS. */
	ctx->ws->cs_flush(ctx->cs, flags);
	printf("flush cs: %i\n", ctx->cs->cdw);

	ctx->pm4_dirty_cdwords = 0;
	ctx->flags = 0;

	r600_init_cs(ctx);

	if (streamout_suspended) {
		ctx->streamout_start = TRUE;
		ctx->streamout_append_bitmask = ~0;
	}

	/* resume queries */
	if (queries_suspended) {
		r600_context_queries_resume(ctx);
	}

	/* set all valid group as dirty so they get reemited on
	 * next draw command
	 */
	LIST_FOR_EACH_ENTRY(enable_block, &ctx->enable_list, enable_list) {
		if (!(enable_block->flags & BLOCK_FLAG_RESOURCE)) {
			if(!(enable_block->status & R600_BLOCK_STATUS_DIRTY)) {
				LIST_ADDTAIL(&enable_block->list,&ctx->dirty);
				enable_block->status |= R600_BLOCK_STATUS_DIRTY;
			}
		} else {
			if(!(enable_block->status & R600_BLOCK_STATUS_RESOURCE_DIRTY)) {
				LIST_ADDTAIL(&enable_block->list,&ctx->resource_dirty);
				enable_block->status |= R600_BLOCK_STATUS_RESOURCE_DIRTY;
			}
		}
		ctx->pm4_dirty_cdwords += enable_block->pm4_ndwords;
		enable_block->nreg_dirty = enable_block->nreg;
	}
}

void r600_context_emit_fence(struct r600_context *ctx, struct r600_resource *fence_bo, unsigned offset, unsigned value)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	uint64_t va;

	r600_need_cs_space(ctx, 10, FALSE);

	va = r600_resource_va(&ctx->screen->screen, (void*)fence_bo);
	va = va + (offset << 2);

	cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 0, 0);
	cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_PS_PARTIAL_FLUSH) | EVENT_INDEX(4);
	cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE_EOP, 4, 0);
	cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5);
	cs->buf[cs->cdw++] = va & 0xFFFFFFFFUL;       /* ADDRESS_LO */
	/* DATA_SEL | INT_EN | ADDRESS_HI */
	cs->buf[cs->cdw++] = (1 << 29) | (0 << 24) | ((va >> 32UL) & 0xFF);
	cs->buf[cs->cdw++] = value;                   /* DATA_LO */
	cs->buf[cs->cdw++] = 0;                       /* DATA_HI */
	cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
	cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, fence_bo, RADEON_USAGE_WRITE);
}

static unsigned r600_query_read_result(char *map, unsigned start_index, unsigned end_index,
				       bool test_status_bit)
{
	uint32_t *current_result = (uint32_t*)map;
	uint64_t start, end;

	start = (uint64_t)current_result[start_index] |
		(uint64_t)current_result[start_index+1] << 32;
	end = (uint64_t)current_result[end_index] |
	      (uint64_t)current_result[end_index+1] << 32;

	if (!test_status_bit ||
	    ((start & 0x8000000000000000UL) && (end & 0x8000000000000000UL))) {
		return end - start;
	}
	return 0;
}

static boolean r600_query_result(struct r600_context *ctx, struct r600_query *query, boolean wait)
{
	unsigned results_base = query->results_start;
	char *map;

	map = ctx->ws->buffer_map(query->buffer->buf, ctx->cs,
				  PIPE_TRANSFER_READ |
				  (wait ? 0 : PIPE_TRANSFER_DONTBLOCK));
	if (!map)
		return FALSE;

	/* count all results across all data blocks */
	switch (query->type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
		while (results_base != query->results_end) {
			query->result.u64 +=
				r600_query_read_result(map + results_base, 0, 2, true);
			results_base = (results_base + 16) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		while (results_base != query->results_end) {
			query->result.b = query->result.b ||
				r600_query_read_result(map + results_base, 0, 2, true) != 0;
			results_base = (results_base + 16) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		while (results_base != query->results_end) {
			query->result.u64 +=
				r600_query_read_result(map + results_base, 0, 2, false);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_PRIMITIVES_EMITTED:
		/* SAMPLE_STREAMOUTSTATS stores this structure:
		 * {
		 *    u64 NumPrimitivesWritten;
		 *    u64 PrimitiveStorageNeeded;
		 * }
		 * We only need NumPrimitivesWritten here. */
		while (results_base != query->results_end) {
			query->result.u64 +=
				r600_query_read_result(map + results_base, 2, 6, true);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_PRIMITIVES_GENERATED:
		/* Here we read PrimitiveStorageNeeded. */
		while (results_base != query->results_end) {
			query->result.u64 +=
				r600_query_read_result(map + results_base, 0, 4, true);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_SO_STATISTICS:
		while (results_base != query->results_end) {
			query->result.so.num_primitives_written +=
				r600_query_read_result(map + results_base, 2, 6, true);
			query->result.so.primitives_storage_needed +=
				r600_query_read_result(map + results_base, 0, 4, true);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;
		}
		break;
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		while (results_base != query->results_end) {
			query->result.b = query->result.b ||
				r600_query_read_result(map + results_base, 2, 6, true) !=
				r600_query_read_result(map + results_base, 0, 4, true);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;
		}
		break;
	default:
		assert(0);
	}

	query->results_start = query->results_end;
	ctx->ws->buffer_unmap(query->buffer->buf);
	return TRUE;
}

void r600_query_begin(struct r600_context *ctx, struct r600_query *query)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	unsigned new_results_end, i;
	uint32_t *results;
	uint64_t va;

	r600_need_cs_space(ctx, query->num_cs_dw * 2, TRUE);

	new_results_end = (query->results_end + query->result_size) % query->buffer->b.b.b.width0;

	/* collect current results if query buffer is full */
	if (new_results_end == query->results_start) {
		r600_query_result(ctx, query, TRUE);
	}

	switch (query->type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		results = ctx->ws->buffer_map(query->buffer->buf, ctx->cs, PIPE_TRANSFER_WRITE);
		if (results) {
			results = (uint32_t*)((char*)results + query->results_end);
			memset(results, 0, query->result_size);

			/* Set top bits for unused backends */
			for (i = 0; i < ctx->max_db; i++) {
				if (!(ctx->backend_mask & (1<<i))) {
					results[(i * 4)+1] = 0x80000000;
					results[(i * 4)+3] = 0x80000000;
				}
			}
			ctx->ws->buffer_unmap(query->buffer->buf);
		}
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		break;
	case PIPE_QUERY_PRIMITIVES_EMITTED:
	case PIPE_QUERY_PRIMITIVES_GENERATED:
	case PIPE_QUERY_SO_STATISTICS:
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		results = ctx->ws->buffer_map(query->buffer->buf, ctx->cs, PIPE_TRANSFER_WRITE);
		results = (uint32_t*)((char*)results + query->results_end);
		memset(results, 0, query->result_size);
		ctx->ws->buffer_unmap(query->buffer->buf);
		break;
	default:
		assert(0);
	}

	/* emit begin query */
	va = r600_resource_va(&ctx->screen->screen, (void*)query->buffer);
	va += query->results_end;

	switch (query->type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 2, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_ZPASS_DONE) | EVENT_INDEX(1);
		cs->buf[cs->cdw++] = va;
		cs->buf[cs->cdw++] = (va >> 32UL) & 0xFF;
		break;
	case PIPE_QUERY_PRIMITIVES_EMITTED:
	case PIPE_QUERY_PRIMITIVES_GENERATED:
	case PIPE_QUERY_SO_STATISTICS:
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 2, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_SAMPLE_STREAMOUTSTATS) | EVENT_INDEX(3);
		cs->buf[cs->cdw++] = query->results_end;
		cs->buf[cs->cdw++] = 0;
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE_EOP, 4, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5);
		cs->buf[cs->cdw++] = va;
		cs->buf[cs->cdw++] = (3 << 29) | ((va >> 32UL) & 0xFF);
		cs->buf[cs->cdw++] = 0;
		cs->buf[cs->cdw++] = 0;
		break;
	default:
		assert(0);
	}
	cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
	cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, query->buffer, RADEON_USAGE_WRITE);

	ctx->num_cs_dw_queries_suspend += query->num_cs_dw;
}

void r600_query_end(struct r600_context *ctx, struct r600_query *query)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	uint64_t va;

	va = r600_resource_va(&ctx->screen->screen, (void*)query->buffer);
	/* emit end query */
	switch (query->type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		va += query->results_end + 8;
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 2, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_ZPASS_DONE) | EVENT_INDEX(1);
		cs->buf[cs->cdw++] = va;
		cs->buf[cs->cdw++] = (va >> 32UL) & 0xFF;
		break;
	case PIPE_QUERY_PRIMITIVES_EMITTED:
	case PIPE_QUERY_PRIMITIVES_GENERATED:
	case PIPE_QUERY_SO_STATISTICS:
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 2, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_SAMPLE_STREAMOUTSTATS) | EVENT_INDEX(3);
		cs->buf[cs->cdw++] = query->results_end + query->result_size/2;
		cs->buf[cs->cdw++] = 0;
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		va += query->results_end + query->result_size/2;
		cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE_EOP, 4, 0);
		cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5);
		cs->buf[cs->cdw++] = va;
		cs->buf[cs->cdw++] = (3 << 29) | ((va >> 32UL) & 0xFF);
		cs->buf[cs->cdw++] = 0;
		cs->buf[cs->cdw++] = 0;
		break;
	default:
		assert(0);
	}
	cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
	cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, query->buffer, RADEON_USAGE_WRITE);

	query->results_end = (query->results_end + query->result_size) % query->buffer->b.b.b.width0;
	ctx->num_cs_dw_queries_suspend -= query->num_cs_dw;
}

void r600_query_predication(struct r600_context *ctx, struct r600_query *query, int operation,
			    int flag_wait)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	uint64_t va;

	if (operation == PREDICATION_OP_CLEAR) {
		r600_need_cs_space(ctx, 3, FALSE);

		cs->buf[cs->cdw++] = PKT3(PKT3_SET_PREDICATION, 1, 0);
		cs->buf[cs->cdw++] = 0;
		cs->buf[cs->cdw++] = PRED_OP(PREDICATION_OP_CLEAR);
	} else {
		unsigned results_base = query->results_start;
		unsigned count;
		uint32_t op;

		/* find count of the query data blocks */
		count = (query->buffer->b.b.b.width0 + query->results_end - query->results_start) % query->buffer->b.b.b.width0;
		count /= query->result_size;

		r600_need_cs_space(ctx, 5 * count, TRUE);

		op = PRED_OP(operation) | PREDICATION_DRAW_VISIBLE |
				(flag_wait ? PREDICATION_HINT_WAIT : PREDICATION_HINT_NOWAIT_DRAW);
		va = r600_resource_va(&ctx->screen->screen, (void*)query->buffer);

		/* emit predicate packets for all data blocks */
		while (results_base != query->results_end) {
			cs->buf[cs->cdw++] = PKT3(PKT3_SET_PREDICATION, 1, 0);
			cs->buf[cs->cdw++] = (va + results_base) & 0xFFFFFFFFUL;
			cs->buf[cs->cdw++] = op | (((va + results_base) >> 32UL) & 0xFF);
			cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
			cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, query->buffer,
									     RADEON_USAGE_READ);
			results_base = (results_base + query->result_size) % query->buffer->b.b.b.width0;

			/* set CONTINUE bit for all packets except the first */
			op |= PREDICATION_CONTINUE;
		}
	}
}

struct r600_query *r600_context_query_create(struct r600_context *ctx, unsigned query_type)
{
	struct r600_query *query;
	unsigned buffer_size = 4096;

	query = CALLOC_STRUCT(r600_query);
	if (query == NULL)
		return NULL;

	query->type = query_type;

	switch (query_type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		query->result_size = 16 * ctx->max_db;
		query->num_cs_dw = 6;
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		query->result_size = 16;
		query->num_cs_dw = 8;
		break;
	case PIPE_QUERY_PRIMITIVES_EMITTED:
	case PIPE_QUERY_PRIMITIVES_GENERATED:
	case PIPE_QUERY_SO_STATISTICS:
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		/* NumPrimitivesWritten, PrimitiveStorageNeeded. */
		query->result_size = 32;
		query->num_cs_dw = 6;
		break;
	default:
		assert(0);
		FREE(query);
		return NULL;
	}

	/* adjust buffer size to simplify offsets wrapping math */
	buffer_size -= buffer_size % query->result_size;

	/* Queries are normally read by the CPU after
	 * being written by the gpu, hence staging is probably a good
	 * usage pattern.
	 */
	query->buffer = (struct r600_resource*)
		pipe_buffer_create(&ctx->screen->screen, PIPE_BIND_CUSTOM, PIPE_USAGE_STAGING, buffer_size);
	if (!query->buffer) {
		FREE(query);
		return NULL;
	}
	return query;
}

void r600_context_query_destroy(struct r600_context *ctx, struct r600_query *query)
{
	pipe_resource_reference((struct pipe_resource**)&query->buffer, NULL);
	free(query);
}

boolean r600_context_query_result(struct r600_context *ctx,
				struct r600_query *query,
				boolean wait, void *vresult)
{
	boolean *result_b = (boolean*)vresult;
	uint64_t *result_u64 = (uint64_t*)vresult;
	struct pipe_query_data_so_statistics *result_so =
		(struct pipe_query_data_so_statistics*)vresult;

	if (!r600_query_result(ctx, query, wait))
		return FALSE;

	switch (query->type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
	case PIPE_QUERY_PRIMITIVES_EMITTED:
	case PIPE_QUERY_PRIMITIVES_GENERATED:
		*result_u64 = query->result.u64;
		break;
	case PIPE_QUERY_OCCLUSION_PREDICATE:
	case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
		*result_b = query->result.b;
		break;
	case PIPE_QUERY_TIME_ELAPSED:
		*result_u64 = (1000000 * query->result.u64) / ctx->screen->info.r600_clock_crystal_freq;
		break;
	case PIPE_QUERY_SO_STATISTICS:
		*result_so = query->result.so;
		break;
	default:
		assert(0);
	}
	return TRUE;
}

void r600_context_queries_suspend(struct r600_context *ctx)
{
	struct r600_query *query;

	LIST_FOR_EACH_ENTRY(query, &ctx->active_query_list, list) {
		r600_query_end(ctx, query);
	}
	assert(ctx->num_cs_dw_queries_suspend == 0);
}

void r600_context_queries_resume(struct r600_context *ctx)
{
	struct r600_query *query;

	assert(ctx->num_cs_dw_queries_suspend == 0);

	LIST_FOR_EACH_ENTRY(query, &ctx->active_query_list, list) {
		r600_query_begin(ctx, query);
	}
}

static void r600_flush_vgt_streamout(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;

	cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONFIG_REG, 1, 0);
	cs->buf[cs->cdw++] = (R_008490_CP_STRMOUT_CNTL - R600_CONFIG_REG_OFFSET) >> 2;
	cs->buf[cs->cdw++] = 0;

	cs->buf[cs->cdw++] = PKT3(PKT3_EVENT_WRITE, 0, 0);
	cs->buf[cs->cdw++] = EVENT_TYPE(EVENT_TYPE_SO_VGTSTREAMOUT_FLUSH) | EVENT_INDEX(0);

	cs->buf[cs->cdw++] = PKT3(PKT3_WAIT_REG_MEM, 5, 0);
	cs->buf[cs->cdw++] = WAIT_REG_MEM_EQUAL; /* wait until the register is equal to the reference value */
	cs->buf[cs->cdw++] = R_008490_CP_STRMOUT_CNTL >> 2;  /* register */
	cs->buf[cs->cdw++] = 0;
	cs->buf[cs->cdw++] = S_008490_OFFSET_UPDATE_DONE(1); /* reference value */
	cs->buf[cs->cdw++] = S_008490_OFFSET_UPDATE_DONE(1); /* mask */
	cs->buf[cs->cdw++] = 4; /* poll interval */
}

static void r600_set_streamout_enable(struct r600_context *ctx, unsigned buffer_enable_bit)
{
	struct radeon_winsys_cs *cs = ctx->cs;

	if (buffer_enable_bit) {
		cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
		cs->buf[cs->cdw++] = (R_028AB0_VGT_STRMOUT_EN - R600_CONTEXT_REG_OFFSET) >> 2;
		cs->buf[cs->cdw++] = S_028AB0_STREAMOUT(1);

		cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
		cs->buf[cs->cdw++] = (R_028B20_VGT_STRMOUT_BUFFER_EN - R600_CONTEXT_REG_OFFSET) >> 2;
		cs->buf[cs->cdw++] = buffer_enable_bit;
	} else {
		cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
		cs->buf[cs->cdw++] = (R_028AB0_VGT_STRMOUT_EN - R600_CONTEXT_REG_OFFSET) >> 2;
		cs->buf[cs->cdw++] = S_028AB0_STREAMOUT(0);
	}
}

void r600_context_streamout_begin(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	struct r600_so_target **t = ctx->so_targets;
	unsigned *stride_in_dw = ctx->vs_so_stride_in_dw;
	unsigned buffer_en, i, update_flags = 0;
	uint64_t va;

	buffer_en = (ctx->num_so_targets >= 1 && t[0] ? 1 : 0) |
		    (ctx->num_so_targets >= 2 && t[1] ? 2 : 0) |
		    (ctx->num_so_targets >= 3 && t[2] ? 4 : 0) |
		    (ctx->num_so_targets >= 4 && t[3] ? 8 : 0);

	ctx->num_cs_dw_streamout_end =
		12 + /* flush_vgt_streamout */
		util_bitcount(buffer_en) * 8 +
		3;

	r600_need_cs_space(ctx,
			   12 + /* flush_vgt_streamout */
			   6 + /* enables */
			   util_bitcount(buffer_en & ctx->streamout_append_bitmask) * 8 +
			   util_bitcount(buffer_en & ~ctx->streamout_append_bitmask) * 6 +
			   (ctx->family > CHIP_R600 && ctx->family < CHIP_RV770 ? 2 : 0) +
			   ctx->num_cs_dw_streamout_end, TRUE);

	if (ctx->chip_class >= EVERGREEN) {
		evergreen_flush_vgt_streamout(ctx);
		evergreen_set_streamout_enable(ctx, buffer_en);
	} else {
		r600_flush_vgt_streamout(ctx);
		r600_set_streamout_enable(ctx, buffer_en);
	}

	for (i = 0; i < ctx->num_so_targets; i++) {
		if (t[i]) {
			t[i]->stride_in_dw = stride_in_dw[i];
			t[i]->so_index = i;
			va = r600_resource_va(&ctx->screen->screen,
					      (void*)t[i]->b.buffer);

			update_flags |= SURFACE_BASE_UPDATE_STRMOUT(i);

			cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 3, 0);
			cs->buf[cs->cdw++] = (R_028AD0_VGT_STRMOUT_BUFFER_SIZE_0 +
							16*i - R600_CONTEXT_REG_OFFSET) >> 2;
			cs->buf[cs->cdw++] = (t[i]->b.buffer_offset +
							t[i]->b.buffer_size) >> 2; /* BUFFER_SIZE (in DW) */
			cs->buf[cs->cdw++] = stride_in_dw[i];		   /* VTX_STRIDE (in DW) */
			cs->buf[cs->cdw++] = va >> 8;			   /* BUFFER_BASE */

			cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
			cs->buf[cs->cdw++] =
				r600_context_bo_reloc(ctx, r600_resource(t[i]->b.buffer),
						      RADEON_USAGE_WRITE);

			if (ctx->streamout_append_bitmask & (1 << i)) {
				va = r600_resource_va(&ctx->screen->screen,
						      (void*)t[i]->filled_size);
				/* Append. */
				cs->buf[cs->cdw++] = PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0);
				cs->buf[cs->cdw++] = STRMOUT_SELECT_BUFFER(i) |
							       STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_FROM_MEM); /* control */
				cs->buf[cs->cdw++] = 0; /* unused */
				cs->buf[cs->cdw++] = 0; /* unused */
				cs->buf[cs->cdw++] = va & 0xFFFFFFFFUL; /* src address lo */
				cs->buf[cs->cdw++] = (va >> 32UL) & 0xFFUL; /* src address hi */

				cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
				cs->buf[cs->cdw++] =
					r600_context_bo_reloc(ctx,  t[i]->filled_size,
							      RADEON_USAGE_READ);
			} else {
				/* Start from the beginning. */
				cs->buf[cs->cdw++] = PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0);
				cs->buf[cs->cdw++] = STRMOUT_SELECT_BUFFER(i) |
							       STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_FROM_PACKET); /* control */
				cs->buf[cs->cdw++] = 0; /* unused */
				cs->buf[cs->cdw++] = 0; /* unused */
				cs->buf[cs->cdw++] = t[i]->b.buffer_offset >> 2; /* buffer offset in DW */
				cs->buf[cs->cdw++] = 0; /* unused */
			}
		}
	}

	if (ctx->family > CHIP_R600 && ctx->family < CHIP_RV770) {
		cs->buf[cs->cdw++] = PKT3(PKT3_SURFACE_BASE_UPDATE, 0, 0);
		cs->buf[cs->cdw++] = update_flags;
	}
}

void r600_context_streamout_end(struct r600_context *ctx)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	struct r600_so_target **t = ctx->so_targets;
	unsigned i, flush_flags = 0;
	uint64_t va;

	if (ctx->chip_class >= EVERGREEN) {
		evergreen_flush_vgt_streamout(ctx);
	} else {
		r600_flush_vgt_streamout(ctx);
	}

	for (i = 0; i < ctx->num_so_targets; i++) {
		if (t[i]) {
			va = r600_resource_va(&ctx->screen->screen,
					      (void*)t[i]->filled_size);
			cs->buf[cs->cdw++] = PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0);
			cs->buf[cs->cdw++] = STRMOUT_SELECT_BUFFER(i) |
						       STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_NONE) |
						       STRMOUT_STORE_BUFFER_FILLED_SIZE; /* control */
			cs->buf[cs->cdw++] = va & 0xFFFFFFFFUL;     /* dst address lo */
			cs->buf[cs->cdw++] = (va >> 32UL) & 0xFFUL; /* dst address hi */
			cs->buf[cs->cdw++] = 0; /* unused */
			cs->buf[cs->cdw++] = 0; /* unused */

			cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
			cs->buf[cs->cdw++] =
				r600_context_bo_reloc(ctx,  t[i]->filled_size,
						      RADEON_USAGE_WRITE);

			flush_flags |= S_0085F0_SO0_DEST_BASE_ENA(1) << i;
		}
	}

	if (ctx->chip_class >= EVERGREEN) {
		evergreen_set_streamout_enable(ctx, 0);
	} else {
		r600_set_streamout_enable(ctx, 0);
	}

	if (ctx->chip_class < R700) {
		r600_atom_dirty(ctx, &ctx->atom_r6xx_flush_and_inv);
	} else {
		ctx->atom_surface_sync.flush_flags |= flush_flags;
		r600_atom_dirty(ctx, &ctx->atom_surface_sync.atom);
	}

	ctx->num_cs_dw_streamout_end = 0;

#if 0
	for (i = 0; i < ctx->num_so_targets; i++) {
		if (!t[i])
			continue;

		uint32_t *ptr = ctx->ws->buffer_map(t[i]->filled_size->buf, ctx->cs, RADEON_USAGE_READ);
		printf("FILLED_SIZE%i: %u\n", i, *ptr);
		ctx->ws->buffer_unmap(t[i]->filled_size->buf);
	}
#endif
}

void r600_context_draw_opaque_count(struct r600_context *ctx, struct r600_so_target *t)
{
	struct radeon_winsys_cs *cs = ctx->cs;
	uint64_t va = r600_resource_va(&ctx->screen->screen,
				       (void*)t->filled_size);

	r600_need_cs_space(ctx, 14 + 21, TRUE);

	cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
	cs->buf[cs->cdw++] = (R_028B28_VGT_STRMOUT_DRAW_OPAQUE_OFFSET - R600_CONTEXT_REG_OFFSET) >> 2;
	cs->buf[cs->cdw++] = 0;

	cs->buf[cs->cdw++] = PKT3(PKT3_SET_CONTEXT_REG, 1, 0);
	cs->buf[cs->cdw++] = (R_028B30_VGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE - R600_CONTEXT_REG_OFFSET) >> 2;
	cs->buf[cs->cdw++] = t->stride_in_dw;

	cs->buf[cs->cdw++] = PKT3(PKT3_COPY_DW, 4, 0);
	cs->buf[cs->cdw++] = COPY_DW_SRC_IS_MEM | COPY_DW_DST_IS_REG;
	cs->buf[cs->cdw++] = va & 0xFFFFFFFFUL;     /* src address lo */
	cs->buf[cs->cdw++] = (va >> 32UL) & 0xFFUL; /* src address hi */
	cs->buf[cs->cdw++] = R_028B2C_VGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE >> 2; /* dst register */
	cs->buf[cs->cdw++] = 0; /* unused */

	cs->buf[cs->cdw++] = PKT3(PKT3_NOP, 0, 0);
	cs->buf[cs->cdw++] = r600_context_bo_reloc(ctx, t->filled_size, RADEON_USAGE_READ);
}
