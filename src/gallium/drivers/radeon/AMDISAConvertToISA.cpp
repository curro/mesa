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
#include "llvm/Constants.h"
#include "llvm/Target/TargetInstrInfo.h"

#include "AMDIL.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILRegisterInfo.h"
#include "AMDISA.h"
#include "AMDISAInstrInfo.h"
#include "AMDISAUtil.h"

#include "R600InstrInfo.h"

#include <stdio.h>

using namespace llvm;

namespace {
  class AMDISAConvertToISAPass : public MachineFunctionPass {

  private:
    static char ID;
    TargetMachine &TM;

    void lowerFLT(MachineInstr &MI);

  public:
    AMDISAConvertToISAPass(TargetMachine &tm) :
      MachineFunctionPass(ID), TM(tm) { }

    virtual bool runOnMachineFunction(MachineFunction &MF);

  };
} /* End anonymous namespace */

char AMDISAConvertToISAPass::ID = 0;

FunctionPass *llvm::createAMDISAConvertToISAPass(TargetMachine &tm) {
  return new AMDISAConvertToISAPass(tm);
}

bool AMDISAConvertToISAPass::runOnMachineFunction(MachineFunction &MF)
{
  const AMDISAInstrInfo * TII =
                      static_cast<const AMDISAInstrInfo*>(TM.getInstrInfo());

  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
    MachineBasicBlock &MBB = *BB;
    for (MachineBasicBlock::iterator I = MBB.begin(), Next = llvm::next(I);
         I != MBB.end(); I = Next, Next = llvm::next(I) ) {
      MachineInstr &MI = *I;
      MachineInstr * newInstr = TII->convertToISA(MI, MF, MBB.findDebugLoc(I));
      if (!newInstr) {
        continue;
      }
      MBB.insert(I, newInstr);
      MI.eraseFromParent();
    }
  }
  return false;
}