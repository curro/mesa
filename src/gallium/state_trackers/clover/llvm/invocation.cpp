/*
 * Copyright 2012 Francisco Jerez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "llvm/invocation.hpp"

#include "core/base.hpp"

#include <CL/cl.h>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h> /* XXX */
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>

#include <llvm/Transforms/Scalar.h> /* XXX */
#include <llvm/Support/Host.h> /* XXX */
#include <llvm-c/Target.h> /* XXX */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdio>

static void
load_binary(const char *path, char **pbinary, size_t *sz) {
   std::ifstream s(path, std::ios::ate);
   *sz = s.tellg();
   *pbinary = new char[*sz];
   s.seekg(0);
   s.read(*pbinary, *sz);
}

llvm::Module *
clover::compile_program(const char *source, char **pbinary, size_t *binary_sz) {
   clang::CompilerInstance c;
/*XXX: Replace this with createfrontendBaseAction*/
#ifdef TGSI_BACKEND
   clang::EmitObjAction act(&llvm::getGlobalContext());
#else
   clang::EmitLLVMOnlyAction act(&llvm::getGlobalContext());
#endif
   std::string log;
   llvm::raw_string_ostream s_log(log);

/*XXX Add a "support LLVM cap" and then skip this for targets that have their
 * own LLVM backend */
#ifdef TGSI_BACKEND

   LLVMInitializeTGSITarget();
   LLVMInitializeTGSITargetInfo();
   LLVMInitializeTGSITargetMC();
   LLVMInitializeTGSIAsmPrinter();

#endif

   c.getFrontendOpts().Inputs.push_back(
      std::make_pair(clang::IK_OpenCL, "cl_input"));

#ifndef TGSI_BACKEND
   c.getFrontendOpts().ProgramAction = clang::frontend::EmitLLVMOnly;
#endif

   c.getHeaderSearchOpts().UseBuiltinIncludes = false;
#if HAVE_LLVM < 0x0300
   c.getHeaderSearchOpts().UseStandardIncludes = false;
#else
   c.getHeaderSearchOpts().UseStandardSystemIncludes = false;
#endif
   c.getLangOpts().NoBuiltin = true;
#ifdef TGSI_BACKEND
   c.getTargetOpts().Triple = "tgsi";
#else
   c.getTargetOpts().Triple = "r600";
#endif
   c.getInvocation().setLangDefaults(clang::IK_OpenCL);
   c.createDiagnostics(0, NULL, new clang::TextDiagnosticPrinter(
                          s_log, c.getDiagnosticOpts()));

   c.getPreprocessorOpts().addRemappedFile(
      "cl_input", llvm::MemoryBuffer::getMemBuffer(source));

   if (!c.ExecuteAction(act))
      throw error(CL_BUILD_PROGRAM_FAILURE, log);

   std::cerr << "build log: " << log << std::endl;

   return act.takeModule();
//   std::auto_ptr<llvm::Module> mod(act.takeModule());
//  mod->dump();
//   load_binary("cl_input.o", pbinary, binary_sz);
   // std::remove("cl_input.o");
}
