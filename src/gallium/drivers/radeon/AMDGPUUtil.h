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


#ifndef AMDGPU_UTIL_H
#define AMDGPU_UTIL_H

#include "llvm/Support/DataTypes.h"
#include "AMDGPURegisterInfo.h"

namespace llvm {

class AMDILMachineFunctionInfo;

class TargetMachine;
class TargetRegisterInfo;

bool isPlaceHolderOpcode(unsigned opcode);

unsigned getRegElement(const AMDGPURegisterInfo * TRI, unsigned regNo);
unsigned getHWRegNum(const AMDGPURegisterInfo * TRI, unsigned amdilRegNo);

bool isTransOp(unsigned opcode);
bool isTexOp(unsigned opcode);
bool isReductionOp(unsigned opcode);
bool isFCOp(unsigned opcode);

/* XXX: Move these to AMDGPUInstrInfo.h */
#define MO_FLAG_CLAMP (1 << 0)
#define MO_FLAG_NEG   (1 << 1)
#define MO_FLAG_ABS   (1 << 2)
#define MO_FLAG_MASK  (1 << 3)

} /* End namespace llvm */

#endif /* AMDGPU_UTIL_H */
