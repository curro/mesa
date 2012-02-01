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


#include "AMDISATargetMachine.h"

#include "AMDILGlobalManager.h"
#include "AMDILKernelManager.h"
#include "AMDILMCAsmInfo.h"
#include "AMDILTargetMachine.h"
#include "AMDISA.h"
#include "AMDISAISelLowering.h"
#include "R600InstrInfo.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/raw_os_ostream.h"

#include <iostream>

using namespace llvm;

MCAsmInfo* llvm::createMCAsmInfo(const Target &T, StringRef TT)
{
  Triple TheTriple(TT);
  switch (TheTriple.getOS()) {
    default:
    case Triple::UnknownOS:
      return new AMDILMCAsmInfo(TheTriple);
  }
}

AMDISATargetMachine::AMDISATargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS,
#if LLVM_VERSION > 3000
  TargetOptions Options,
#endif
  Reloc::Model RM, CodeModel::Model CM
#if LLVM_VERSION > 3000
  ,CodeGenOpt::Level OptLevel
#endif
)
:
  AMDILTargetMachine(T, TT, CPU, FS,
#if LLVM_VERSOIN > 3000
                     Options,
#endif
                     RM, CM
#if LLVM_VERSOIN > 3000
                     ,OptLevel
#endif
),
  Subtarget(TT, CPU, FS),
  TLInfo(*this),
  InstrInfo(new R600InstrInfo(*this)),
  mGM(new AMDILGlobalManager(0 /* Debug mode */)),
  mKM(new AMDILKernelManager(this, mGM)),
  mDump(false)
//   DataLayout(""/*Subtarget.getDataLayout()*/),
//   TLInfo(*this), TSInfo(*this), InstrInfo(Subtarget),
//   FrameLowering(Subtarget)

{
  /* XXX: Add these two initializations to fix a segfault, not sure if this
   * is correct.  These are normally initialized in the AsmPrinter, but AMDISA
   * does not use the asm printer */
  Subtarget.setGlobalManager(mGM);
  Subtarget.setKernelManager(mKM);
}

AMDISATargetMachine::~AMDISATargetMachine()
{
    delete mGM;
    delete mKM;
}

bool AMDISATargetMachine::addInstSelector(PassManagerBase &PM
                                          AMDIL_OPT_LEVEL_DECL) {
  if (AMDILTargetMachine::addInstSelector(PM AMDIL_OPT_LEVEL_VAR)) {
    return true;
  }

//  PM.add(createAMDISAFixRegClassesPass(*this));
  return false;
}

bool AMDISATargetMachine::addPreEmitPass(PassManagerBase &PM
     AMDIL_OPT_LEVEL_DECL)
{
  /* This is exactly the same as in AMDILTargetManager, minus theSwizzleEncoder
   * pass. */

  PM.add(createAMDILCFGPreparationPass(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDILCFGStructurizerPass(*this AMDIL_OPT_LEVEL_VAR));
//  PM.add(createAMDILIOExpansion(*this AMDIL_OPT_LEVEL_VAR));
  return false;
}

bool AMDISATargetMachine::addPreRegAlloc(PassManagerBase &PM
     AMDIL_OPT_LEVEL_DECL)
{
//  if (AMDILTargetMachine::addPreRegAlloc(PM AMDIL_OPT_LEVEL_VAR)) {
//    return true;
//  }

  PM.add(createAMDILLiteralManager(*this AMDIL_OPT_LEVEL_VAR));
  PM.add(createAMDISAReorderPreloadInstructionsPass(*this));
  if (Subtarget.device()->getGeneration() <= AMDILDeviceInfo::HD6XXX) {
    PM.add(createR600LowerShaderInstructionsPass(*this));
    PM.add(createR600LowerInstructionsPass(*this));
  }
  PM.add(createAMDISAConvertToISAPass(*this));
  return false;
}

/*XXX: We should use addPassesToEmitMC in llvm 3.0 */
bool AMDISATargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                              formatted_raw_ostream &Out,
                                              CodeGenFileType FileType,
                                              CodeGenOpt::Level OptLevel,
                                              bool DisableVerify) {
  PM.add(createTypeBasedAliasAnalysisPass());
  PM.add(createBasicAliasAnalysisPass());

  // Before running any passes, run the verifier to determine if the input
  // coming from the front-end and/or optimizer is valid.
  if (!DisableVerify)
    PM.add(createVerifierPass());

  // Run loop strength reduction before anything else.
  if (OptLevel != CodeGenOpt::None) {
    PM.add(createLoopStrengthReducePass(getTargetLowering()));
  }

  PM.add(createGCLoweringPass());

  // Make sure that no unreachable blocks are instruction selected.
  PM.add(createUnreachableBlockEliminationPass());

  // Turn exception handling constructs into something the code generators can
  // handle.
  switch (getMCAsmInfo()->getExceptionHandlingType()) {
  case ExceptionHandling::SjLj:
    // SjLj piggy-backs on dwarf for this bit. The cleanups done apply to both
    // Dwarf EH prepare needs to be run after SjLj prepare. Otherwise,
    // catch info can get misplaced when a selector ends up more than one block
    // removed from the parent invoke(s). This could happen when a landing
    // pad is shared by multiple invokes and is also a target of a normal
    // edge from elsewhere.
    PM.add(createSjLjEHPass(getTargetLowering()));
    // FALLTHROUGH
  case ExceptionHandling::DwarfCFI:
  case ExceptionHandling::ARM:
  case ExceptionHandling::Win64:
    PM.add(createDwarfEHPass(this));
    break;
  case ExceptionHandling::None:
    PM.add(createLowerInvokePass(getTargetLowering()));

    // The lower invoke pass may create unreachable code. Remove it.
    PM.add(createUnreachableBlockEliminationPass());
    break;
  }

  if (OptLevel != CodeGenOpt::None)
    PM.add(createCodeGenPreparePass(getTargetLowering()));

  PM.add(createStackProtectorPass(getTargetLowering()));

  addPreISel(PM AMDIL_OPT_LEVEL_VAR);

  // All passes which modify the LLVM IR are now complete; run the verifier
  // to ensure that the IR is valid.
  if (!DisableVerify)
    PM.add(createVerifierPass());

  // Standard Lower-Level Passes.

  // Install a MachineModuleInfo class, which is an immutable pass that holds
  // all the per-module stuff we're generating, including MCContext.
  MachineModuleInfo *MMI = new MachineModuleInfo(*getMCAsmInfo(),
                                                 *getRegisterInfo(),
                                     (MCObjectFileInfo*)&getTargetLowering()->getObjFileLowering());
  PM.add(MMI);

  // Set up a MachineFunction for the rest of CodeGen to work on.
  PM.add(new MachineFunctionAnalysis(*this));

  // Ask the target for an isel.
  if (addInstSelector(PM AMDIL_OPT_LEVEL_VAR))
    return true;

  // Expand pseudo-instructions emitted by ISel.
  PM.add(createExpandISelPseudosPass());

  // Pre-ra tail duplication.
  if (OptLevel != CodeGenOpt::None) {
    PM.add(createTailDuplicatePass(true));
  }

  // Optimize PHIs before DCE: removing dead PHI cycles may make more
  // instructions dead.
  if (OptLevel != CodeGenOpt::None)
    PM.add(createOptimizePHIsPass());

  // If the target requests it, assign local variables to stack slots relative
  // to one another and simplify frame index references where possible.
  PM.add(createLocalStackSlotAllocationPass());

  if (OptLevel != CodeGenOpt::None) {
    // With optimization, dead code should already be eliminated. However
    // there is one known exception: lowered code for arguments that are only
    // used by tail calls, where the tail calls reuse the incoming stack
    // arguments directly (see t11 in test/CodeGen/X86/sibcall.ll).
    PM.add(createDeadMachineInstructionElimPass());

    PM.add(createMachineLICMPass());
    PM.add(createMachineCSEPass());
    PM.add(createMachineSinkingPass());

    PM.add(createPeepholeOptimizerPass());
  }

  // Run pre-ra passes.
  addPreRegAlloc(PM AMDIL_OPT_LEVEL_VAR);

  // Perform register allocation.
  PM.add(createRegisterAllocator(OptLevel));

  // Perform stack slot coloring and post-ra machine LICM.
  if (OptLevel != CodeGenOpt::None) {
    // FIXME: Re-enable coloring with register when it's capable of adding
    // kill markers.
    PM.add(createStackSlotColoringPass(false));

    // Run post-ra machine LICM to hoist reloads / remats.
    PM.add(createMachineLICMPass(false));

  }

  // Run post-ra passes.
  addPostRegAlloc(PM AMDIL_OPT_LEVEL_VAR);

  PM.add(createExpandPostRAPseudosPass());

  // Insert prolog/epilog code.  Eliminate abstract frame index references...
  PM.add(createPrologEpilogCodeInserter());

  // Run pre-sched2 passes.
  addPreSched2(PM AMDIL_OPT_LEVEL_VAR);

  // Second pass scheduler.
  if (OptLevel != CodeGenOpt::None) {
    PM.add(createPostRAScheduler(OptLevel));
  }

  // Branch folding must be run after regalloc and prolog/epilog insertion.
  if (OptLevel != CodeGenOpt::None) {
    PM.add(createBranchFoldingPass(getEnableTailMergeDefault()));
  }

  // Tail duplication.
  if (OptLevel != CodeGenOpt::None) {
    PM.add(createTailDuplicatePass(false));
  }

  PM.add(createGCMachineCodeAnalysisPass());

  if (OptLevel != CodeGenOpt::None) {
    PM.add(createCodePlacementOptPass());
  }

  addPreEmitPass(PM AMDIL_OPT_LEVEL_VAR);

  if (Subtarget.device()->getGeneration() <= AMDILDeviceInfo::HD6XXX) {
    PM.add(createR600CodeEmitterPass(Out));
  } else {
    abort();
    return true;
  }
  PM.add(createGCInfoDeleter());

   return false;
}
