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


#include "R600RegisterInfo.h"

#include "AMDGPUTargetMachine.h"

using namespace llvm;

R600RegisterInfo::R600RegisterInfo(AMDGPUTargetMachine &tm,
    const TargetInstrInfo &tii)
: AMDGPURegisterInfo(tm, tii),
  TM(tm),
  TII(tii)
  { }

BitVector R600RegisterInfo::getReservedRegs(const MachineFunction &MF) const
{
  BitVector Reserved(getNumRegs());
  Reserved.set(AMDIL::ZERO);
  Reserved.set(AMDIL::HALF);
  Reserved.set(AMDIL::ONE);
  Reserved.set(AMDIL::NEG_HALF);
  Reserved.set(AMDIL::NEG_ONE);
  Reserved.set(AMDIL::PV_X);
  Reserved.set(AMDIL::ALU_LITERAL_X);

  for (TargetRegisterClass::iterator I = AMDIL::R600_CReg32RegClass.begin(),
                        E = AMDIL::R600_CReg32RegClass.end(); I != E; ++I) {
    Reserved.set(*I);
  }

  for (MachineFunction::const_iterator BB = MF.begin(),
                                 BB_E = MF.end(); BB != BB_E; ++BB) {
    const MachineBasicBlock &MBB = *BB;
    for (MachineBasicBlock::const_iterator I = MBB.begin(), E = MBB.end();
                                                                  I != E; ++I) {
      const MachineInstr &MI = *I;
      if (MI.getOpcode() == AMDIL::RESERVE_REG) {
        if (!TargetRegisterInfo::isVirtualRegister(MI.getOperand(0).getReg())) {
          Reserved.set(MI.getOperand(0).getReg());
        }
      }
    }
  }
  return Reserved;
}

const TargetRegisterClass *
R600RegisterInfo::getISARegClass(const TargetRegisterClass * rc) const
{
  switch (rc->getID()) {
  case AMDIL::GPRV4F32RegClassID:
  case AMDIL::GPRV4I32RegClassID:
    return &AMDIL::R600_Reg128RegClass;
  case AMDIL::GPRF32RegClassID:
  case AMDIL::GPRI32RegClassID:
    return &AMDIL::R600_Reg32RegClass;
  default: return rc;
  }
}

unsigned R600RegisterInfo::getHWRegIndex(unsigned reg) const
{
  switch(reg) {
  case AMDIL::ZERO: return 248;
  case AMDIL::ONE:
  case AMDIL::NEG_ONE: return 249;
  case AMDIL::HALF:
  case AMDIL::NEG_HALF: return 252;
  case AMDIL::ALU_LITERAL_X: return 253;
  default: return getHWRegIndexGen(reg);
  }
}

unsigned R600RegisterInfo::getHWRegChan(unsigned reg) const
{
  switch(reg) {
  case AMDIL::ZERO:
  case AMDIL::ONE:
  case AMDIL::NEG_ONE:
  case AMDIL::HALF:
  case AMDIL::NEG_HALF:
  case AMDIL::ALU_LITERAL_X:
    return 0;
  default: return getHWRegChanGen(reg);
  }
}

#include "R600HwRegInfo.inc"
