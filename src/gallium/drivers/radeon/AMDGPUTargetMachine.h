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


#ifndef AMDGPU_TARGET_MACHINE_H
#define AMDGPU_TARGET_MACHINE_H

#include "AMDILTargetMachine.h"

#include "AMDGPUInstrInfo.h"
#include "AMDGPUISelLowering.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"
//#include "llvm/Target/TargetFrameInfo.h"

/*
#include "AMDGPU.h"
#include "AMDGPUFrameLowering.h"
#include "AMDGPUInstrInfo.h"
#include "AMDGPUISelLowering.h"
#include "AMDGPUSelectionDAGInfo.h"
#include "AMDGPUSubtarget.h"
*/
namespace llvm {

MCAsmInfo* createMCAsmInfo(const Target &T, StringRef TT);

class AMDGPUTargetMachine : public AMDILTargetMachine {
  AMDILSubtarget Subtarget;
/*   const TargetData DataLayout;
*/   AMDGPUTargetLowering TLInfo;
/*   AMDGPUSelectionDAGInfo TSInfo;
*/   OwningPtr<AMDGPUInstrInfo> InstrInfo;
//   AMDGPUFrameLowering FrameLowering;
     AMDILGlobalManager *mGM;
     AMDILKernelManager *mKM;
     bool mDump;

public:
   AMDGPUTargetMachine(const Target &T, StringRef TT, StringRef FS,
                       StringRef CPU,
#if LLVM_VERSION > 3000
                       TargetOptions Options,
#endif
                       Reloc::Model RM, CodeModel::Model CM
#if LLVM_VERSON > 3000
                       ,CodeGenOpt::Level OL
#endif
);
   ~AMDGPUTargetMachine();
   virtual const AMDGPUInstrInfo *getInstrInfo() const {return InstrInfo.get();}
/*
   virtual const TargetFrameLowering *getFrameLowering() const {
      return &FrameLowering;
   }
*/
   virtual const AMDILSubtarget *getSubtargetImpl() const {return &Subtarget; }
   virtual const AMDGPURegisterInfo *getRegisterInfo() const {
      return &InstrInfo->getRegisterInfo();
   }
   virtual AMDGPUTargetLowering * getTargetLowering() const {
      return const_cast<AMDGPUTargetLowering*>(&TLInfo);
   }
/*   virtual const AMDGPUSelectionDAGInfo* getSelectionDAGInfo() const {
      return &TSInfo;
   }
   virtual const TargetData *getTargetData() const { return &DataLayout; }
*/
   virtual bool addInstSelector(PassManagerBase &PM AMDIL_OPT_LEVEL_DECL);
   virtual bool addPreEmitPass(PassManagerBase &PM AMDIL_OPT_LEVEL_DECL);
   virtual bool addPreRegAlloc(PassManagerBase &PM AMDIL_OPT_LEVEL_DECL);
   virtual bool addPassesToEmitFile(PassManagerBase &PM,
                                    formatted_raw_ostream &Out,
                                    CodeGenFileType FileType,
                                    CodeGenOpt::Level OptLevel,
                                    bool DisableVerify);
public:
   void dumpCode() { mDump = true; }
   bool shouldDumpCode() const { return mDump; }
};

} /* End namespace llvm */

#endif /* AMDGPU_TARGET_MACHINE_H */
