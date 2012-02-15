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


#ifndef R600REGISTERINFO_H_
#define R600REGISTERINFO_H_

#include "AMDILRegisterInfo.h"
#include "AMDGPUTargetMachine.h"

namespace llvm {

  class R600TargetMachine;
  class TargetInstrInfo;

  struct R600RegisterInfo : public AMDGPURegisterInfo
  {
    AMDGPUTargetMachine &TM;
    const TargetInstrInfo &TII;

    R600RegisterInfo(AMDGPUTargetMachine &tm, const TargetInstrInfo &tii);

    virtual BitVector getReservedRegs(const MachineFunction &MF) const;

    virtual bool isBaseRegClass(unsigned regClassID) const;

    virtual const TargetRegisterClass *
    getISARegClass(const TargetRegisterClass * rc) const;
  };
} // End namespace llvm

#endif // AMDIDSAREGISTERINFO_H_
