//===-- AMDGPUConvertToISA.cpp - Lower AMDIL to HW ISA --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass lowers AMDIL machine instructions to the appropriate hardware
// instructions. 
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUInstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

namespace {
  class AMDGPUConvertToISAPass : public MachineFunctionPass {

  private:
    static char ID;
    TargetMachine &TM;

    void lowerFLT(MachineInstr &MI);

  public:
    AMDGPUConvertToISAPass(TargetMachine &tm) :
      MachineFunctionPass(ID), TM(tm) { }

    virtual bool runOnMachineFunction(MachineFunction &MF);

  };
} /* End anonymous namespace */

char AMDGPUConvertToISAPass::ID = 0;

FunctionPass *llvm::createAMDGPUConvertToISAPass(TargetMachine &tm) {
  return new AMDGPUConvertToISAPass(tm);
}

bool AMDGPUConvertToISAPass::runOnMachineFunction(MachineFunction &MF)
{
  const AMDGPUInstrInfo * TII =
                      static_cast<const AMDGPUInstrInfo*>(TM.getInstrInfo());

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
