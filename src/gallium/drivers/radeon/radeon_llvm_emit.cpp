/*
 * Copyright 2011 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors: Tom Stellard <thomas.stellard@amd.com>
 *
 */
#include "radeon_llvm.h"

#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetMachine.h>

#include <llvm/Transforms/Scalar.h>

#include <llvm-c/Target.h>

#include <iostream>
#include <stdlib.h>
#include <stdio.h>

using namespace llvm;

#ifndef EXTERNAL_LLVM
#include "AMDGPUTargetMachine.h"
Target TheAMDGPUTarget;
#endif


/**
 * Compile an LLVM module to machine code.
 *
 * @param bytes This function allocates memory for the byte stream, it is the
 * caller's responsibility to free it.
 */
extern "C" unsigned
radeon_llvm_compile(LLVMModuleRef M, unsigned char ** bytes,
                 unsigned * byte_count, const char * gpu_family,
                 unsigned dump) {

#ifdef EXTERNAL_LLVM
   const Target * AMDGPUTarget = NULL;

   /* XXX: Can we just initialize the AMDGPU target here? */
   InitializeAllTargets();
   Triple::ArchType Arch = Triple::getArchTypeForLLVMName("amdisa");
   if (Arch == Triple::UnknownArch) {
      fprintf(stderr, "Unknown Arch\n");
   }
   std::string AMDGPUArchName = "amdisa";
   AMDGPUTriple.setArch(Arch);
   for (TargetRegistry::iterator it = TargetRegistry::begin(),
           ie = TargetRegistry::end(); it != ie; ++it) {
      if (it->getName() == AMDGPUArchName) {
         AMDGPUTarget = &*it;
         break;
      }
   }

   if(!AMDGPUTarget) {
      fprintf(stderr, "Can't find target\n");
      return 1;
   }
#else
   RegisterTargetMachine<AMDGPUTargetMachine> Y(TheAMDGPUTarget);
   RegisterMCAsmInfoFn A(TheAMDGPUTarget, createMCAsmInfo);
#endif

   Module * mod = unwrap(M);
#if HAVE_LLVM > 0x0300
   Triple AMDGPUTriple(sys::getDefaultTargetTriple());
#else
   Triple AMDGPUTriple(sys::getHostTriple());
#endif

   std::string FS = gpu_family;

#ifdef EXTERNAL_LLVM
   std::auto_ptr<TargetMachine> tm(AMDGPUTarget->createTargetMachine(
                     AMDGPUTriple.getTriple(), FS));
   TargetMachine &AMDGPUTargetMachine = *tm.get();
#else

#if HAVE_LLVM > 0x0300
   TargetOptions TO;
#endif
   AMDGPUTargetMachine * tm = new AMDGPUTargetMachine(TheAMDGPUTarget,
            AMDGPUTriple.getTriple(), gpu_family,
            "",
#if HAVE_LLVM > 0x0300
            TO,
#endif
            Reloc::Default, CodeModel::Default
#if HAVE_LLVM > 0x0300
            ,CodeGenOpt::Default
#endif
);
   TargetMachine &AMDGPUTargetMachine = *tm;
   /* XXX: Use TargetMachine.Options in 3.0 */
   if (dump) {
      mod->dump();
      tm->dumpCode();
   }
#endif
   const TargetData * AMDGPUData = AMDGPUTargetMachine.getTargetData();
   PassManager PM;
   PM.add(new TargetData(*AMDGPUData));
   PM.add(createPromoteMemoryToRegisterPass());
   AMDGPUTargetMachine.setAsmVerbosityDefault(true);

   std::string CodeString;
   raw_string_ostream oStream(CodeString);
   formatted_raw_ostream out(oStream);

   /* Optional extra paramater true / false to disable verify */
   if (AMDGPUTargetMachine.addPassesToEmitFile(PM, out, TargetMachine::CGFT_AssemblyFile,
#if HAVE_LLVM <= 0x300
                                               CodeGenOpt::Default,
#endif
                                               true)){
      fprintf(stderr, "AddingPasses failed.\n");
      return 1;
   }
   PM.run(*mod);

   out.flush();
   std::string &data = oStream.str();

   *bytes = (unsigned char*)malloc(data.length() * sizeof(unsigned char));
   memcpy(*bytes, data.c_str(), data.length() * sizeof(unsigned char));
   *byte_count = data.length();

#ifndef EXTERNAL_LLVM
   delete tm;
#endif

   return 0;
}
