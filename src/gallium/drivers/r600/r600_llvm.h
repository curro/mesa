
#ifndef R600_LLVM_H
#define R600_LLVM_H

#include <llvm-c/Core.h>

#include "gallivm/lp_bld_tgsi.h"

struct r600_shader_ctx;
struct radeon_llvm_context;
enum radeon_family;

LLVMModuleRef r600_tgsi_llvm(
	struct radeon_llvm_context * ctx,
	const struct tgsi_token * tokens);

unsigned r600_llvm_compile(
	LLVMModuleRef mod,
	unsigned char ** inst_bytes,
	unsigned * inst_byte_count,
	enum radeon_family family,
	unsigned dump);


#endif /* R600_LLVM_H */
