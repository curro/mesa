//===-- AMDILUtilityFunctions.h - AMDIL Utility Functions Header --------===//
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
// This file provides declarations for functions that are used across different
// classes and provide various conversions or utility to shorten the code
//
//===----------------------------------------------------------------------===//
#ifndef AMDILUTILITYFUNCTIONS_H_
#define AMDILUTILITYFUNCTIONS_H_

#include "AMDIL.h"
#include "AMDILLLVMVersion.h"
#include "AMDILTargetMachine.h"
#include "llvm/ADT/SmallVector.h"
// Utility functions from ID
//
namespace llvm {
class TargetRegisterClass;
class SDValue;
class SDNode;
class Value;
class Type;
class StructType;
class IntegerType;
class FunctionType;
class VectorType;
class ArrayType;
class PointerType;
class OpaqueType;
class MachineInstr;

}
enum SrcSwizzles {
  AMDIL_SRC_SWIZZLE_DEFAULT = 0,
  AMDIL_SRC_SWIZZLE_X000,
  AMDIL_SRC_SWIZZLE_0X00,
  AMDIL_SRC_SWIZZLE_00X0,
  AMDIL_SRC_SWIZZLE_000X,
  AMDIL_SRC_SWIZZLE_Y000,
  AMDIL_SRC_SWIZZLE_0Y00,
  AMDIL_SRC_SWIZZLE_00Y0,
  AMDIL_SRC_SWIZZLE_000Y,
  AMDIL_SRC_SWIZZLE_Z000,
  AMDIL_SRC_SWIZZLE_0Z00,
  AMDIL_SRC_SWIZZLE_00Z0,
  AMDIL_SRC_SWIZZLE_000Z,
  AMDIL_SRC_SWIZZLE_W000,
  AMDIL_SRC_SWIZZLE_0W00,
  AMDIL_SRC_SWIZZLE_00W0,
  AMDIL_SRC_SWIZZLE_000W,
  AMDIL_SRC_SWIZZLE_XY00,
  AMDIL_SRC_SWIZZLE_00XY,
  AMDIL_SRC_SWIZZLE_ZW00,
  AMDIL_SRC_SWIZZLE_00ZW,
  AMDIL_SRC_SWIZZLE_XYZ0,
  AMDIL_SRC_SWIZZLE_0XYZ,
  AMDIL_SRC_SWIZZLE_XYZW,
  AMDIL_SRC_SWIZZLE_0000,
  AMDIL_SRC_SWIZZLE_XXXX,
  AMDIL_SRC_SWIZZLE_YYYY,
  AMDIL_SRC_SWIZZLE_ZZZZ,
  AMDIL_SRC_SWIZZLE_WWWW,
  AMDIL_SRC_SWIZZLE_XYXY,
  AMDIL_SRC_SWIZZLE_ZWZW,
  AMDIL_SRC_SWIZZLE_XZXZ,
  AMDIL_SRC_SWIZZLE_YWYW,
  AMDIL_SRC_SWIZZLE_X0Y0,
  AMDIL_SRC_SWIZZLE_0X0Y,
  AMDIL_SRC_SWIZZLE_XY_NEGY,
  AMDIL_SRC_SWIZZLE_NEGYW,
  AMDIL_SRC_SWIZZLE_NEGX,
  AMDIL_SRC_SWIZZLE_XY_NEGXY,
  AMDIL_SRC_SWIZZLE_NEG_XYZW,
  AMDIL_SRC_SWIZZLE_0YZW,
  AMDIL_SRC_SWIZZLE_X0ZW,
  AMDIL_SRC_SWIZZLE_XY0W,
  AMDIL_SRC_SWIZZLE_X,
  AMDIL_SRC_SWIZZLE_Y,
  AMDIL_SRC_SWIZZLE_Z,
  AMDIL_SRC_SWIZZLE_W,
  AMDIL_SRC_SWIZZLE_XY,
  AMDIL_SRC_SWIZZLE_ZW,
  AMDIL_SRC_SWIZZLE_LAST
};
enum DstSwizzles {
  AMDIL_DST_SWIZZLE_DEFAULT = 0,
  AMDIL_DST_SWIZZLE_X___,
  AMDIL_DST_SWIZZLE_XY__,
  AMDIL_DST_SWIZZLE_XYZ_,
  AMDIL_DST_SWIZZLE_XYZW,
  AMDIL_DST_SWIZZLE__Y__,
  AMDIL_DST_SWIZZLE__YZ_,
  AMDIL_DST_SWIZZLE__YZW,
  AMDIL_DST_SWIZZLE___Z_,
  AMDIL_DST_SWIZZLE___ZW,
  AMDIL_DST_SWIZZLE____W,
  AMDIL_DST_SWIZZLE_X_ZW,
  AMDIL_DST_SWIZZLE_XY_W,
  AMDIL_DST_SWIZZLE_X_Z_,
  AMDIL_DST_SWIZZLE_X__W,
  AMDIL_DST_SWIZZLE__Y_W,
  AMDIL_DST_SWIZZLE_LAST
};
// Function to get the correct src swizzle string from ID
const char *getSrcSwizzle(unsigned);

// Function to get the correct dst swizzle string from ID
const char *getDstSwizzle(unsigned);

const llvm::TargetRegisterClass *getRegClassFromID(unsigned int ID);

unsigned int getMoveInstFromID(unsigned int ID);
unsigned int getPHIMoveInstFromID(unsigned int ID);

// Utility functions from Type.
const llvm::TargetRegisterClass *getRegClassFromType(unsigned int type);
unsigned int getTargetIndependentMoveFromType(unsigned int type);

// Debug functions for SDNode and SDValue.
void printSDValue(const llvm::SDValue &Op, int level);
void printSDNode(const llvm::SDNode *N);

// Functions to check if an opcode is a specific type.
bool isMove(unsigned int opcode);
bool isPHIMove(unsigned int opcode);
bool isMoveOrEquivalent(unsigned int opcode);

// Function to check address space
bool check_type(const llvm::Value *ptr, unsigned int addrspace);

// Group of functions that recursively calculate the size of a structure based
// on it's sub-types.
size_t getTypeSize(llvm::Type * const T, bool dereferencePtr = false);
size_t
getTypeSize(llvm::StructType * const ST, bool dereferencePtr = false);
size_t
getTypeSize(llvm::IntegerType * const IT, bool dereferencePtr = false);
size_t
getTypeSize(llvm::FunctionType * const FT, bool dereferencePtr = false);
size_t
getTypeSize(llvm::ArrayType * const AT, bool dereferencePtr = false);
size_t
getTypeSize(llvm::VectorType * const VT, bool dereferencePtr = false);
size_t
getTypeSize(llvm::PointerType * const PT, bool dereferencePtr = false);
size_t
getTypeSize(llvm::OpaqueType * const OT, bool dereferencePtr = false);

// Group of functions that recursively calculate the number of elements of a
// structure based on it's sub-types.
size_t getNumElements(llvm::Type * const T);
size_t getNumElements(llvm::StructType * const ST);
size_t getNumElements(llvm::IntegerType * const IT);
size_t getNumElements(llvm::FunctionType * const FT);
size_t getNumElements(llvm::ArrayType * const AT);
size_t getNumElements(llvm::VectorType * const VT);
size_t getNumElements(llvm::PointerType * const PT);
size_t getNumElements(llvm::OpaqueType * const OT);
const llvm::Value *getBasePointerValue(const llvm::Value *V);
const llvm::Value *getBasePointerValue(const llvm::MachineInstr *MI);


int64_t GET_SCALAR_SIZE(llvm::Type* A);

// Helper functions that check the opcode for status information
bool isLoadInst(llvm::MachineInstr *MI);
bool isExtLoadInst(llvm::MachineInstr *MI);
bool isSWSExtLoadInst(llvm::MachineInstr *MI);
bool isSExtLoadInst(llvm::MachineInstr *MI);
bool isZExtLoadInst(llvm::MachineInstr *MI);
bool isAExtLoadInst(llvm::MachineInstr *MI);
bool isStoreInst(llvm::MachineInstr *MI);
bool isTruncStoreInst(llvm::MachineInstr *MI);
bool isAtomicInst(llvm::MachineInstr *MI);
bool isVolatileInst(llvm::MachineInstr *MI);
bool isGlobalInst(llvm::MachineInstr *MI);
bool isPrivateInst(llvm::MachineInstr *MI);
bool isConstantInst(llvm::MachineInstr *MI);
bool isRegionInst(llvm::MachineInstr *MI);
bool isLocalInst(llvm::MachineInstr *MI);
bool isImageInst(llvm::MachineInstr *MI);
bool isAppendInst(llvm::MachineInstr *MI);
bool isRegionAtomic(llvm::MachineInstr *MI);
bool isLocalAtomic(llvm::MachineInstr *MI);
bool isGlobalAtomic(llvm::MachineInstr *MI);
bool isArenaAtomic(llvm::MachineInstr *MI);


// Macros that are used to help with switch statements for various data types
// However, these macro's do not return anything unlike the second set below.
#define ExpandCaseTo32bitIntTypes(Instr)  \
case Instr##_i8: \
case Instr##_i16: \
case Instr##_i32:

#define ExpandCaseTo32bitIntTruncTypes(Instr)  \
case Instr##_i16i8: \
case Instr##_i32i8: \
case Instr##_i32i16: 

#define ExpandCaseToIntTypes(Instr) \
    ExpandCaseTo32bitIntTypes(Instr) \
case Instr##_i64:

#define ExpandCaseToIntTruncTypes(Instr) \
    ExpandCaseTo32bitIntTruncTypes(Instr) \
case Instr##_i64i8:\
case Instr##_i64i16:\
case Instr##_i64i32:\

#define ExpandCaseToFloatTypes(Instr) \
    case Instr##_f32: \
case Instr##_f64:

#define ExpandCaseToFloatTruncTypes(Instr) \
case Instr##_f64f32:

#define ExpandCaseTo32bitScalarTypes(Instr) \
    ExpandCaseTo32bitIntTypes(Instr) \
case Instr##_f32:

#define ExpandCaseToAllScalarTypes(Instr) \
    ExpandCaseToFloatTypes(Instr) \
ExpandCaseToIntTypes(Instr)

#define ExpandCaseToAllScalarTruncTypes(Instr) \
    ExpandCaseToFloatTruncTypes(Instr) \
ExpandCaseToIntTruncTypes(Instr)

// Vector versions of above macros
#define ExpandCaseToVectorIntTypes(Instr) \
    case Instr##_v2i8: \
case Instr##_v4i8: \
case Instr##_v2i16: \
case Instr##_v4i16: \
case Instr##_v2i32: \
case Instr##_v4i32: \
case Instr##_v2i64:

#define ExpandCaseToVectorIntTruncTypes(Instr) \
case Instr##_v2i16i8: \
case Instr##_v4i16i8: \
case Instr##_v2i32i8: \
case Instr##_v4i32i8: \
case Instr##_v2i32i16: \
case Instr##_v4i32i16: \
case Instr##_v2i64i8: \
case Instr##_v2i64i16: \
case Instr##_v2i64i32: 

#define ExpandCaseToVectorFloatTypes(Instr) \
    case Instr##_v2f32: \
case Instr##_v4f32: \
case Instr##_v2f64:

#define ExpandCaseToVectorFloatTruncTypes(Instr) \
case Instr##_v2f64f32:

#define ExpandCaseToVectorByteTypes(Instr) \
  case Instr##_v4i8:\
case Instr##_v2i16: \
case Instr##_v4i16:

#define ExpandCaseToAllVectorTypes(Instr) \
    ExpandCaseToVectorFloatTypes(Instr) \
ExpandCaseToVectorIntTypes(Instr)

#define ExpandCaseToAllVectorTruncTypes(Instr) \
    ExpandCaseToVectorFloatTruncTypes(Instr) \
ExpandCaseToVectorIntTruncTypes(Instr)

#define ExpandCaseToAllTypes(Instr) \
    ExpandCaseToAllVectorTypes(Instr) \
ExpandCaseToAllScalarTypes(Instr)

#define ExpandCaseToAllTruncTypes(Instr) \
    ExpandCaseToAllVectorTruncTypes(Instr) \
ExpandCaseToAllScalarTruncTypes(Instr)

#define ExpandCaseToPackedTypes(Instr) \
    case Instr##_v2i8: \
    case Instr##_v4i8: \
    case Instr##_v2i16: \
    case Instr##_v4i16:

#define ExpandCaseToByteShortTypes(Instr) \
    case Instr##_i8: \
    case Instr##_i16: \
    ExpandCaseToPackedTypes(Instr)

// Macros that expand into case statements with return values
#define ExpandCaseTo32bitIntReturn(Instr, Return)  \
case Instr##_i8: return Return##_i8;\
case Instr##_i16: return Return##_i16;\
case Instr##_i32: return Return##_i32;

#define ExpandCaseToIntReturn(Instr, Return) \
    ExpandCaseTo32bitIntReturn(Instr, Return) \
case Instr##_i64: return Return##_i64;

#define ExpandCaseToFloatReturn(Instr, Return) \
    case Instr##_f32: return Return##_f32;\
case Instr##_f64: return Return##_f64;

#define ExpandCaseToAllScalarReturn(Instr, Return) \
    ExpandCaseToFloatReturn(Instr, Return) \
ExpandCaseToIntReturn(Instr, Return)

// These macros expand to common groupings of RegClass ID's
#define ExpandCaseTo1CompRegID \
case AMDIL::GPRI8RegClassID: \
case AMDIL::GPRI16RegClassID: \
case AMDIL::GPRI32RegClassID: \
case AMDIL::GPRF32RegClassID:

#define ExpandCaseTo2CompRegID \
    case AMDIL::GPRI64RegClassID: \
case AMDIL::GPRF64RegClassID: \
case AMDIL::GPRV2I8RegClassID: \
case AMDIL::GPRV2I16RegClassID: \
case AMDIL::GPRV2I32RegClassID: \
case AMDIL::GPRV2F32RegClassID:

// Macros that expand to case statements for specific bitlengths
#define ExpandCaseTo8BitType(Instr) \
    case Instr##_i8:

#define ExpandCaseTo16BitType(Instr) \
    case Instr##_v2i8: \
case Instr##_i16:

#define ExpandCaseTo32BitType(Instr) \
    case Instr##_v4i8: \
case Instr##_v2i16: \
case Instr##_i32: \
case Instr##_f32:

#define ExpandCaseTo64BitType(Instr) \
    case Instr##_v4i16: \
case Instr##_v2i32: \
case Instr##_v2f32: \
case Instr##_i64: \
case Instr##_f64:

#define ExpandCaseTo128BitType(Instr) \
    case Instr##_v4i32: \
case Instr##_v4f32: \
case Instr##_v2i64: \
case Instr##_v2f64:

bool commaPrint(int i, OSTREAM_TYPE &O);
/// Helper function to get the currently get/set flags.
void getAsmPrinterFlags(llvm::MachineInstr *MI, llvm::AMDILAS::InstrResEnc &curRes);
void setAsmPrinterFlags(llvm::MachineInstr *MI, llvm::AMDILAS::InstrResEnc &curRes);

#endif // AMDILUTILITYFUNCTIONS_H_
