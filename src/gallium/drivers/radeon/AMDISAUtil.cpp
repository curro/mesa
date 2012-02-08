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

#include "AMDISAUtil.h"
#include "AMDIL.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDISARegisterInfo.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"

using namespace llvm;

/* Some instructions act as place holders to emulate operations that the GPU
 * hardware does automatically. This function can be used to check if
 * an opcode falls into this category. */
bool llvm::isPlaceHolderOpcode(unsigned opcode)
{
  switch (opcode) {
  default: return false;
  case AMDIL::EXPORT_REG:
  case AMDIL::LOAD_INPUT:
  case AMDIL::LAST:
  case AMDIL::RESERVE_REG:
    return true;
  }
}

/* For f32 registers, this returns the corresponding element (X,Y,Z, or W) of
 * the v4f32 super register that it belongs to.
 */
unsigned llvm::getRegElement(const AMDISARegisterInfo * TRI, unsigned regNo)
{
  if (AMDIL::REPLRegisterClass->contains(regNo)
      || AMDIL::SPECIALRegisterClass->contains(regNo)
      || regNo == AMDIL::ALU_LITERAL_X) {
    return 0;
  } else {
    return getHWRegNum(TRI, regNo) % 4;
  }
}

unsigned llvm::getHWRegNum(const AMDISARegisterInfo * TRI, unsigned amdilRegNo)
{
  for (TargetRegisterInfo::regclass_iterator RI = TRI->regclass_begin(),
       RE = TRI->regclass_end(); RI != RE; ++RI) {
    const TargetRegisterClass * TRC = *RI;
    if (TRC->contains(amdilRegNo) && TRI->isBaseRegClass(TRC->getID())) {
      return amdilRegNo - TRC->getRegister(0);
    }
  }
  abort();
  return 0;
}

bool llvm::isTransOp(unsigned opcode)
{
  switch(opcode) {
    default: return false;

    case AMDIL::COS_f32:
    case AMDIL::COS_r600:
    case AMDIL::COS_eg:
    case AMDIL::RSQ_f32:
    case AMDIL::FTOI:
    case AMDIL::ITOF:
    case AMDIL::MULLIT:
    case AMDIL::MUL_LIT_r600:
    case AMDIL::MUL_LIT_eg:
    case AMDIL::SHR_i32:
    case AMDIL::SIN_f32:
    case AMDIL::EXP_f32:
    case AMDIL::EXP_IEEE_r600:
    case AMDIL::EXP_IEEE_eg:
    case AMDIL::LOG_CLAMPED_r600:
    case AMDIL::LOG_IEEE_r600:
    case AMDIL::LOG_CLAMPED_eg:
    case AMDIL::LOG_IEEE_eg:
    case AMDIL::LOG_f32:
      return true;
  }
}

bool llvm::isTexOp(unsigned opcode)
{
  switch(opcode) {
  default: return false;
  case AMDIL::TEX_SAMPLE:
  case AMDIL::TEX_SAMPLE_C:
  case AMDIL::TEX_SAMPLE_L:
  case AMDIL::TEX_SAMPLE_C_L:
  case AMDIL::TEX_SAMPLE_LB:
  case AMDIL::TEX_SAMPLE_C_LB:
  case AMDIL::TEX_SAMPLE_G:
  case AMDIL::TEX_SAMPLE_C_G:
    return true;
  }
}

bool llvm::isReductionOp(unsigned opcode)
{
  switch(opcode) {
    default: return false;
    case AMDIL::DOT4_r600:
    case AMDIL::DOT4_eg:
      return true;
  }
}

bool llvm::isFCOp(unsigned opcode)
{
  switch(opcode) {
  default: return false;
  case AMDIL::BREAK_LOGICALZ_f32:
  case AMDIL::CONTINUE_LOGICALNZ_f32:
  case AMDIL::IF_LOGICALZ_f32:
	case AMDIL::ELSE:
  case AMDIL::ENDIF:
  case AMDIL::ENDLOOP:
  case AMDIL::IF_LOGICALNZ_f32:
  case AMDIL::WHILELOOP:
    return true;
  }
}
