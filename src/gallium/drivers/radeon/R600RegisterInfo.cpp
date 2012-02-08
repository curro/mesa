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

#include "AMDISATargetMachine.h"

using namespace llvm;

R600RegisterInfo::R600RegisterInfo(AMDISATargetMachine &tm,
    const TargetInstrInfo &tii)
: AMDISARegisterInfo(tm, tii),
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
  for (unsigned i = AMDIL::C0; i <= AMDIL::C1023; i++) {
    Reserved.set(i);
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

bool R600RegisterInfo::isBaseRegClass(unsigned regClassID) const
{
  switch(regClassID) {
  case AMDIL::R600_CReg_32RegClassID:
  case AMDIL::GPRF32RegClassID:
  case AMDIL::REPLRegClassID:
    return true;
  default:
    return false;
  }
}

const TargetRegisterClass *
R600RegisterInfo::getISARegClass(const TargetRegisterClass * rc) const
{
  switch (rc->getID()) {
  case AMDIL::GPRV4F32RegClassID:
  case AMDIL::GPRV4I32RegClassID:
    return &AMDIL::REPLRegClass;
  case AMDIL::GPRF32RegClassID:
  case AMDIL::GPRI32RegClassID:
    return &AMDIL::R600_Reg32RegClass;
  default: return rc;
}


}
