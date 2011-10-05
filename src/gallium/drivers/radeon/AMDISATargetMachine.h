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


#ifndef AMDISA_TARGET_MACHINE_H
#define AMDISA_TARGET_MACHINE_H

#include "AMDILTargetMachine.h"

#include "AMDISAInstrInfo.h"
#include "AMDISAISelLowering.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"
//#include "llvm/Target/TargetFrameInfo.h"

/*
#include "AMDISA.h"
#include "AMDISAFrameLowering.h"
#include "AMDISAInstrInfo.h"
#include "AMDISAISelLowering.h"
#include "AMDISASelectionDAGInfo.h"
#include "AMDISASubtarget.h"
*/
namespace llvm {

MCAsmInfo* createMCAsmInfo(const Target &T, StringRef TT);

class AMDISATargetMachine : public AMDILTargetMachine {
  AMDILSubtarget Subtarget;
/*   const TargetData DataLayout;
*/   AMDISATargetLowering TLInfo;
/*   AMDISASelectionDAGInfo TSInfo;
*/   OwningPtr<AMDISAInstrInfo> InstrInfo;
//   AMDISAFrameLowering FrameLowering;
     AMDILGlobalManager *mGM;
     AMDILKernelManager *mKM;
     bool mDump;

public:
   AMDISATargetMachine(const Target &T, StringRef TT, StringRef FS,
                       StringRef CPU, Reloc::Model RM, CodeModel::Model CM);
   ~AMDISATargetMachine();
   virtual const AMDISAInstrInfo *getInstrInfo() const {return InstrInfo.get();}
/*
   virtual const TargetFrameLowering *getFrameLowering() const {
      return &FrameLowering;
   }
*/
   virtual const AMDILSubtarget *getSubtargetImpl() const {return &Subtarget; }
   virtual const AMDISARegisterInfo *getRegisterInfo() const {
      return &InstrInfo->getRegisterInfo();
   }
   virtual AMDISATargetLowering * getTargetLowering() const {
      return const_cast<AMDISATargetLowering*>(&TLInfo);
   }
/*   virtual const AMDISASelectionDAGInfo* getSelectionDAGInfo() const {
      return &TSInfo;
   }
   virtual const TargetData *getTargetData() const { return &DataLayout; }
*/
   virtual bool addInstSelector(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
   virtual bool addPreEmitPass(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
   virtual bool addPreRegAlloc(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
   virtual bool addPostRegAlloc(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
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

#endif /* AMDISA_TARGET_MACHINE_H */
