//===-- AMDILTargetMachine.cpp - Define TargetMachine for AMDIL -----------===//
// Copyright (c) 2011, Advanced Micro Devices, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// If you use the software (in whole or in part), you shall adhere to all
// applicable U.S., European, and other export laws, including but not limited
// to the U.S. Export Administration Regulations (“EAR”), (15 C.F.R. Sections
// 730 through 774), and E.U. Council Regulation (EC) No 1334/2000 of 22 June
// 2000.  Further, pursuant to Section 740.6 of the EAR, you hereby certify
// that, except pursuant to a license granted by the United States Department
// of Commerce Bureau of Industry and Security or as otherwise permitted
// pursuant to a License Exception under the U.S. Export Administration
// Regulations ("EAR"), you will not (1) export, re-export or release to a
// national of a country in Country Groups D:1, E:1 or E:2 any restricted
// technology, software, or source code you receive hereunder, or (2) export to
// Country Groups D:1, E:1 or E:2 the direct product of such technology or
// software, if such foreign produced direct product is subject to national
// security controls as identified on the Commerce Control List (currently
// found in Supplement 1 to Part 774 of EAR).  For the most current Country
// Group listings, or for additional information about the EAR or your
// obligations under those regulations, please refer to the U.S. Bureau of
// Industry and Security’s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//


#include "AMDILTargetMachine.h"
#include "AMDILDevices.h"
#if LLVM_VERSION >= 2500
#include "AMDILFrameLowering.h"
#else
#include "AMDILFrameInfo.h"
#endif 
#include "AMDILMCAsmInfo.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"

#include "KernelParameters.h"

using namespace llvm;

static MCAsmInfo* createMCAsmInfo(const Target &T, StringRef TT)
{
  Triple TheTriple(TT);
  switch (TheTriple.getOS()) {
    default:
    case Triple::UnknownOS:
      return new AMDILMCAsmInfo(TheTriple);
  }
}

extern "C" void LLVMInitializeAMDILTarget() {
  // Register the target
//  RegisterTargetMachine<TheAMDILTargetMachine> X(TheAMDILTarget);

  // Register the target asm info
//  RegisterMCAsmInfoFn A(TheAMDILTarget, createMCAsmInfo);

  // Register the code emitter
  //TargetRegistry::RegisterCodeEmitter(TheAMDILTarget,
  //createAMDILMCCodeEmitter);
}

/*
TheAMDILTargetMachine::TheAMDILTargetMachine(const Target &T,
    StringRef TT, StringRef CPU, StringRef FS,
    Reloc::Model RM, CodeModel::Model CM)
: AMDILTargetMachine(T, TT, CPU, FS, RM, CM)
{
}
*/

/// AMDILTargetMachine ctor -
///
AMDILTargetMachine::AMDILTargetMachine(const Target &T,
    StringRef TT, StringRef CPU, StringRef FS,
#if LLVM_VERSION > 3000
    TargetOptions Options,
#endif
    Reloc::Model RM, CodeModel::Model CM
#if LLVM_VERSION > 3000
    ,CodeGenOpt::Level OL
#endif
)
:
  LLVMTargetMachine(T, TT, CPU, FS,
#if LLVM_VERSION > 3000
  Options,
#endif
  RM, CM
#if LLVM_VERSION > 3000
  ,OL
#endif
),
  Subtarget(TT, CPU, FS),
  DataLayout(Subtarget.getDataLayout()),
#if LLVM_VERSION >= 2500
  FrameLowering(TargetFrameLowering::StackGrowsUp,
#else
  FrameInfo(TargetFrameInfo::StackGrowsUp,
#endif
      Subtarget.device()->getStackAlignment(), 0),
  InstrInfo(*this), //JITInfo(*this),
  TLInfo(*this), 
  IntrinsicInfo(this),
  ELFWriterInfo(false, true)
{
  setAsmVerbosityDefault(true);
  setMCUseLoc(false);
}

AMDILTargetLowering*
AMDILTargetMachine::getTargetLowering() const
{
  return const_cast<AMDILTargetLowering*>(&TLInfo);
}

const AMDILInstrInfo*
AMDILTargetMachine::getInstrInfo() const
{
  return &InstrInfo;
}
#if LLVM_VERSION >= 2500
const AMDILFrameLowering*
AMDILTargetMachine::getFrameLowering() const
{
  return &FrameLowering;
}
#else
const AMDILFrameInfo*
AMDILTargetMachine::getFrameInfo() const
{
  return &FrameInfo;
}
#endif

const AMDILSubtarget*
AMDILTargetMachine::getSubtargetImpl() const
{
  return &Subtarget;
}

const AMDILRegisterInfo*
AMDILTargetMachine::getRegisterInfo() const
{
  return &InstrInfo.getRegisterInfo();
}

const TargetData*
AMDILTargetMachine::getTargetData() const
{
  return &DataLayout;
}

const AMDILELFWriterInfo*
AMDILTargetMachine::getELFWriterInfo() const
{
  return Subtarget.isTargetELF() ? &ELFWriterInfo : 0;
}

const AMDILIntrinsicInfo*
AMDILTargetMachine::getIntrinsicInfo() const
{
  return &IntrinsicInfo;
}
bool
AMDILTargetMachine::addPreISel(PassManagerBase &PM
#if LLVM_VERSION <= 3000
 AMDIL_OPT_LEVEL_DECL
#endif
)
{
  return true;
}

  bool
AMDILTargetMachine::addInstSelector(PassManagerBase &PM AMDIL_OPT_LEVEL_DECL)
{
#if LLVM_VERSION <=3000
  mOptLevel = AMDIL_OPT_LEVEL_VAR_NO_COMMA;
#else
  mOptLevel = getOptLevel();
#endif
  PM.add(createKernelParametersPass(this->getTargetData()));
  PM.add(createAMDILBarrierDetect(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILPrintfConvert(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILInlinePass(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILPeepholeOpt(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILISelDag(*this AMDIL_OPT_LEVEL_VAR));
  return false;
}
  bool
AMDILTargetMachine::addPreRegAlloc(PassManagerBase &PM
#if LLVM_VERSION <= 3000
     AMDIL_OPT_LEVEL_DECL
#endif
)

{
#if LLVM_VERSION > 3000
  CodeGenOpt::Level OptLevel = getOptLevel();
#endif
  // If debugging, reduce code motion. Use less aggressive pre-RA scheduler
  if (OptLevel == CodeGenOpt::None) {
    llvm::RegisterScheduler::setDefault(&llvm::createSourceListDAGScheduler);
  }

  PM.add(createAMDILMachinePeephole(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILPointerManager(*this AMDIL_OPT_LEVEL_VAR));
  return false;
}

bool
AMDILTargetMachine::addPostRegAlloc(PassManagerBase &PM
#if LLVM_VERSION <= 3000
     AMDIL_OPT_LEVEL_DECL
#endif
) {
  return false;  // -print-machineinstr should print after this.
}

/// addPreEmitPass - This pass may be implemented by targets that want to run
/// passes immediately before machine code is emitted.  This should return
/// true if -print-machineinstrs should print out the code after the passes.
  bool
AMDILTargetMachine::addPreEmitPass(PassManagerBase &PM
#if LLVM_VERSION <= 3000
   AMDIL_OPT_LEVEL_DECL
#endif
)
{
  PM.add(createAMDILCFGPreparationPass(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILCFGStructurizerPass(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILLiteralManager(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILIOExpansion(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILSwizzleEncoder(*this AMDIL_OPT_LEVEL_VAR));
  return true;
}

  void
AMDILTargetMachine::dump(OSTREAM_TYPE &O)
{
  if (!mDebugMode) {
    return;
  }
  O << ";AMDIL Target Machine State Dump: \n";
}

  void
AMDILTargetMachine::setDebug(bool debugMode)
{
  mDebugMode = debugMode;
}

bool
AMDILTargetMachine::getDebug() const
{
  return mDebugMode;
}
