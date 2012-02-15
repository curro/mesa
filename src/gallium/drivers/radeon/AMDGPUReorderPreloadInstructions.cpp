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


#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

#include "AMDIL.h"
#include "AMDGPU.h"
#include "AMDILInstrInfo.h"

#include <vector>

using namespace llvm;

namespace {
  class AMDGPUReorderPreloadInstructionsPass : public MachineFunctionPass {

  private:
    static char ID;
    TargetMachine &TM;

  public:
    AMDGPUReorderPreloadInstructionsPass(TargetMachine &tm) :
      MachineFunctionPass(ID), TM(tm) { }

      bool runOnMachineFunction(MachineFunction &MF);

      const char *getPassName() const { return "AMDGPU Reorder Preload Instructions"; }
    };
} /* End anonymous namespace */

char AMDGPUReorderPreloadInstructionsPass::ID = 0;

FunctionPass *llvm::createAMDGPUReorderPreloadInstructionsPass(TargetMachine &tm) {
    return new AMDGPUReorderPreloadInstructionsPass(tm);
}

/* This pass moves instructions that represent preloaded registers to the
 * start of the program. */
bool AMDGPUReorderPreloadInstructionsPass::runOnMachineFunction(MachineFunction &MF)
{
  const AMDGPUInstrInfo * TII =
                        static_cast<const AMDGPUInstrInfo*>(TM.getInstrInfo());

  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
    MachineBasicBlock &MBB = *BB;
    for (MachineBasicBlock::iterator I = MBB.begin(), Next = llvm::next(I);
         I != MBB.end(); I = Next, Next = llvm::next(I) ) {
      MachineInstr &MI = *I;
      if (TII->isRegPreload(MI)) {
         MF.front().insert(MF.front().begin(), MI.removeFromParent());
      }
    }
  }
  return false;
}
