//===- AMDILIntrinsicInfo.cpp - AMDIL Intrinsic Information ------*- C++ -*-===//
// Copyright (c) 2011, Advanced Micro Devices, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// If you use the software (in whole or in part), you shall adhere to all
// applicable U.S., European, and other export laws, including but not limited
// to the U.S. Export Administration Regulations (“EAR”), (15 C.F.R. Sections
// 730 through 774), and E.U. Council Regulation (EC) No 1334/2000 of 22 June
// 2000.  Further, pursuant to Section 740.6 of the EAR, you hereby certify
// that, except pursuant to a license granted by the United States Department
// of Commerce Bureau of Industry and Security or as otherwise permitted
// pursuant to a License Exception under the U.S. Export Administration
// Regulations ("EAR"), you will not (1) export, re-export or release to a
// national of a country in Country Groups D:1, E:1 or E:2 any restricted
// technology, software, or source code you receive hereunder, or (2) export to
// Country Groups D:1, E:1 or E:2 the direct product of such technology or
// software, if such foreign produced direct product is subject to national
// security controls as identified on the Commerce Control List (currently
// found in Supplement 1 to Part 774 of EAR).  For the most current Country
// Group listings, or for additional information about the EAR or your
// obligations under those regulations, please refer to the U.S. Bureau of
// Industry and Security’s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//
//
// This file contains the AMDIL Implementation of the IntrinsicInfo class.
//
//===-----------------------------------------------------------------------===//

#include "AMDIL.h"
#include "AMDILIntrinsicInfo.h"
#include "AMDILTargetMachine.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;

#define GET_LLVM_INTRINSIC_FOR_GCC_BUILTIN
#include "AMDILGenIntrinsics.inc"
#undef GET_LLVM_INTRINSIC_FOR_GCC_BUILTIN

AMDILIntrinsicInfo::AMDILIntrinsicInfo(AMDILTargetMachine *tm) 
  : TargetIntrinsicInfo(), mTM(tm)
{
}

std::string 
AMDILIntrinsicInfo::getName(unsigned int IntrID, Type **Tys,
    unsigned int numTys) const 
{
  static const char* const names[] = {
#define GET_INTRINSIC_NAME_TABLE
#include "AMDILGenIntrinsics.inc"
#undef GET_INTRINSIC_NAME_TABLE
  };

  //assert(!isOverloaded(IntrID)
  //&& "AMDIL Intrinsics are not overloaded");
  if (IntrID < Intrinsic::num_intrinsics) {
    return 0;
  }
  assert(IntrID < AMDILIntrinsic::num_AMDIL_intrinsics
      && "Invalid intrinsic ID");

  std::string Result(names[IntrID - Intrinsic::num_intrinsics]);
  return Result;
}

  static bool
checkTruncation(const char *Name, unsigned int& Len)
{
  const char *ptr = Name + (Len - 1);
  while(ptr != Name && *ptr != '_') {
    --ptr;
  }
  // We don't want to truncate on atomic instructions
  // but we do want to enter the check Truncation
  // section so that we can translate the atomic
  // instructions if we need to.
  if (!strncmp(Name, "__atom", 6)) {
    return true;
  }
  if (strstr(ptr, "i32")
      || strstr(ptr, "u32")
      || strstr(ptr, "i64")
      || strstr(ptr, "u64")
      || strstr(ptr, "f32")
      || strstr(ptr, "f64")
      || strstr(ptr, "i16")
      || strstr(ptr, "u16")
      || strstr(ptr, "i8")
      || strstr(ptr, "u8")) {
    Len = (unsigned int)(ptr - Name);
    return true;
  }
  return false;
}

// We don't want to support both the OpenCL 1.0 atomics
// and the 1.1 atomics with different names, so we translate
// the 1.0 atomics to the 1.1 naming here if needed.
static char*
atomTranslateIfNeeded(const char *Name, unsigned int Len) 
{
  char *buffer = NULL;
  if (strncmp(Name, "__atom_", 7))  {
    // If we are not starting with __atom_, then
    // go ahead and continue on with the allocation.
    buffer = new char[Len + 1];
    memcpy(buffer, Name, Len);
  } else {
    buffer = new char[Len + 3];
    memcpy(buffer, "__atomic_", 9);
    memcpy(buffer + 9, Name + 7, Len - 7);
    Len += 2;
  }
  buffer[Len] = '\0';
  return buffer;
}

unsigned int
AMDILIntrinsicInfo::lookupName(const char *Name, unsigned int Len) const 
{
#define GET_FUNCTION_RECOGNIZER
#include "AMDILGenIntrinsics.inc"
#undef GET_FUNCTION_RECOGNIZER
  AMDILIntrinsic::ID IntrinsicID
    = (AMDILIntrinsic::ID)Intrinsic::not_intrinsic;
  if (checkTruncation(Name, Len)) {
    char *buffer = atomTranslateIfNeeded(Name, Len);
    IntrinsicID = getIntrinsicForGCCBuiltin("AMDIL", buffer);
    delete [] buffer;
  } else {
    IntrinsicID = getIntrinsicForGCCBuiltin("AMDIL", Name);
  }
  if (!isValidIntrinsic(IntrinsicID)) {
    return 0;
  }
  if (IntrinsicID != (AMDILIntrinsic::ID)Intrinsic::not_intrinsic) {
    return IntrinsicID;
  }
  return 0;
}

bool 
AMDILIntrinsicInfo::isOverloaded(unsigned IntrID) const 
{
  // Overload Table
  const bool OTable[] = {
#define GET_INTRINSIC_OVERLOAD_TABLE
#include "AMDILGenIntrinsics.inc"
#undef GET_INTRINSIC_OVERLOAD_TABLE
  };
  if (!IntrID) {
    return false;
  }
  return OTable[IntrID - Intrinsic::num_intrinsics];
}

/// This defines the "getAttributes(ID id)" method.
#define GET_INTRINSIC_ATTRIBUTES
#include "AMDILGenIntrinsics.inc"
#undef GET_INTRINSIC_ATTRIBUTES

Function*
AMDILIntrinsicInfo::getDeclaration(Module *M, unsigned IntrID,
    Type **Tys,
    unsigned numTys) const 
{
  assert(!isOverloaded(IntrID) && "AMDIL intrinsics are not overloaded");
  AttrListPtr AList = getAttributes((AMDILIntrinsic::ID) IntrID);
  LLVMContext& Context = M->getContext();
  unsigned int id = IntrID;
  Type *ResultTy = NULL;
  std::vector<Type*> ArgTys;
  bool IsVarArg = false;

#define GET_INTRINSIC_GENERATOR
#include "AMDILGenIntrinsics.inc"
#undef GET_INTRINSIC_GENERATOR
  // We need to add the resource ID argument for atomics.
  if (id >= AMDILIntrinsic::AMDIL_atomic_add_gi32
        && id <= AMDILIntrinsic::AMDIL_atomic_xor_ru32_noret) {
    ArgTys.push_back(IntegerType::get(Context, 32));
  }

  return cast<Function>(M->getOrInsertFunction(getName(IntrID),
        FunctionType::get(ResultTy, ArgTys, IsVarArg),
        AList));
}

/// Because the code generator has to support different SC versions, 
/// this function is added to check that the intrinsic being used
/// is actually valid. In the case where it isn't valid, the 
/// function call is not translated into an intrinsic and the
/// fall back software emulated path should pick up the result.
bool
AMDILIntrinsicInfo::isValidIntrinsic(unsigned int IntrID) const
{
  const AMDILSubtarget *stm = mTM->getSubtargetImpl();
  switch (IntrID) {
    default:
      return true;
    case AMDILIntrinsic::AMDIL_convert_f32_i32_rpi:
    case AMDILIntrinsic::AMDIL_convert_f32_i32_flr:
    case AMDILIntrinsic::AMDIL_convert_f32_f16_near:
    case AMDILIntrinsic::AMDIL_convert_f32_f16_neg_inf:
    case AMDILIntrinsic::AMDIL_convert_f32_f16_plus_inf:
        return stm->calVersion() >= CAL_VERSION_SC_139;
  };
}
