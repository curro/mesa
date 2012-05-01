
#include "gallivm/lp_bld_tgsi_action.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_intr.h"
#include "gallivm/lp_bld_tgsi.h"
#include "radeon_llvm.h"
#include "radeon_llvm_emit.h"
#include "tgsi/tgsi_info.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_dump.h"

#include "radeonsi_pipe.h"
#include "radeonsi_shader.h"
#include "sid.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

/*
static ps_remap_inputs(
	struct tgsi_llvm_context * tl_ctx,
	unsigned tgsi_index,
	unsigned tgsi_chan)
{
	:
}

struct si_input
{
	struct list_head head;
	unsigned tgsi_index;
	unsigned tgsi_chan;
	unsigned order;
};
*/


struct si_shader_context
{
	struct radeon_llvm_context radeon_bld;
	struct r600_context *rctx;
	struct tgsi_parse_context parse;
	struct tgsi_token * tokens;
	struct si_pipe_shader *shader;
	unsigned type; /* TGSI_PROCESSOR_* specifies the type of shader. */
/*	unsigned num_inputs; */
/*	struct list_head inputs; */
/*	unsigned * input_mappings *//* From TGSI to SI hw */
/*	struct tgsi_shader_info info;*/
};

static struct si_shader_context * si_shader_context(
	struct lp_build_tgsi_context * bld_base)
{
	return (struct si_shader_context *)bld_base;
}


#define PERSPECTIVE_BASE 0
#define LINEAR_BASE 9

#define SAMPLE_OFFSET 0
#define CENTER_OFFSET 2
#define CENTROID_OFSET 4

#define USE_SGPR_MAX_SUFFIX_LEN 5

enum sgpr_type {
	SGPR_I32,
	SGPR_I64,
	SGPR_PTR_V4I32,
	SGPR_PTR_V8I32
};

static LLVMValueRef use_sgpr(
	struct gallivm_state * gallivm,
	enum sgpr_type type,
	unsigned sgpr)
{
	LLVMValueRef sgpr_index;
	LLVMValueRef sgpr_value;
	LLVMTypeRef ret_type;

	sgpr_index = lp_build_const_int32(gallivm, sgpr);

	if (type == SGPR_I32) {
		ret_type = LLVMInt32TypeInContext(gallivm->context);
		return lp_build_intrinsic_unary(gallivm->builder,
						"llvm.SI.use.sgpr.i32",
						ret_type, sgpr_index);
	}

	ret_type = LLVMInt64TypeInContext(gallivm->context);
	sgpr_value = lp_build_intrinsic_unary(gallivm->builder,
				"llvm.SI.use.sgpr.i64",
				 ret_type, sgpr_index);

	switch (type) {
	case SGPR_I64:
		return sgpr_value;
	case SGPR_PTR_V4I32:
		ret_type = LLVMInt32TypeInContext(gallivm->context);
		ret_type = LLVMVectorType(ret_type, 4);
		ret_type = LLVMPointerType(ret_type,
					0 /*XXX: Specify address space*/);
		return LLVMBuildIntToPtr(gallivm->builder, sgpr_value,
								ret_type, "");
	case SGPR_PTR_V8I32:
		ret_type = LLVMInt32TypeInContext(gallivm->context);
		ret_type = LLVMVectorType(ret_type, 8);
		ret_type = LLVMPointerType(ret_type,
					0 /*XXX: Specify address space*/);
		return LLVMBuildIntToPtr(gallivm->builder, sgpr_value,
								ret_type, "");
	default:
		assert(!"Unsupported SGPR type in use_sgpr()");
		return NULL;
	}
}

static void declare_input_vs(
	struct si_shader_context * si_shader_ctx,
	unsigned input_index,
	const struct tgsi_full_declaration *decl)
{
	LLVMValueRef t_list_ptr;
	LLVMValueRef t_offset;
	LLVMValueRef attribute_offset;
	LLVMValueRef buffer_index_reg;
	LLVMValueRef args[4];
	LLVMTypeRef vec4_type;
	LLVMValueRef input;
	struct lp_build_context * uint = &si_shader_ctx->radeon_bld.soa.bld_base.uint_bld;
	struct lp_build_context * base = &si_shader_ctx->radeon_bld.soa.bld_base.base;
	struct r600_context *rctx = si_shader_ctx->rctx;
	struct pipe_vertex_element *velem = &rctx->vertex_elements->elements[input_index];
	unsigned chan;

	/* XXX: Communicate with the rest of the driver about which SGPR the T#
	 * list pointer is going to be stored in.  Hard code to SGPR[6:7] for
 	 * now */
	t_list_ptr = use_sgpr(base->gallivm, SGPR_I64, 3);

	t_offset = lp_build_const_int32(base->gallivm,
					4 * velem->vertex_buffer_index);
	attribute_offset = lp_build_const_int32(base->gallivm, velem->src_offset);

	/* Load the buffer index is always, which is always stored in VGPR0
	 * for Vertex Shaders */
	buffer_index_reg = lp_build_intrinsic(base->gallivm->builder,
		"llvm.SI.vs.load.buffer.index", uint->elem_type, NULL, 0);

	vec4_type = LLVMVectorType(base->elem_type, 4);
	args[0] = t_list_ptr;
	args[1] = t_offset;
	args[2] = attribute_offset;
	args[3] = buffer_index_reg;
	input = lp_build_intrinsic(base->gallivm->builder,
		"llvm.SI.vs.load.input", vec4_type, args, 4);

	/* Break up the vec4 into individual components */
	for (chan = 0; chan < 4; chan++) {
		LLVMValueRef llvm_chan = lp_build_const_int32(base->gallivm, chan);
		/* XXX: Use a helper function for this.  There is one in
 		 * tgsi_llvm.c. */
		si_shader_ctx->radeon_bld.inputs[radeon_llvm_reg_index_soa(input_index, chan)] =
				LLVMBuildExtractElement(base->gallivm->builder,
				input, llvm_chan, "");
	}
}

static void declare_input_fs(
	struct si_shader_context * si_shader_ctx,
	unsigned input_index,
	const struct tgsi_full_declaration *decl)
{
	const char * intr_name;
	unsigned chan;
	struct lp_build_context * base =
				&si_shader_ctx->radeon_bld.soa.bld_base.base;
	struct gallivm_state * gallivm = base->gallivm;

	/* This value is:
	 * [15:0] NewPrimMask (Bit mask for each quad.  It is set it the
	 *                     quad begins a new primitive.  Bit 0 always needs
	 *                     to be unset)
	 * [32:16] ParamOffset
	 *
	 */
	/* XXX: This register number must be identical to the S_00B02C_USER_SGPR
	 * register field value
	 */
	LLVMValueRef params = use_sgpr(base->gallivm, SGPR_I32, 6);


	/* XXX: Is this the input_index? */
	LLVMValueRef attr_number = lp_build_const_int32(gallivm, input_index);

	/* XXX: Handle all possible interpolation modes */
	switch (decl->Interp.Interpolate) {
	case TGSI_INTERPOLATE_COLOR:
		if (si_shader_ctx->rctx->rasterizer->flatshade)
			intr_name = "llvm.SI.fs.interp.constant";
		else
			intr_name = "llvm.SI.fs.interp.linear.center";
		break;
	case TGSI_INTERPOLATE_CONSTANT:
		intr_name = "llvm.SI.fs.interp.constant";
		break;
	case TGSI_INTERPOLATE_LINEAR:
		intr_name = "llvm.SI.fs.interp.linear.center";
		break;
	default:
		fprintf(stderr, "Warning: Unhandled interpolation mode.\n");
		return;
	}

	/* XXX: Could there be more than TGSI_NUM_CHANNELS (4) ? */
	for (chan = 0; chan < TGSI_NUM_CHANNELS; chan++) {
		LLVMValueRef args[3];
		LLVMValueRef llvm_chan = lp_build_const_int32(gallivm, chan);
		unsigned soa_index = radeon_llvm_reg_index_soa(input_index, chan);
		LLVMTypeRef input_type = LLVMFloatTypeInContext(gallivm->context);
		args[0] = llvm_chan;
		args[1] = attr_number;
		args[2] = params;
		si_shader_ctx->radeon_bld.inputs[soa_index] =
			lp_build_intrinsic(gallivm->builder, intr_name,
						input_type, args, 3);
	}
}

static void declare_input(
	struct radeon_llvm_context * radeon_bld,
	unsigned input_index,
	const struct tgsi_full_declaration *decl)
{
	struct si_shader_context * si_shader_ctx =
				si_shader_context(&radeon_bld->soa.bld_base);
	if (si_shader_ctx->type == TGSI_PROCESSOR_VERTEX) {
		declare_input_vs(si_shader_ctx, input_index, decl);
	} else if (si_shader_ctx->type == TGSI_PROCESSOR_FRAGMENT) {
		declare_input_fs(si_shader_ctx, input_index, decl);
	} else {
		fprintf(stderr, "Warning: Unsupported shader type,\n");
	}
}

static LLVMValueRef fetch_constant(
	struct lp_build_tgsi_context * bld_base,
	const struct tgsi_full_src_register *reg,
	enum tgsi_opcode_type type,
	unsigned swizzle)
{
	struct lp_build_context * base = &bld_base->base;

	LLVMValueRef const_ptr;
	LLVMValueRef offset;

	/* XXX: Assume the pointer to the constant buffer is being stored in
	 * SGPR[0:1] */
	const_ptr = use_sgpr(base->gallivm, SGPR_I64, 0);

	/* XXX: This assumes that the constant buffer is not packed, so
	 * CONST[0].x will have an offset of 0 and CONST[1].x will have an
	 * offset of 4. */
	offset = lp_build_const_int32(base->gallivm,
					(reg->Register.Index * 4) + swizzle);

	return lp_build_intrinsic_binary(base->gallivm->builder,
		"llvm.SI.load.const", base->elem_type, const_ptr, offset);
}


/* Declare some intrinsics with the correct attributes */
static void si_llvm_emit_prologue(struct lp_build_tgsi_context * bld_base)
{
	LLVMValueRef function;
	struct gallivm_state * gallivm = bld_base->base.gallivm;

	LLVMTypeRef i64 = LLVMInt64TypeInContext(gallivm->context);
	LLVMTypeRef i32 = LLVMInt32TypeInContext(gallivm->context);

	/* declare i32 @llvm.SI.use.sgpr.i32(i32) */
	function = lp_declare_intrinsic(gallivm->module, "llvm.SI.use.sgpr.i32",
					i32, &i32, 1);
	LLVMAddFunctionAttr(function, LLVMReadNoneAttribute);

	/* declare i64 @llvm.SI.use.sgpr.i64(i32) */
	function = lp_declare_intrinsic(gallivm->module, "llvm.SI.use.sgpr.i64",
					i64, &i32, 1);
	LLVMAddFunctionAttr(function, LLVMReadNoneAttribute);
}

/* XXX: This is partially implemented for VS only at this point.  It is not complete */
static void si_llvm_emit_epilogue(struct lp_build_tgsi_context * bld_base)
{
	struct si_shader_context * si_shader_ctx = si_shader_context(bld_base);
	struct r600_shader * shader = &si_shader_ctx->shader->shader;
	struct lp_build_context * base = &bld_base->base;
	struct lp_build_context * uint =
				&si_shader_ctx->radeon_bld.soa.bld_base.uint_bld;
	struct tgsi_parse_context *parse = &si_shader_ctx->parse;
	LLVMValueRef last_args[9] = { 0 };

	while (!tgsi_parse_end_of_tokens(parse)) {
		/* XXX: component_bits controls which components of the output
		 * registers actually get exported. (e.g bit 0 means export
		 * X component, bit 1 means export Y component, etc.)  I'm
		 * hard coding this to 0xf for now.  In the future, we might
		 * want to do something else. */
		unsigned component_bits = 0xf;
		unsigned chan;
		struct tgsi_full_declaration *d =
					&parse->FullToken.FullDeclaration;
		LLVMValueRef args[9];
		unsigned target;
		unsigned index;
		unsigned color_count = 0;
		unsigned param_count = 0;
		int i;

		tgsi_parse_token(parse);
		if (parse->FullToken.Token.Type != TGSI_TOKEN_TYPE_DECLARATION)
			continue;

		switch (d->Declaration.File) {
		case TGSI_FILE_INPUT:
			i = shader->ninput++;
			shader->input[i].name = d->Semantic.Name;
			shader->input[i].sid = d->Semantic.Index;
			shader->input[i].interpolate = d->Interp.Interpolate;
			shader->input[i].centroid = d->Interp.Centroid;
			break;
		case TGSI_FILE_OUTPUT:
			i = shader->noutput++;
			shader->output[i].name = d->Semantic.Name;
			shader->output[i].sid = d->Semantic.Index;
			shader->output[i].interpolate = d->Interp.Interpolate;
			break;
		}

		if (d->Declaration.File != TGSI_FILE_OUTPUT)
			continue;

		for (index = d->Range.First; index <= d->Range.Last; index++) {
			for (chan = 0; chan < 4; chan++ ) {
				LLVMValueRef out_ptr =
					si_shader_ctx->radeon_bld.soa.outputs
					[index][chan];
				/* +5 because the first output value will be
				 * the 6th argument to the intrinsic. */
				args[chan + 5]= LLVMBuildLoad(
					base->gallivm->builder,	out_ptr, "");
			}

			/* XXX: We probably need to keep track of the output
			 * values, so we know what we are passing to the next
			 * stage. */

			/* Select the correct target */
			switch(d->Semantic.Name) {
			case TGSI_SEMANTIC_POSITION:
				target = V_008DFC_SQ_EXP_POS;
				break;
			case TGSI_SEMANTIC_COLOR:
				if (si_shader_ctx->type == TGSI_PROCESSOR_VERTEX) {
					target = V_008DFC_SQ_EXP_PARAM + param_count;
					param_count++;
				} else {
					target = V_008DFC_SQ_EXP_MRT + color_count;
					color_count++;
				}
				break;
			case TGSI_SEMANTIC_GENERIC:
				target = V_008DFC_SQ_EXP_PARAM + param_count;
				param_count++;
				break;
			default:
				target = 0;
				fprintf(stderr,
					"Warning: SI unhandled output type:%d\n",
					d->Semantic.Name);
			}

			/* Specify which components to enable */
			args[0] = lp_build_const_int32(base->gallivm,
								component_bits);

			/* Specify whether the EXEC mask represents the valid mask */
			args[1] = lp_build_const_int32(base->gallivm, 0);

			/* Specify whether this is the last export */
			args[2] = lp_build_const_int32(base->gallivm, 0);

			/* Specify the target we are exporting */
			args[3] = lp_build_const_int32(base->gallivm, target);

			/* Set COMPR flag to zero to export data as 32-bit */
			args[4] = uint->zero;

			if (si_shader_ctx->type == TGSI_PROCESSOR_VERTEX ?
			    (d->Semantic.Name == TGSI_SEMANTIC_POSITION) :
			    (d->Semantic.Name == TGSI_SEMANTIC_COLOR)) {
				if (last_args[0]) {
					lp_build_intrinsic(base->gallivm->builder,
							   "llvm.SI.export",
							   LLVMVoidTypeInContext(base->gallivm->context),
							   last_args, 9);
				}

				memcpy(last_args, args, sizeof(args));
			} else {
				lp_build_intrinsic(base->gallivm->builder,
						   "llvm.SI.export",
						   LLVMVoidTypeInContext(base->gallivm->context),
						   args, 9);
			}

		}
	}

	/* Specify whether the EXEC mask represents the valid mask */
	last_args[1] = lp_build_const_int32(base->gallivm,
					    si_shader_ctx->type == TGSI_PROCESSOR_FRAGMENT);

	/* Specify that this is the last export */
	last_args[2] = lp_build_const_int32(base->gallivm, 1);

	lp_build_intrinsic(base->gallivm->builder,
			   "llvm.SI.export",
			   LLVMVoidTypeInContext(base->gallivm->context),
			   last_args, 9);

/* XXX: Look up what this function does */
/*		ctx->shader->output[i].spi_sid = r600_spi_sid(&ctx->shader->output[i]);*/
}

static void tex_fetch_args(
	struct lp_build_tgsi_context * bld_base,
	struct lp_build_emit_data * emit_data)
{
	/* WriteMask */
	emit_data->args[0] = lp_build_const_int32(bld_base->base.gallivm,
				emit_data->inst->Dst[0].Register.WriteMask);

	/* Coordinates */
	/* XXX: Not all sample instructions need 4 address arguments. */
	emit_data->args[1] = lp_build_emit_fetch(bld_base, emit_data->inst,
							0, LP_CHAN_ALL);

	/* Resource */
	emit_data->args[2] = use_sgpr(bld_base->base.gallivm, SGPR_I64, 2);
	emit_data->args[3] = lp_build_const_int32(bld_base->base.gallivm,
						  8 * emit_data->inst->Src[1].Register.Index);

	/* Sampler */
	emit_data->args[4] = use_sgpr(bld_base->base.gallivm, SGPR_I64, 1);
	emit_data->args[5] = lp_build_const_int32(bld_base->base.gallivm,
						  4 * emit_data->inst->Src[1].Register.Index);

	/* Dimensions */
	/* XXX: We might want to pass this information to the shader at some. */
/*	emit_data->args[4] = lp_build_const_int32(bld_base->base.gallivm,
					emit_data->inst->Texture.Texture);
*/

	emit_data->arg_count = 6;
	/* XXX: To optimize, we could use a float or v2f32, if the last bits of
	 * the writemask are clear */
	emit_data->dst_type = LLVMVectorType(
			LLVMFloatTypeInContext(bld_base->base.gallivm->context),
			4);
}

static const struct lp_build_tgsi_action tex_action = {
	.fetch_args = tex_fetch_args,
	.emit = lp_build_tgsi_intrinsic,
	.intr_name = "llvm.SI.sample"
};


int si_pipe_shader_create(
	struct pipe_context *ctx,
	struct si_pipe_shader *shader)
{
	struct r600_context *rctx = (struct r600_context*)ctx;
	struct si_shader_context si_shader_ctx;
	struct tgsi_shader_info shader_info;
	struct lp_build_tgsi_context * bld_base;
	LLVMModuleRef mod;
	unsigned char * inst_bytes;
	unsigned inst_byte_count;
	unsigned i;

	radeon_llvm_context_init(&si_shader_ctx.radeon_bld);
	bld_base = &si_shader_ctx.radeon_bld.soa.bld_base;

	tgsi_scan_shader(shader->tokens, &shader_info);
	bld_base->info = &shader_info;
	bld_base->emit_fetch_funcs[TGSI_FILE_CONSTANT] = fetch_constant;
	bld_base->emit_prologue = si_llvm_emit_prologue;
	bld_base->emit_epilogue = si_llvm_emit_epilogue;

	bld_base->op_actions[TGSI_OPCODE_TEX] = tex_action;

	si_shader_ctx.radeon_bld.load_input = declare_input;
	si_shader_ctx.tokens = shader->tokens;
	tgsi_parse_init(&si_shader_ctx.parse, si_shader_ctx.tokens);
	si_shader_ctx.shader = shader;
	si_shader_ctx.type = si_shader_ctx.parse.FullHeader.Processor.Processor;
	si_shader_ctx.rctx = rctx;

	shader->shader.nr_cbufs = rctx->nr_cbufs;

	lp_build_tgsi_llvm(bld_base, shader->tokens);

	radeon_llvm_finalize_module(&si_shader_ctx.radeon_bld);

	mod = bld_base->base.gallivm->module;
	tgsi_dump(shader->tokens, 0);
	LLVMDumpModule(mod);
	radeon_llvm_compile(mod, &inst_bytes, &inst_byte_count, "SI", 1 /* dump */);
	fprintf(stderr, "SI CODE:\n");
	for (i = 0; i < inst_byte_count; i+=4 ) {
		fprintf(stderr, "%02x%02x%02x%02x\n", inst_bytes[i + 3],
			inst_bytes[i + 2], inst_bytes[i + 1],
			inst_bytes[i]);
	}

	shader->num_sgprs = util_le32_to_cpu(*(uint32_t*)inst_bytes);
	shader->num_vgprs = util_le32_to_cpu(*(uint32_t*)(inst_bytes + 4));
	shader->spi_ps_input_ena = util_le32_to_cpu(*(uint32_t*)(inst_bytes + 8));

	tgsi_parse_free(&si_shader_ctx.parse);

	/* copy new shader */
	if (shader->bo == NULL) {
		uint32_t *ptr;

		shader->bo = (struct r600_resource*)
			pipe_buffer_create(ctx->screen, PIPE_BIND_CUSTOM, PIPE_USAGE_IMMUTABLE, inst_byte_count);
		if (shader->bo == NULL) {
			return -ENOMEM;
		}
		ptr = (uint32_t*)rctx->ws->buffer_map(shader->bo->cs_buf, rctx->cs, PIPE_TRANSFER_WRITE);
		if (0 /*R600_BIG_ENDIAN*/) {
			for (i = 0; i < (inst_byte_count-12)/4; ++i) {
				ptr[i] = util_bswap32(*(uint32_t*)(inst_bytes+12 + i*4));
			}
		} else {
			memcpy(ptr, inst_bytes + 12, inst_byte_count - 12);
		}
		rctx->ws->buffer_unmap(shader->bo->cs_buf);
	}

	free(inst_bytes);

	return 0;
}

void si_pipe_shader_destroy(struct pipe_context *ctx, struct si_pipe_shader *shader)
{
	pipe_resource_reference((struct pipe_resource**)&shader->bo, NULL);

	memset(&shader->shader,0,sizeof(struct r600_shader));
}
