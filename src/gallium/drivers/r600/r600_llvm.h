
#ifndef R600_LLVM_H
#define R600_LLVM_H

#if HAVE_LLVM >= 0x300

#include <llvm-c/Core.h>

#include "gallivm/lp_bld_tgsi.h"

struct r600_shader_ctx;
struct radeon_llvm_context;
enum radeon_family;

LLVMModuleRef r600_tgsi_llvm(
	struct radeon_llvm_context * ctx,
	const struct tgsi_token * tokens);

const char * r600_llvm_gpu_string(enum radeon_family family);

unsigned r600_llvm_compile(
	LLVMModuleRef mod,
	unsigned char ** inst_bytes,
	unsigned * inst_byte_count,
	enum radeon_family family,
	unsigned dump);

#endif /* HAVE_LLVM >= 0x300 */

#endif /* R600_LLVM_H */
