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


#ifndef AMDISAINSTRUCTIONINFO_H_
#define AMDISAINSTRUCTIONINFO_H_

#include "AMDIL.h"
#include "AMDILInstrInfo.h"
#include "AMDISARegisterInfo.h"

#include <map>


namespace llvm {

  class AMDISATargetMachine;
  class MachineFunction;
  class MachineInstr;
  class MachineInstrBuilder;

  class AMDISAInstrInfo : public AMDILInstrInfo {
  private:
  AMDISATargetMachine & TM;
  std::map<unsigned, unsigned> amdilToISA;

  public:
  explicit AMDISAInstrInfo(AMDISATargetMachine &tm);

  virtual const AMDISARegisterInfo &getRegisterInfo() const = 0;

  virtual unsigned getISAOpcode(unsigned AMDILopcode) const;

  MachineInstr * convertToISA(MachineInstr & MI, MachineFunction &MF,
    DebugLoc DL) const;

  bool isRegPreload(const MachineInstr &MI) const;

  #include "AMDISAInstrEnums.h.inc"
  };

} // End llvm namespace

/* AMDISA target flags are stored in bits 32-39 */
namespace AMDISA_TFLAG_SHIFTS {
  enum TFLAGS {
    PRELOAD_REG = 32
  };
}


#endif // AMDISAINSTRINFO_H_
