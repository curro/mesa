//===-- AMDGPUISelLowering.cpp - TODO: Add brief description -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// TODO: Add full description
//
//===----------------------------------------------------------------------===//

#include "AMDGPUISelLowering.h"
#include "AMDGPUUtil.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

AMDGPUTargetLowering::AMDGPUTargetLowering(TargetMachine &TM) :
  AMDILTargetLowering(TM)
{
}

void AMDGPUTargetLowering::addLiveIn(MachineInstr * MI,
    MachineFunction * MF, MachineRegisterInfo & MRI,
    const struct TargetInstrInfo * TII, unsigned reg) const
{
  AMDGPU::utilAddLiveIn(MF, MRI, TII, reg, MI->getOperand(0).getReg()); 
}

