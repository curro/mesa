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

#include "AMDIL.h"
#include "AMDGPUInstrInfo.h"

#include "AMDGPURegisterInfo.h"
#include "AMDGPUTargetMachine.h"

#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

AMDGPUInstrInfo::AMDGPUInstrInfo(AMDGPUTargetMachine &tm)
  : AMDILInstrInfo(tm), TM(tm)
{
  unsigned deviceGen =
                   TM.getSubtarget<AMDILSubtarget>().device()->getGeneration();
  for (unsigned i = 0; i < AMDIL::INSTRUCTION_LIST_END; i++) {
    const MCInstrDesc & instDesc = get(i);
    uint32_t instGen = (instDesc.TSFlags >> 40) & 0x7;
    uint32_t inst = (instDesc.TSFlags >>  48) & 0xffff;
    if (inst == 0) {
      continue;
    }
    switch (instGen) {
    case AMDGPUInstrInfo::R600_CAYMAN:
      if (deviceGen > AMDILDeviceInfo::HD6XXX) {
        continue;
      }
      break;
    case AMDGPUInstrInfo::R600:
      if (deviceGen != AMDILDeviceInfo::HD4XXX) {
        continue;
      }
      break;
    case AMDGPUInstrInfo::EG_CAYMAN:
      if (deviceGen < AMDILDeviceInfo::HD5XXX
          || deviceGen > AMDILDeviceInfo::HD6XXX) {
        continue;
      }
      break;
    case AMDGPUInstrInfo::CAYMAN:
      if (deviceGen != AMDILDeviceInfo::HD6XXX) {
        continue;
      }
      break;
    default:
      abort();
      break;
    }

    unsigned amdilOpcode = GetRealAMDILOpcode(inst);
    amdilToISA[amdilOpcode] = instDesc.Opcode;
  }
}

MachineInstr * AMDGPUInstrInfo::convertToISA(MachineInstr & MI, MachineFunction &MF,
    DebugLoc DL) const
{
  MachineInstrBuilder newInstr;
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const AMDGPURegisterInfo & RI = getRegisterInfo();
  unsigned ISAOpcode = getISAOpcode(MI.getOpcode());

  /* Create the new instruction */
  newInstr = BuildMI(MF, DL, TM.getInstrInfo()->get(ISAOpcode));

  for (unsigned i = 0; i < MI.getNumOperands(); i++) {
    MachineOperand &MO = MI.getOperand(i);
    /* Convert dst regclass to one that is supported by the ISA */
    if (MO.isReg() && MO.isDef()) {
      if (TargetRegisterInfo::isVirtualRegister(MO.getReg())) {
        const TargetRegisterClass * oldRegClass = MRI.getRegClass(MO.getReg());
        const TargetRegisterClass * newRegClass = RI.getISARegClass(oldRegClass);

        assert(newRegClass);

        MRI.setRegClass(MO.getReg(), newRegClass);
      }
    }
    /* Add the operand to the new instruction */
    newInstr.addOperand(MO);
  }

  return newInstr;
}

unsigned AMDGPUInstrInfo::getISAOpcode(unsigned opcode) const
{
  if (amdilToISA.count(opcode) == 0) {
    return opcode;
  } else {
    return amdilToISA.find(opcode)->second;
  }
}

bool AMDGPUInstrInfo::isRegPreload(const MachineInstr &MI) const
{
  return (get(MI.getOpcode()).TSFlags >> AMDGPU_TFLAG_SHIFTS::PRELOAD_REG) & 0x1;
}

#include "AMDGPUInstrEnums.inc"
