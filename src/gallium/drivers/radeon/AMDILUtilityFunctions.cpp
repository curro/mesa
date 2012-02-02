//===-- AMDILUtilityFunctions.cpp - AMDIL Utility Functions       ---------===//
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
// to the U.S. Export Administration Regulations (�EAR�), (15 C.F.R. Sections
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
// Industry and Security�s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//
//
// This file provides the implementations of functions that are declared in the
// AMDILUtilityFUnctions.h file.
//
//===----------------------------------------------------------------------===//
#include "AMDILUtilityFunctions.h"
#include "AMDILISelLowering.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ADT/ValueMap.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineInstr.h"
#include <cstdio>
#include <queue>
#include <list>
using namespace llvm;
int64_t GET_SCALAR_SIZE(llvm::Type *A) {
  return A->getScalarSizeInBits();
}

const TargetRegisterClass * getRegClassFromID(unsigned int ID) {
  switch (ID) {
  default:
    assert(0 && "Passed in ID does not match any register classes.");
    return NULL;
  case AMDIL::GPRI8RegClassID:
    return &AMDIL::GPRI8RegClass;
  case AMDIL::GPRI16RegClassID:
    return &AMDIL::GPRI16RegClass;
  case AMDIL::GPRI32RegClassID:
    return &AMDIL::GPRI32RegClass;
  case AMDIL::GPRF32RegClassID:
    return &AMDIL::GPRF32RegClass;
  case AMDIL::GPRI64RegClassID:
    return &AMDIL::GPRI64RegClass;
  case AMDIL::GPRF64RegClassID:
    return &AMDIL::GPRF64RegClass;
  case AMDIL::GPRV4F32RegClassID:
    return &AMDIL::GPRV4F32RegClass;
  case AMDIL::GPRV4I8RegClassID:
    return &AMDIL::GPRV4I8RegClass;
  case AMDIL::GPRV4I16RegClassID:
    return &AMDIL::GPRV4I16RegClass;
  case AMDIL::GPRV4I32RegClassID:
    return &AMDIL::GPRV4I32RegClass;
  case AMDIL::GPRV2F32RegClassID:
    return &AMDIL::GPRV2F32RegClass;
  case AMDIL::GPRV2I8RegClassID:
    return &AMDIL::GPRV2I8RegClass;
  case AMDIL::GPRV2I16RegClassID:
    return &AMDIL::GPRV2I16RegClass;
  case AMDIL::GPRV2I32RegClassID:
    return &AMDIL::GPRV2I32RegClass;
  case AMDIL::GPRV2F64RegClassID:
    return &AMDIL::GPRV2F64RegClass;
  case AMDIL::GPRV2I64RegClassID:
    return &AMDIL::GPRV2I64RegClass;
  };
}

unsigned int getMoveInstFromID(unsigned int ID) {
  switch (ID) {
  default:
    assert(0 && "Passed in ID does not match any move instructions.");
  case AMDIL::GPRI8RegClassID:
    return AMDIL::MOVE_i8;
  case AMDIL::GPRI16RegClassID:
    return AMDIL::MOVE_i16;
  case AMDIL::GPRI32RegClassID:
    return AMDIL::MOVE_i32;
  case AMDIL::GPRF32RegClassID:
    return AMDIL::MOVE_f32;
  case AMDIL::GPRI64RegClassID:
    return AMDIL::MOVE_i64;
  case AMDIL::GPRF64RegClassID:
    return AMDIL::MOVE_f64;
  case AMDIL::GPRV4F32RegClassID:
    return AMDIL::MOVE_v4f32;
  case AMDIL::GPRV4I8RegClassID:
    return AMDIL::MOVE_v4i8;
  case AMDIL::GPRV4I16RegClassID:
    return AMDIL::MOVE_v4i16;
  case AMDIL::GPRV4I32RegClassID:
    return AMDIL::MOVE_v4i32;
  case AMDIL::GPRV2F32RegClassID:
    return AMDIL::MOVE_v2f32;
  case AMDIL::GPRV2I8RegClassID:
    return AMDIL::MOVE_v2i8;
  case AMDIL::GPRV2I16RegClassID:
    return AMDIL::MOVE_v2i16;
  case AMDIL::GPRV2I32RegClassID:
    return AMDIL::MOVE_v2i32;
  case AMDIL::GPRV2F64RegClassID:
    return AMDIL::MOVE_v2f64;
  case AMDIL::GPRV2I64RegClassID:
    return AMDIL::MOVE_v2i64;
  };
  return -1;
}

unsigned int getPHIMoveInstFromID(unsigned int ID) {
  switch (ID) {
  default:
    assert(0 && "Passed in ID does not match any move instructions.");
  case AMDIL::GPRI8RegClassID:
    return AMDIL::PHIMOVE_i8;
  case AMDIL::GPRI16RegClassID:
    return AMDIL::PHIMOVE_i16;
  case AMDIL::GPRI32RegClassID:
    return AMDIL::PHIMOVE_i32;
  case AMDIL::GPRF32RegClassID:
    return AMDIL::PHIMOVE_f32;
  case AMDIL::GPRI64RegClassID:
    return AMDIL::PHIMOVE_i64;
  case AMDIL::GPRF64RegClassID:
    return AMDIL::PHIMOVE_f64;
  case AMDIL::GPRV4F32RegClassID:
    return AMDIL::PHIMOVE_v4f32;
  case AMDIL::GPRV4I8RegClassID:
    return AMDIL::PHIMOVE_v4i8;
  case AMDIL::GPRV4I16RegClassID:
    return AMDIL::PHIMOVE_v4i16;
  case AMDIL::GPRV4I32RegClassID:
    return AMDIL::PHIMOVE_v4i32;
  case AMDIL::GPRV2F32RegClassID:
    return AMDIL::PHIMOVE_v2f32;
  case AMDIL::GPRV2I8RegClassID:
    return AMDIL::PHIMOVE_v2i8;
  case AMDIL::GPRV2I16RegClassID:
    return AMDIL::PHIMOVE_v2i16;
  case AMDIL::GPRV2I32RegClassID:
    return AMDIL::PHIMOVE_v2i32;
  case AMDIL::GPRV2F64RegClassID:
    return AMDIL::PHIMOVE_v2f64;
  case AMDIL::GPRV2I64RegClassID:
    return AMDIL::PHIMOVE_v2i64;
  };
  return -1;
}

const TargetRegisterClass* getRegClassFromType(unsigned int type) {
  switch (type) {
  default:
    assert(0 && "Passed in type does not match any register classes.");
  case MVT::i8:
    return &AMDIL::GPRI8RegClass;
  case MVT::i16:
    return &AMDIL::GPRI16RegClass;
  case MVT::i32:
    return &AMDIL::GPRI32RegClass;
  case MVT::f32:
    return &AMDIL::GPRF32RegClass;
  case MVT::i64:
    return &AMDIL::GPRI64RegClass;
  case MVT::f64:
    return &AMDIL::GPRF64RegClass;
  case MVT::v4f32:
    return &AMDIL::GPRV4F32RegClass;
  case MVT::v4i8:
    return &AMDIL::GPRV4I8RegClass;
  case MVT::v4i16:
    return &AMDIL::GPRV4I16RegClass;
  case MVT::v4i32:
    return &AMDIL::GPRV4I32RegClass;
  case MVT::v2f32:
    return &AMDIL::GPRV2F32RegClass;
  case MVT::v2i8:
    return &AMDIL::GPRV2I8RegClass;
  case MVT::v2i16:
    return &AMDIL::GPRV2I16RegClass;
  case MVT::v2i32:
    return &AMDIL::GPRV2I32RegClass;
  case MVT::v2f64:
    return &AMDIL::GPRV2F64RegClass;
  case MVT::v2i64:
    return &AMDIL::GPRV2I64RegClass;
  }
}

void printSDNode(const SDNode *N) {
  printf("Opcode: %d isTargetOpcode: %d isMachineOpcode: %d\n",
         N->getOpcode(), N->isTargetOpcode(), N->isMachineOpcode());
  printf("Empty: %d OneUse: %d Size: %d NodeID: %d\n",
         N->use_empty(), N->hasOneUse(), (int)N->use_size(), N->getNodeId());
  for (unsigned int i = 0; i < N->getNumOperands(); ++i) {
    printf("OperandNum: %d ValueCount: %d ValueType: %d\n",
           i, N->getNumValues(), N->getValueType(0) .getSimpleVT().SimpleTy);
    printSDValue(N->getOperand(i), 0);
  }
}

void printSDValue(const SDValue &Op, int level) {
  printf("\nOp: %p OpCode: %d NumOperands: %d ", &Op, Op.getOpcode(),
         Op.getNumOperands());
  printf("IsTarget: %d IsMachine: %d ", Op.isTargetOpcode(),
         Op.isMachineOpcode());
  if (Op.isMachineOpcode()) {
    printf("MachineOpcode: %d\n", Op.getMachineOpcode());
  } else {
    printf("\n");
  }
  EVT vt = Op.getValueType();
  printf("ValueType: %d \n", vt.getSimpleVT().SimpleTy);
  printf("UseEmpty: %d OneUse: %d\n", Op.use_empty(), Op.hasOneUse());
  if (level) {
    printf("Children for %d:\n", level);
    for (unsigned int i = 0; i < Op.getNumOperands(); ++i) {
      printf("Child %d->%d:", level, i);
      printSDValue(Op.getOperand(i), level - 1);
    }
  }
}

bool isPHIMove(unsigned int opcode) {
  switch (opcode) {
  default:
    return false;
    ExpandCaseToAllTypes(AMDIL::PHIMOVE);
    return true;
  }
  return false;
}

bool isMove(unsigned int opcode) {
  switch (opcode) {
  default:
    return false;
    ExpandCaseToAllTypes(AMDIL::MOVE);
    return true;
  }
  return false;
}

bool isMoveOrEquivalent(unsigned int opcode) {
  switch (opcode) {
  default:
    return isMove(opcode) || isPHIMove(opcode);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASCHAR);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASSHORT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASINT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASLONG);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASDOUBLE);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASFLOAT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2CHAR);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2SHORT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2INT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2FLOAT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2LONG);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV2DOUBLE);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV4CHAR);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV4SHORT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV4INT);
    ExpandCaseToAllScalarTypes(AMDIL::IL_ASV4FLOAT);
    case AMDIL::INTTOANY_i8:
    case AMDIL::INTTOANY_i16:
    case AMDIL::INTTOANY_i32:
    case AMDIL::INTTOANY_f32:
    case AMDIL::DLO:
    case AMDIL::LLO:
    case AMDIL::LLO_v2i64:
      return true;
  };
  return false;
}

bool check_type(const Value *ptr, unsigned int addrspace) {
  if (!ptr) {
    return false;
  }
  Type *ptrType = ptr->getType();
  return dyn_cast<PointerType>(ptrType)->getAddressSpace() == addrspace;
}

size_t getTypeSize(Type * const T, bool dereferencePtr) {
  size_t size = 0;
  if (!T) {
    return size;
  }
  switch (T->getTypeID()) {
  case Type::X86_FP80TyID:
  case Type::FP128TyID:
  case Type::PPC_FP128TyID:
  case Type::LabelTyID:
    assert(0 && "These types are not supported by this backend");
  default:
  case Type::FloatTyID:
  case Type::DoubleTyID:
    size = T->getPrimitiveSizeInBits() >> 3;
    break;
  case Type::PointerTyID:
    size = getTypeSize(dyn_cast<PointerType>(T), dereferencePtr);
    break;
  case Type::IntegerTyID:
    size = getTypeSize(dyn_cast<IntegerType>(T), dereferencePtr);
    break;
  case Type::StructTyID:
    size = getTypeSize(dyn_cast<StructType>(T), dereferencePtr);
    break;
  case Type::ArrayTyID:
    size = getTypeSize(dyn_cast<ArrayType>(T), dereferencePtr);
    break;
  case Type::FunctionTyID:
    size = getTypeSize(dyn_cast<FunctionType>(T), dereferencePtr);
    break;
  case Type::VectorTyID:
    size = getTypeSize(dyn_cast<VectorType>(T), dereferencePtr);
    break;
  };
  return size;
}

size_t getTypeSize(StructType * const ST, bool dereferencePtr) {
  size_t size = 0;
  if (!ST) {
    return size;
  }
  Type *curType;
  StructType::element_iterator eib;
  StructType::element_iterator eie;
  for (eib = ST->element_begin(), eie = ST->element_end(); eib != eie; ++eib) {
    curType = *eib;
    size += getTypeSize(curType, dereferencePtr);
  }
  return size;
}

size_t getTypeSize(IntegerType * const IT, bool dereferencePtr) {
  return IT ? (IT->getBitWidth() >> 3) : 0;
}

size_t getTypeSize(FunctionType * const FT, bool dereferencePtr) {
    assert(0 && "Should not be able to calculate the size of an function type");
    return 0;
}

size_t getTypeSize(ArrayType * const AT, bool dereferencePtr) {
  return (size_t)(AT ? (getTypeSize(AT->getElementType(),
                                    dereferencePtr) * AT->getNumElements())
                     : 0);
}

size_t getTypeSize(VectorType * const VT, bool dereferencePtr) {
  return VT ? (VT->getBitWidth() >> 3) : 0;
}

size_t getTypeSize(PointerType * const PT, bool dereferencePtr) {
  if (!PT) {
    return 0;
  }
  Type *CT = PT->getElementType();
  if (CT->getTypeID() == Type::StructTyID &&
      PT->getAddressSpace() == AMDILAS::PRIVATE_ADDRESS) {
    return getTypeSize(dyn_cast<StructType>(CT));
  } else if (dereferencePtr) {
    size_t size = 0;
    for (size_t x = 0, y = PT->getNumContainedTypes(); x < y; ++x) {
      size += getTypeSize(PT->getContainedType(x), dereferencePtr);
    }
    return size;
  } else {
    return 4;
  }
}

size_t getTypeSize(OpaqueType * const OT, bool dereferencePtr) {
  //assert(0 && "Should not be able to calculate the size of an opaque type");
  return 4;
}

size_t getNumElements(Type * const T) {
  size_t size = 0;
  if (!T) {
    return size;
  }
  switch (T->getTypeID()) {
  case Type::X86_FP80TyID:
  case Type::FP128TyID:
  case Type::PPC_FP128TyID:
  case Type::LabelTyID:
    assert(0 && "These types are not supported by this backend");
  default:
  case Type::FloatTyID:
  case Type::DoubleTyID:
    size = 1;
    break;
  case Type::PointerTyID:
    size = getNumElements(dyn_cast<PointerType>(T));
    break;
  case Type::IntegerTyID:
    size = getNumElements(dyn_cast<IntegerType>(T));
    break;
  case Type::StructTyID:
    size = getNumElements(dyn_cast<StructType>(T));
    break;
  case Type::ArrayTyID:
    size = getNumElements(dyn_cast<ArrayType>(T));
    break;
  case Type::FunctionTyID:
    size = getNumElements(dyn_cast<FunctionType>(T));
    break;
  case Type::VectorTyID:
    size = getNumElements(dyn_cast<VectorType>(T));
    break;
  };
  return size;
}

size_t getNumElements(StructType * const ST) {
  size_t size = 0;
  if (!ST) {
    return size;
  }
  Type *curType;
  StructType::element_iterator eib;
  StructType::element_iterator eie;
  for (eib = ST->element_begin(), eie = ST->element_end();
       eib != eie; ++eib) {
    curType = *eib;
    size += getNumElements(curType);
  }
  return size;
}

size_t getNumElements(IntegerType * const IT) {
  return (!IT) ? 0 : 1;
}

size_t getNumElements(FunctionType * const FT) {
  assert(0 && "Should not be able to calculate the number of "
         "elements of a function type");
  return 0;
}

size_t getNumElements(ArrayType * const AT) {
  return (!AT) ? 0
               :  (size_t)(getNumElements(AT->getElementType()) *
                           AT->getNumElements());
}

size_t getNumElements(VectorType * const VT) {
  return (!VT) ? 0
               : VT->getNumElements() * getNumElements(VT->getElementType());
}

size_t getNumElements(PointerType * const PT) {
  size_t size = 0;
  if (!PT) {
    return size;
  }
  for (size_t x = 0, y = PT->getNumContainedTypes(); x < y; ++x) {
    size += getNumElements(PT->getContainedType(x));
  }
  return size;
}

const llvm::Value *getBasePointerValue(const llvm::Value *V)
{
  if (!V) {
    return NULL;
  }
  const Value *ret = NULL;
  ValueMap<const Value *, bool> ValueBitMap;
  std::queue<const Value *, std::list<const Value *> > ValueQueue;
  ValueQueue.push(V);
  while (!ValueQueue.empty()) {
    V = ValueQueue.front();
    if (ValueBitMap.find(V) == ValueBitMap.end()) {
      ValueBitMap[V] = true;
      if (dyn_cast<Argument>(V) && dyn_cast<PointerType>(V->getType())) {
        ret = V;
        break;
      } else if (dyn_cast<GlobalVariable>(V)) {
        ret = V;
        break;
      } else if (dyn_cast<Constant>(V)) {
        const ConstantExpr *CE = dyn_cast<ConstantExpr>(V);
        if (CE) {
          ValueQueue.push(CE->getOperand(0));
        }
      } else if (const AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
        ret = AI;
        break;
      } else if (const Instruction *I = dyn_cast<Instruction>(V)) {
        uint32_t numOps = I->getNumOperands();
        for (uint32_t x = 0; x < numOps; ++x) {
          ValueQueue.push(I->getOperand(x));
        }
      } else {
        // assert(0 && "Found a Value that we didn't know how to handle!");
      }
    }
    ValueQueue.pop();
  }
  return ret;
}

const llvm::Value *getBasePointerValue(const llvm::MachineInstr *MI) {
  const Value *moVal = NULL;
  if (!MI->memoperands_empty()) {
    const MachineMemOperand *memOp = (*MI->memoperands_begin());
    moVal = memOp ? memOp->getValue() : NULL;
    moVal = getBasePointerValue(moVal);
  }
  return moVal;
}

bool commaPrint(int i, OSTREAM_TYPE &O) {
  O << ":" << i;
  return false;
}

bool isLoadInst(MachineInstr *MI) {
  if (strstr(MI->getDesc().getName(), "LOADCONST")) {
    return false;
  }
  return strstr(MI->getDesc().getName(), "LOAD");
}

bool isSWSExtLoadInst(MachineInstr *MI)
{
switch (MI->getOpcode()) {
    default:
      break;
      ExpandCaseToByteShortTypes(AMDIL::LOCALLOAD);
      ExpandCaseToByteShortTypes(AMDIL::GLOBALLOAD);
      ExpandCaseToByteShortTypes(AMDIL::REGIONLOAD);
      ExpandCaseToByteShortTypes(AMDIL::PRIVATELOAD);
      ExpandCaseToByteShortTypes(AMDIL::CPOOLLOAD);
      ExpandCaseToByteShortTypes(AMDIL::CONSTANTLOAD);
      return true;
  };
  return false;
}

bool isExtLoadInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "EXTLOAD");
}

bool isSExtLoadInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "SEXTLOAD");
}

bool isAExtLoadInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "AEXTLOAD");
}

bool isZExtLoadInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "ZEXTLOAD");
}

bool isStoreInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "STORE");
}

bool isTruncStoreInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "TRUNCSTORE");
}

bool isAtomicInst(MachineInstr *MI) {
  return strstr(MI->getDesc().getName(), "ATOM");
}

bool isVolatileInst(MachineInstr *MI) {
  if (!MI->memoperands_empty()) {
    for (MachineInstr::mmo_iterator mob = MI->memoperands_begin(),
        moe = MI->memoperands_end(); mob != moe; ++mob) {
      // If there is a volatile mem operand, this is a volatile instruction.
      if ((*mob)->isVolatile()) {
        return true;
      }
    }
  }
  return false;
}
bool isGlobalInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "GLOBAL");
}
bool isPrivateInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "PRIVATE");
}
bool isConstantInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "CONSTANT")
    || strstr(MI->getDesc().getName(), "CPOOL");
}
bool isRegionInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "REGION");
}
bool isLocalInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "LOCAL");
}
bool isImageInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "IMAGE");
}
bool isAppendInst(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "APPEND");
}
bool isRegionAtomic(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "ATOM_R");
}
bool isLocalAtomic(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "ATOM_L");
}
bool isGlobalAtomic(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "ATOM_G")
    || isArenaAtomic(MI);
}
bool isArenaAtomic(llvm::MachineInstr *MI)
{
  return strstr(MI->getDesc().getName(), "ATOM_A");
}

const char* getSrcSwizzle(unsigned idx) {
  const char *srcSwizzles[]  = {
    "", ".x000", ".0x00", ".00x0", ".000x", ".y000", ".0y00", ".00y0", ".000y", 
    ".z000", ".0z00", ".00z0", ".000z", ".w000", ".0w00", ".00w0", ".000w",
    ".xy00", ".00xy", ".zw00", ".00zw", ".xyz0", ".0xyz", ".xyzw", ".0000",
    ".xxxx", ".yyyy", ".zzzz", ".wwww", ".xyxy", ".zwzw", ".xzxz", ".ywyw",
    ".x0y0", ".0x0y", ".xy_neg(y)", "_neg(yw)", "_neg(x)", ".xy_neg(xy)",
    "_neg(xyzw)", ".0yzw", ".x0zw", ".xy0w", ".x", ".y", ".z", ".w", ".xy",
    ".zw"
  };
  assert(idx < sizeof(srcSwizzles)/sizeof(srcSwizzles[0])
      && "Idx passed in is invalid!");
  return srcSwizzles[idx];
}
const char* getDstSwizzle(unsigned idx) {
  const char *dstSwizzles[] = {
    "", ".x___", ".xy__", ".xyz_", ".xyzw", "._y__", "._yz_", "._yzw", ".__z_",
    ".__zw", ".___w", ".x_zw", ".xy_w", ".x_z_", ".x__w", "._y_w", 
  };
  assert(idx < sizeof(dstSwizzles)/sizeof(dstSwizzles[0])
      && "Idx passed in is invalid!");
  return dstSwizzles[idx];
}
/// Helper function to get the currently set flags
void getAsmPrinterFlags(MachineInstr *MI, AMDILAS::InstrResEnc &curRes)
{
#if LLVM_VERSION < 2500
  curRes.u16all = MI->getAsmPrinterFlags();
#else
  // We need 16 bits of information, but LLVMr127097 cut the field in half.
  // So we have to use two different fields to store all of our information.
  uint16_t upper = MI->getFlags() << 8;
  uint16_t lower = MI->getAsmPrinterFlags();
  curRes.u16all = upper | lower;
#endif  
}
/// Helper function to clear the currently set flags and add the new flags.
void setAsmPrinterFlags(MachineInstr *MI, AMDILAS::InstrResEnc &curRes)
{
#if LLVM_VERSION < 2500
  MI->clearAsmPrinterFlags();
  MI->setAsmPrinterFlag((llvm::MachineInstr::CommentFlag)curRes.u16all);
#else 
  // We need 16 bits of information, but LLVMr127097 cut the field in half.
  // So we have to use two different fields to store all of our information.
  MI->clearAsmPrinterFlags();
  MI->setFlags(0);
  uint8_t lower = curRes.u16all & 0xFF;
  uint8_t upper = (curRes.u16all >> 8) & 0xFF;
  MI->setFlags(upper);
  MI->setAsmPrinterFlag((llvm::MachineInstr::CommentFlag)lower);
#endif
}
