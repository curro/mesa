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


#ifndef AMDISAREGISTERINFO_H_
#define AMDISAREGISTERINFO_H_

#include "AMDILRegisterInfo.h"

namespace llvm {

  class AMDISATargetMachine;
  class TargetInstrInfo;

  struct AMDISARegisterInfo : public AMDILRegisterInfo
  {
    AMDISATargetMachine &TM;
    const TargetInstrInfo &TII;

    AMDISARegisterInfo(AMDISATargetMachine &tm, const TargetInstrInfo &tii);

    virtual BitVector getReservedRegs(const MachineFunction &MF) const = 0;

    /* This is used to help calculate the index of a register.  A return value
     * of true means that the index of any register in this class may be
     * calcluated in this way:
     * TargetRegisterClass * TRC;
     * index = register - TRC->getRegister(0);
     */
    virtual bool isBaseRegClass(unsigned regClassID) const = 0;

    virtual const TargetRegisterClass *
    getISARegClass(const TargetRegisterClass * rc) const = 0;
  };
} // End namespace llvm

#endif // AMDIDSAREGISTERINFO_H_
