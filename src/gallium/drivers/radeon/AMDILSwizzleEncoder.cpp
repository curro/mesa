//===-------- AMDILSwizzleEncoder.cpp - Encode the swizzle information ----===//
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
// The implementation of the AMDIL Swizzle Encoder. The swizzle encoder goes
// through all instructions in a machine function and all operands and
// encodes swizzle information in the operands. The AsmParser can then
// use the swizzle information to print out the swizzles correctly.
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "SwizzleEncoder"
#if !defined(NDEBUG)
#define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
#define DEBUGME (false)
#endif
#include "AMDILSwizzleEncoder.h"
#include "AMDILAlgorithms.tpp"
#include "AMDILUtilityFunctions.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;
/// Encode all of the swizzles for the instructions in the machine operand.
static void encodeSwizzles(MachineFunction &MF, bool mDebug, 
    const AMDILTargetMachine *ATM);
#if 0
static void allocateSwizzles(MachineFunction &MF, bool mDebug);
static void allocateDstOperands(MachineFunction &MF, 
    std::map<unsigned, unsigned> &scalars,
    std::map<unsigned, unsigned> &doubles,
    bool mDebug);
static void allocateSrcOperands(MachineFunction &MF, 
    std::map<unsigned, unsigned> &scalars,
    std::map<unsigned, unsigned> &doubles,
    bool mDebug);
#endif
/// Get the swizzle id for the src swizzle that corresponds to the
/// current operand.
static OpSwizzle getSrcSwizzleID(MachineInstr *MI, unsigned opNum,
    const AMDILTargetMachine *ATM);

/// Get the swizzle id for the dst swizzle that corresponds to the
/// current instruction.
static OpSwizzle getDstSwizzleID(MachineInstr *MI, 
    const AMDILTargetMachine *ATM);

/// Determine if the custom source swizzle or the
/// default swizzle for the specified operand should be used.
static bool isCustomSrcInst(MachineInstr *MI, unsigned opNum);

/// Get the custom source swizzle that corresponds to the specified
/// operand for the instruction.
static OpSwizzle getCustomSrcSwizzle(MachineInstr *MI, unsigned opNum);

/// Determine if the custom destination swizzle or the
/// default swizzle should be used for the instruction.
static bool isCustomDstInst(MachineInstr *MI);

/// Get the custom destination swizzle that corresponds tothe
/// instruction.
static OpSwizzle getCustomDstSwizzle(MachineInstr *MI);

/// Determine if the instruction is a custom vector instruction
/// that needs a unique swizzle type.
static bool isCustomVectorInst(MachineInstr *MI);

/// Encode the new swizzle for the vector instruction.
static void encodeVectorInst(MachineInstr *MI,
    const AMDILTargetMachine *STM, bool mDebug);
#if 0
static void dumpSwizzle(OpSwizzle);
#endif
/// Helper function to dump the operand for the machine instruction
/// and the relevant target flags.
static void dumpOperand(MachineInstr *MI, unsigned opNum);
#if 0
static void propogateSrcSwizzles(MachineOperand *MO, unsigned idx, bool mDebug);
#endif
namespace llvm {
  FunctionPass*
    createAMDILSwizzleEncoder(TargetMachine &TM, CodeGenOpt::Level OptLevel)
    {
      return new AMDILSwizzleEncoder(TM, OptLevel);
    }
}

AMDILSwizzleEncoder::AMDILSwizzleEncoder(TargetMachine &tm, 
                                         CodeGenOpt::Level OptLevel) :
#if LLVM_VERSION >= 2500
  MachineFunctionPass(ID)
#else
  MachineFunctionPass((intptr_t)&ID)
#endif
{
  ATM = reinterpret_cast<const AMDILTargetMachine*>(&tm);
  mDebug = DEBUGME;
  opt = OptLevel;
}

const char* AMDILSwizzleEncoder::getPassName() const
{
  return "AMD IL Swizzle Encoder Pass";
}

bool AMDILSwizzleEncoder::runOnMachineFunction(MachineFunction &MF)
{
  // Encode swizzles in instruction operands.
  encodeSwizzles(MF, mDebug, ATM);
#if 0
  // pack the swizzles into vector lanes for 
  // more efficient code generation
  const char *packSwizz = getenv("GPU_PACK_SWIZZLES");
  if (packSwizz && packSwizz[0] == '1') {
    if (opt != CodeGenOpt::None) {
      if (mDebug) {
        dbgs() << "//--------------------------- Packing Stage "
          << "---------------------------//\n";
      }
      allocateSwizzles(MF, mDebug);
    }
  }
#endif
  return true;
}
#if 0
void dumpSwizzle(OpSwizzle swizID) {
  dbgs() << "\t" << (swizID.bits.dst ? "Dst" : "Src")
      << " Operand SwizID: " 
      << (unsigned)swizID.bits.swizzle
      << " Swizzle: " << (swizID.bits.dst 
          ? getDstSwizzle(swizID.bits.swizzle) 
          : getSrcSwizzle(swizID.bits.swizzle)) << "\n";

}
#endif
/// Dump the operand swizzle information to the dbgs() stream.
void dumpOperand(MachineInstr *MI, unsigned opNum)
{
  OpSwizzle swizID;
  swizID.u8all = MI->getOperand(opNum).getTargetFlags();
  dbgs() << "\t" << (swizID.bits.dst ? "Dst" : "Src")
      << " Operand: " << opNum << " SwizID: " 
      << (unsigned)swizID.bits.swizzle
      << " Swizzle: " << (swizID.bits.dst 
          ? getDstSwizzle(swizID.bits.swizzle) 
          : getSrcSwizzle(swizID.bits.swizzle)) << "\n";

}
// This function checks for instructions that don't have
// normal swizzle patterns to their source operands. These have to be
// handled on a case by case basis.
bool isCustomSrcInst(MachineInstr *MI, unsigned opNum) {
  unsigned opcode = MI->getOpcode();
  switch (opcode) {
    default:
      if ((opcode >= AMDIL::VEXTRACT_v2f32 &&
          opcode <= AMDIL::VINSERT_v4i8)) {
        return true;
      }
      break;
    case AMDIL::LDSLOAD:
    case AMDIL::LDSLOAD_i8:
    case AMDIL::LDSLOAD_u8:
    case AMDIL::LDSLOAD_i16:
    case AMDIL::LDSLOAD_u16:
    case AMDIL::LDSSTORE:
    case AMDIL::LDSSTORE_i8:
    case AMDIL::LDSSTORE_i16:
    case AMDIL::LDSLOAD_Y:
    case AMDIL::LDSSTORE_Y:
    case AMDIL::LDSLOAD_Z:
    case AMDIL::LDSSTORE_Z:
    case AMDIL::LDSLOAD_W:
    case AMDIL::LDSSTORE_W:
    case AMDIL::GDSLOAD:
    case AMDIL::GDSSTORE:
    case AMDIL::GDSLOAD_Y:
    case AMDIL::GDSSTORE_Y:
    case AMDIL::GDSLOAD_Z:
    case AMDIL::GDSSTORE_Z:
    case AMDIL::GDSLOAD_W:
    case AMDIL::GDSSTORE_W:
    case AMDIL::SCRATCHLOAD:
    case AMDIL::CBLOAD:
    case AMDIL::SCRATCHSTORE:
    case AMDIL::SCRATCHSTORE_X:
    case AMDIL::SCRATCHSTORE_Y:
    case AMDIL::SCRATCHSTORE_Z:
    case AMDIL::SCRATCHSTORE_W:
    case AMDIL::SCRATCHSTORE_XY:
    case AMDIL::SCRATCHSTORE_ZW:
    case AMDIL::UAVARENALOAD_i8:
    case AMDIL::UAVARENALOAD_i16:
    case AMDIL::UAVARENALOAD_i32:
    case AMDIL::UAVARENASTORE_i8:
    case AMDIL::UAVARENASTORE_i16:
    case AMDIL::UAVARENASTORE_i32:
    case AMDIL::UAVARENALOAD_Y_i32:
    case AMDIL::UAVARENALOAD_Z_i32:
    case AMDIL::UAVARENALOAD_W_i32:
    case AMDIL::UAVARENASTORE_Y_i32:
    case AMDIL::UAVARENASTORE_Z_i32:
    case AMDIL::UAVARENASTORE_W_i32:
      return true;
    case AMDIL::CMOVLOG_Y_i32:
    case AMDIL::CMOVLOG_Z_i32:
    case AMDIL::CMOVLOG_W_i32:
    case AMDIL::CMOVLOG_f64:
    case AMDIL::CMOVLOG_i64:
      return (opNum == 1) ? true : false;
    case AMDIL::SUB_f64:
    case AMDIL::SUB_v2f64:
      return (opNum == 2) ? true : false;
    case AMDIL::APPEND_CONSUME:
    case AMDIL::APPEND_CONSUME_NORET:
    case AMDIL::APPEND_ALLOC:
    case AMDIL::APPEND_ALLOC_NORET:
    case AMDIL::LLO:
    case AMDIL::LLO_v2i64:
    case AMDIL::LHI:
    case AMDIL::LHI_v2i64:
    case AMDIL::LCREATE:
    case AMDIL::LCREATE_v2i64:
    case AMDIL::LNEGATE:
    case AMDIL::LNEGATE_v2i64:
    case AMDIL::CALL:
    case AMDIL::RETURN:
    case AMDIL::RETDYN:
    case AMDIL::NEG_f32:
    case AMDIL::NEG_v2f32:
    case AMDIL::NEG_v4f32:
    case AMDIL::NEG_f64:
    case AMDIL::NEG_v2f64:
    case AMDIL::DHI:
    case AMDIL::DLO:
    case AMDIL::DCREATE:
    case AMDIL::DHI_v2f64:
    case AMDIL::DLO_v2f64:
    case AMDIL::DCREATE_v2f64:
    case AMDIL::ADDri:
    case AMDIL::ADDir:
    case AMDIL::HILO_BITOR_v2i32:
    case AMDIL::HILO_BITOR_v4i16:
    case AMDIL::HILO_BITOR_v2i64:
    case AMDIL::CONTINUE_LOGICALNZ_f64:
    case AMDIL::BREAK_LOGICALNZ_f64:
    case AMDIL::IF_LOGICALNZ_f64:
    case AMDIL::CONTINUE_LOGICALZ_f64:
    case AMDIL::BREAK_LOGICALZ_f64:
    case AMDIL::IF_LOGICALZ_f64:
    case AMDIL::CONTINUE_LOGICALNZ_i64:
    case AMDIL::BREAK_LOGICALNZ_i64:
    case AMDIL::IF_LOGICALNZ_i64:
    case AMDIL::CONTINUE_LOGICALZ_i64:
    case AMDIL::BREAK_LOGICALZ_i64:
    case AMDIL::IF_LOGICALZ_i64:
    case AMDIL::SWITCH:
      return true;
    case AMDIL::UBIT_INSERT_i32:
    case AMDIL::UBIT_INSERT_v2i32:
    case AMDIL::UBIT_INSERT_v4i32:
      return (opNum == 1 || opNum == 2);
  };
  return !strncmp(MI->getDesc().getName(), "MACRO", 5);
}

// This function returns the OpSwizzle with the custom swizzle set
// correclty for source operands.
OpSwizzle getCustomSrcSwizzle(MachineInstr *MI, unsigned opNum) {
  OpSwizzle opSwiz;
  opSwiz.u8all = 0;
  if (!strncmp(MI->getDesc().getName(), "MACRO", 5)) {
    return opSwiz;
  }
  unsigned opcode = MI->getOpcode();
  switch (opcode) {
    default:
      if ((opcode >= AMDIL::VEXTRACT_v2f32 &&
          opcode <= AMDIL::VINSERT_v4i8)) {
        opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_DEFAULT;
      } 
      break;
    case AMDIL::SCRATCHSTORE:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::SCRATCHLOAD:
    case AMDIL::CBLOAD:
    case AMDIL::SCRATCHSTORE_X:
    case AMDIL::SCRATCHSTORE_Y:
    case AMDIL::SCRATCHSTORE_Z:
    case AMDIL::SCRATCHSTORE_W:
    case AMDIL::UAVARENALOAD_i8:
    case AMDIL::UAVARENALOAD_i16:
    case AMDIL::UAVARENALOAD_i32:
    case AMDIL::UAVARENASTORE_i8:
    case AMDIL::UAVARENASTORE_i16:
    case AMDIL::UAVARENASTORE_i32:
    case AMDIL::LDSLOAD:
    case AMDIL::LDSLOAD_i8:
    case AMDIL::LDSLOAD_u8:
    case AMDIL::LDSLOAD_i16:
    case AMDIL::LDSLOAD_u16:
    case AMDIL::LDSSTORE:
    case AMDIL::LDSSTORE_i8:
    case AMDIL::LDSSTORE_i16:
    case AMDIL::GDSLOAD:
    case AMDIL::GDSSTORE:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_X : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::LDSLOAD_Y:
    case AMDIL::LDSSTORE_Y:
    case AMDIL::GDSLOAD_Y:
    case AMDIL::GDSSTORE_Y:
    case AMDIL::UAVARENALOAD_Y_i32:
    case AMDIL::UAVARENASTORE_Y_i32:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_Y : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::LDSLOAD_Z:
    case AMDIL::LDSSTORE_Z:
    case AMDIL::GDSLOAD_Z:
    case AMDIL::GDSSTORE_Z:
    case AMDIL::UAVARENALOAD_Z_i32:
    case AMDIL::UAVARENASTORE_Z_i32:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_Z : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::LDSLOAD_W:
    case AMDIL::LDSSTORE_W:
    case AMDIL::GDSLOAD_W:
    case AMDIL::GDSSTORE_W:
    case AMDIL::UAVARENALOAD_W_i32:
    case AMDIL::UAVARENASTORE_W_i32:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_W : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::SCRATCHSTORE_XY:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_XY00 : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::SCRATCHSTORE_ZW:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_00XY : AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::APPEND_CONSUME:
    case AMDIL::APPEND_CONSUME_NORET:
    case AMDIL::APPEND_ALLOC:
    case AMDIL::APPEND_ALLOC_NORET:
    case AMDIL::ADDri:
    case AMDIL::ADDir:
    case AMDIL::CALL:
    case AMDIL::RETURN:
    case AMDIL::RETDYN:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_DEFAULT;
      break;
    case AMDIL::CMOVLOG_Y_i32:
      assert(opNum == 1 && "Only operand number 1 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_YYYY;
      break;
    case AMDIL::CMOVLOG_Z_i32:
      assert(opNum == 1 && "Only operand number 1 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_ZZZZ;
      break;
    case AMDIL::CMOVLOG_W_i32:
      assert(opNum == 1 && "Only operand number 1 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_WWWW;
      break;
    case AMDIL::CMOVLOG_f64:
    case AMDIL::CMOVLOG_i64:
      assert(opNum == 1 && "Only operand number 1 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XXXX;
      break;
    case AMDIL::DHI:
    case AMDIL::LLO:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_X000;
      break;
    case AMDIL::DHI_v2f64:
    case AMDIL::LLO_v2i64:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XZXZ;
      break;
    case AMDIL::DLO:
    case AMDIL::LHI:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_Y000;
      break;
    case AMDIL::DLO_v2f64:
    case AMDIL::LHI_v2i64:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_YWYW;
      break;
    case AMDIL::DCREATE:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_0X00 : AMDIL_SRC_SWIZZLE_X000;
      break;
    case AMDIL::DCREATE_v2f64:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_0X0Y : AMDIL_SRC_SWIZZLE_X0Y0; 
      break;
    case AMDIL::LCREATE:
      opSwiz.bits.swizzle = opNum;
      break;
    case AMDIL::LCREATE_v2i64:
      opSwiz.bits.swizzle = opNum + 32;
      break;
    case AMDIL::SUB_f64:
      assert(opNum == 2 && "Only operand number 2 is custom!");
    case AMDIL::NEG_f64:
    case AMDIL::LNEGATE:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XY_NEGY;;
      break;
    case AMDIL::SUB_v2f64:
      assert(opNum == 2 && "Only operand number 2 is custom!");
    case AMDIL::NEG_v2f64:
    case AMDIL::LNEGATE_v2i64:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_NEGYW;
      break;
    case AMDIL::NEG_f32:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_NEGX;
      break;
    case AMDIL::NEG_v2f32:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XY_NEGXY;
      break;
    case AMDIL::NEG_v4f32:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_NEG_XYZW;
      break;
    case AMDIL::SWITCH:
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_X;
      break;
    case AMDIL::CONTINUE_LOGICALNZ_f64:
    case AMDIL::BREAK_LOGICALNZ_f64:
    case AMDIL::IF_LOGICALNZ_f64:
    case AMDIL::CONTINUE_LOGICALZ_f64:
    case AMDIL::BREAK_LOGICALZ_f64:
    case AMDIL::IF_LOGICALZ_f64:
    case AMDIL::CONTINUE_LOGICALNZ_i64:
    case AMDIL::BREAK_LOGICALNZ_i64:
    case AMDIL::IF_LOGICALNZ_i64:
    case AMDIL::CONTINUE_LOGICALZ_i64:
    case AMDIL::BREAK_LOGICALZ_i64:
    case AMDIL::IF_LOGICALZ_i64:
      assert(opNum == 0
          && "Only operand numbers 0 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_X;
      break;
    case AMDIL::UBIT_INSERT_i32:
      assert((opNum == 1 || opNum == 2)
          && "Only operand numbers 1 or 2 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_X;
      break;
    case AMDIL::UBIT_INSERT_v2i32:
      assert((opNum == 1 || opNum == 2)
          && "Only operand numbers 1 or 2 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XY;
      break;
    case AMDIL::UBIT_INSERT_v4i32:
      assert((opNum == 1 || opNum == 2)
          && "Only operand numbers 1 or 2 is custom!");
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XYZW;
      break;
    case AMDIL::HILO_BITOR_v4i16:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_XZXZ : AMDIL_SRC_SWIZZLE_YWYW;
      break;
    case AMDIL::HILO_BITOR_v2i32:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_X000 : AMDIL_SRC_SWIZZLE_Y000;
      break;
    case AMDIL::HILO_BITOR_v2i64:
      opSwiz.bits.swizzle = (opNum == 1) 
        ? AMDIL_SRC_SWIZZLE_XY00 : AMDIL_SRC_SWIZZLE_ZW00;
      break;
  };
  return opSwiz;
}

// This function checks for instructions that don't have 
// normal swizzle patterns to their destination operand.
// These have to be handled on a case by case basis.
bool isCustomDstInst(MachineInstr *MI) {
  unsigned opcode = MI->getOpcode();
  if ((opcode >= AMDIL::VEXTRACT_v2f32 &&
        opcode <= AMDIL::VINSERT_v4i8)) {
    return true;
  }
  if ((opcode >= AMDIL::VCREATE_v2f32 &&
        opcode <= AMDIL::VCREATE_v4i8) ||
      (opcode >= AMDIL::LOADCONST_f32 &&
       opcode <= AMDIL::LOADCONST_i8)) {
    return true;
  }
  switch (opcode) {
    default:
      break;
    case AMDIL::UAVARENASTORE_i8:
    case AMDIL::UAVARENASTORE_i16:
    case AMDIL::UAVARENASTORE_i32:
    case AMDIL::UAVARENASTORE_Y_i32:
    case AMDIL::UAVARENASTORE_Z_i32:
    case AMDIL::UAVARENASTORE_W_i32:
    case AMDIL::UAVARENALOAD_i8:
    case AMDIL::UAVARENALOAD_i16:
    case AMDIL::UAVARENALOAD_i32:
    case AMDIL::UAVARENALOAD_Y_i32:
    case AMDIL::UAVARENALOAD_Z_i32:
    case AMDIL::UAVARENALOAD_W_i32:
    case AMDIL::LDSLOAD:
    case AMDIL::LDSLOAD_i8:
    case AMDIL::LDSLOAD_u8:
    case AMDIL::LDSLOAD_i16:
    case AMDIL::LDSLOAD_u16:
    case AMDIL::LDSSTORE:
    case AMDIL::LDSSTORE_i8:
    case AMDIL::LDSSTORE_i16:
    case AMDIL::LDSLOAD_Y:
    case AMDIL::LDSSTORE_Y:
    case AMDIL::LDSLOAD_Z:
    case AMDIL::LDSSTORE_Z:
    case AMDIL::LDSLOAD_W:
    case AMDIL::LDSSTORE_W:
    case AMDIL::GDSLOAD:
    case AMDIL::GDSSTORE:
    case AMDIL::GDSLOAD_Y:
    case AMDIL::GDSSTORE_Y:
    case AMDIL::GDSLOAD_Z:
    case AMDIL::GDSSTORE_Z:
    case AMDIL::GDSLOAD_W:
    case AMDIL::GDSSTORE_W:
    case AMDIL::SCRATCHSTORE:
    case AMDIL::SCRATCHSTORE_X:
    case AMDIL::SCRATCHSTORE_Y:
    case AMDIL::SCRATCHSTORE_Z:
    case AMDIL::SCRATCHSTORE_W:
    case AMDIL::SCRATCHSTORE_XY:
    case AMDIL::SCRATCHSTORE_ZW:
    case AMDIL::APPEND_CONSUME:
    case AMDIL::APPEND_CONSUME_NORET:
    case AMDIL::APPEND_ALLOC:
    case AMDIL::APPEND_ALLOC_NORET:
    case AMDIL::ADDri:
    case AMDIL::ADDir:
    case AMDIL::HILO_BITOR_v2i32:
    case AMDIL::HILO_BITOR_v4i16:
    case AMDIL::HILO_BITOR_v2i64:
      return true;
  }

  return !strncmp(MI->getDesc().getName(), "MACRO", 5);
}
// This function returns the OpSwizzle with the custom swizzle set
// correclty for destination operands.
OpSwizzle getCustomDstSwizzle(MachineInstr *MI) {
  OpSwizzle opSwiz;
  opSwiz.u8all = 0;
  unsigned opcode = MI->getOpcode();
  opSwiz.bits.dst = 1;
  if (!strncmp(MI->getDesc().getName(), "MACRO", 5)) {
    return opSwiz;
  }
  if ((opcode >= AMDIL::VEXTRACT_v2f32 &&
        opcode <= AMDIL::VINSERT_v4i8)) {
    opSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_DEFAULT;
  } else if ((opcode >= AMDIL::VCREATE_v2f32 &&
        opcode <= AMDIL::VCREATE_v4i8) ||
      (opcode >= AMDIL::LOADCONST_f32 &&
       opcode <= AMDIL::LOADCONST_i8) ||
      opcode == AMDIL::ADDri ||
      opcode == AMDIL::ADDir) {
    opSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_DEFAULT;
  } else {
    switch (opcode) {
    case AMDIL::LDSLOAD_Y:
    case AMDIL::LDSSTORE_Y:
    case AMDIL::GDSLOAD_Y:
    case AMDIL::GDSSTORE_Y:
    case AMDIL::UAVARENASTORE_Y_i32:
    case AMDIL::UAVARENALOAD_Y_i32:
      opSwiz.bits.dst = 0;
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_Y;
      break;
    case AMDIL::LDSLOAD_Z:
    case AMDIL::LDSSTORE_Z:
    case AMDIL::GDSLOAD_Z:
    case AMDIL::GDSSTORE_Z:
    case AMDIL::UAVARENASTORE_Z_i32:
    case AMDIL::UAVARENALOAD_Z_i32:
      opSwiz.bits.dst = 0;
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_Z;
      break;
    case AMDIL::LDSLOAD_W:
    case AMDIL::LDSSTORE_W:
    case AMDIL::GDSLOAD_W:
    case AMDIL::GDSSTORE_W:
    case AMDIL::UAVARENASTORE_W_i32:
    case AMDIL::UAVARENALOAD_W_i32:
      opSwiz.bits.dst = 0;
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_W;
      break;
    case AMDIL::LDSLOAD:
    case AMDIL::LDSLOAD_i8:
    case AMDIL::LDSLOAD_u8:
    case AMDIL::LDSLOAD_i16:
    case AMDIL::LDSLOAD_u16:
    case AMDIL::LDSSTORE:
    case AMDIL::LDSSTORE_i8:
    case AMDIL::LDSSTORE_i16:
    case AMDIL::UAVARENALOAD_i8:
    case AMDIL::UAVARENALOAD_i16:
    case AMDIL::UAVARENALOAD_i32:
    case AMDIL::UAVARENASTORE_i8:
    case AMDIL::UAVARENASTORE_i16:
    case AMDIL::UAVARENASTORE_i32:
    case AMDIL::GDSLOAD:
    case AMDIL::GDSSTORE:
    case AMDIL::SCRATCHSTORE:
    case AMDIL::SCRATCHSTORE_X:
    case AMDIL::SCRATCHSTORE_Y:
    case AMDIL::SCRATCHSTORE_Z:
    case AMDIL::SCRATCHSTORE_W:
    case AMDIL::SCRATCHSTORE_XY:
    case AMDIL::SCRATCHSTORE_ZW:
    case AMDIL::APPEND_CONSUME:
    case AMDIL::APPEND_CONSUME_NORET:
    case AMDIL::APPEND_ALLOC:
    case AMDIL::APPEND_ALLOC_NORET:
      opSwiz.bits.dst = 0;
      opSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_X;
      break;
    case AMDIL::HILO_BITOR_v2i32:
      opSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_X___;
      break;
    case AMDIL::HILO_BITOR_v4i16:
    case AMDIL::HILO_BITOR_v2i64:
      opSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_XY__;
      break;
    default:
    assert(0 
        && "getCustomDstSwizzle hit an opcode it doesnt' understand!");
    opSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_X___;
    };
  }
  return opSwiz;
}

OpSwizzle getSrcSwizzleID(MachineInstr *MI, unsigned opNum, 
    const AMDILTargetMachine *ATM)
{
  assert(opNum < MI->getNumOperands() && 
      "Must pass in a valid operand number.");
  OpSwizzle curSwiz;
  curSwiz.u8all = 0;
  curSwiz.bits.dst = 0; // We need to reset the dst bit.
  if (isCustomSrcInst(MI, opNum)) {
    curSwiz = getCustomSrcSwizzle(MI, opNum);
  } else if (isLoadInst(MI) || isStoreInst(MI)) {
    if (!MI->getOperand(opNum).isReg()) {
      // If we aren't a register, then we base the
      // operand swizzle on the first operand.
      opNum = 0;
    }
    // Load/store swizzles need to be based on
    // the size of the load/store, not on the
    // number of components.
    const TargetInstrInfo *TII = ATM->getInstrInfo();
    const AMDILRegisterInfo *TRI = ATM->getRegisterInfo();
    const TargetRegisterClass *TRC = TII->getRegClass(MI->getDesc(), 0, TRI);
    unsigned trcID = (TRC) ? TRC->getID() : AMDIL::GPRV4I32RegClassID;
    switch(trcID) {
      default:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XXXX;
        break;
      case AMDIL::GPRV2F32RegClassID:
      case AMDIL::GPRV2I32RegClassID:
      case AMDIL::GPRV4I16RegClassID:
      case AMDIL::GPRI64RegClassID:
      case AMDIL::GPRF64RegClassID:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XYXY;
        break;
      case AMDIL::GPRV4F32RegClassID:
      case AMDIL::GPRV4I32RegClassID:
      case AMDIL::GPRV2I64RegClassID:
      case AMDIL::GPRV2F64RegClassID:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_DEFAULT;
        break;
    }
  } else if (!MI->getDesc().isCall()) {
    if (!MI->getOperand(opNum).isReg()) {
      // If we aren't a register, then we base the
      // operand swizzle on the first operand.
      opNum = 0;
    }
    // All other non-special case instructions need to be
    // based on the number of components
    const TargetInstrInfo *TII = ATM->getInstrInfo();
    const AMDILRegisterInfo *TRI = ATM->getRegisterInfo();
    const TargetRegisterClass *TRC = TII->getRegClass(MI->getDesc(), opNum, TRI);
    unsigned trcID = (TRC) ? TRC->getID() : AMDIL::GPRV4I32RegClassID;
    switch(trcID) {
      default:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XXXX;
        break;
      case AMDIL::GPRV2F32RegClassID:
      case AMDIL::GPRV2I32RegClassID:
      case AMDIL::GPRV2I16RegClassID:
      case AMDIL::GPRV2I8RegClassID:
      case AMDIL::GPRI64RegClassID:
      case AMDIL::GPRF64RegClassID:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_XYXY;
        break;
      case AMDIL::GPRV4F32RegClassID:
      case AMDIL::GPRV4I32RegClassID:
      case AMDIL::GPRV4I16RegClassID:
      case AMDIL::GPRV4I8RegClassID:
      case AMDIL::GPRV2I64RegClassID:
      case AMDIL::GPRV2F64RegClassID:
        curSwiz.bits.swizzle = AMDIL_SRC_SWIZZLE_DEFAULT;
        break;
    }
  }
  return curSwiz;
}

OpSwizzle getDstSwizzleID(MachineInstr *MI,
    const AMDILTargetMachine *ATM)
{
  OpSwizzle curSwiz;
  curSwiz.bits.dst = 1;
  curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_DEFAULT;
  if (isCustomDstInst(MI)) {
    curSwiz = getCustomDstSwizzle(MI);
  } else if (isLoadInst(MI) || isStoreInst(MI)) {
    // Load/store swizzles need to be based on
    // the size of the load/store, not on the
    // number of components.
    const TargetInstrInfo *TII = ATM->getInstrInfo();
    const AMDILRegisterInfo *TRI = ATM->getRegisterInfo();
    const TargetRegisterClass *TRC = TII->getRegClass(MI->getDesc(), 0, TRI);
    unsigned trcID = (TRC) ? TRC->getID() : AMDIL::GPRV4I32RegClassID;
    switch(trcID) {
      default:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_X___;
        break;
      case AMDIL::GPRV2F32RegClassID:
      case AMDIL::GPRV2I32RegClassID:
      case AMDIL::GPRV4I16RegClassID:
      case AMDIL::GPRI64RegClassID:
      case AMDIL::GPRF64RegClassID:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_XY__;
        break;
      case AMDIL::GPRV4F32RegClassID:
      case AMDIL::GPRV4I32RegClassID:
      case AMDIL::GPRV2I64RegClassID:
      case AMDIL::GPRV2F64RegClassID:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_DEFAULT;
        break;
    }
  } else if (!MI->getDesc().isCall()) {
    // All other non-special case instructions need to be
    // based on the number of components
    const TargetInstrInfo *TII = ATM->getInstrInfo();
    const AMDILRegisterInfo *TRI = ATM->getRegisterInfo();
    const TargetRegisterClass *TRC = TII->getRegClass(MI->getDesc(), 0, TRI);
    unsigned trcID = (TRC) ? TRC->getID() : AMDIL::GPRV4I32RegClassID;
    switch(trcID) {
      default:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_X___;
        break;
      case AMDIL::GPRV2F32RegClassID:
      case AMDIL::GPRV2I32RegClassID:
      case AMDIL::GPRV2I16RegClassID:
      case AMDIL::GPRV2I8RegClassID:
      case AMDIL::GPRI64RegClassID:
      case AMDIL::GPRF64RegClassID:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_XY__;
        break;
      case AMDIL::GPRV4F32RegClassID:
      case AMDIL::GPRV4I32RegClassID:
      case AMDIL::GPRV4I16RegClassID:
      case AMDIL::GPRV4I8RegClassID:
      case AMDIL::GPRV2I64RegClassID:
      case AMDIL::GPRV2F64RegClassID:
        curSwiz.bits.swizzle = AMDIL_DST_SWIZZLE_DEFAULT;
        break;
    }
  }

  return curSwiz;
}

/// All vector instructions except for VCREATE_* need to be handled
/// with custom swizzle packing code.
bool isCustomVectorInst(MachineInstr *MI)
{
  unsigned opcode = MI->getOpcode();
  return (opcode >= AMDIL::VCONCAT_v2f32 && opcode <= AMDIL::VCONCAT_v4i8)
      || (opcode >= AMDIL::VEXTRACT_v2f32 && opcode <= AMDIL::VINSERT_v4i8);
}

void encodeVectorInst(MachineInstr *MI, 
    const AMDILTargetMachine *ATM,
    bool mDebug) 
{
  assert(isCustomVectorInst(MI) && "Only a vector instruction can be"
      " used to generate a new vector instruction!");
  unsigned opcode = MI->getOpcode();
  // For all of the opcodes, the destination swizzle is the same.
  OpSwizzle swizID = getDstSwizzleID(MI, ATM);
  OpSwizzle srcID;
  srcID.u8all = 0;
  MI->getOperand(0).setTargetFlags(swizID.u8all);
  switch (opcode) {
    case AMDIL::VCONCAT_v2f32:
    case AMDIL::VCONCAT_v2i16:
    case AMDIL::VCONCAT_v2i32:
    case AMDIL::VCONCAT_v2i8:
      srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_X000;
      MI->getOperand(1).setTargetFlags(srcID.u8all);
      srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_0X00;
      MI->getOperand(2).setTargetFlags(srcID.u8all);
      break;
    case AMDIL::VCONCAT_v2f64:
    case AMDIL::VCONCAT_v2i64:
    case AMDIL::VCONCAT_v4f32:
    case AMDIL::VCONCAT_v4i16:
    case AMDIL::VCONCAT_v4i32:
    case AMDIL::VCONCAT_v4i8:
      srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_XY00;
      MI->getOperand(1).setTargetFlags(srcID.u8all);
      srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_00XY;
      MI->getOperand(2).setTargetFlags(srcID.u8all);
      break;
    case AMDIL::VEXTRACT_v2f32:
    case AMDIL::VEXTRACT_v2i16:
    case AMDIL::VEXTRACT_v2i32:
    case AMDIL::VEXTRACT_v2i8:
      assert(MI->getOperand(2).getImm() <= 2 
          && "Invalid immediate value encountered for this formula!");
    case AMDIL::VEXTRACT_v4f32:
    case AMDIL::VEXTRACT_v4i16:
    case AMDIL::VEXTRACT_v4i32:
    case AMDIL::VEXTRACT_v4i8:
      assert(MI->getOperand(2).getImm() <= 4 
          && "Invalid immediate value encountered for this formula!");
      srcID.bits.swizzle = ((MI->getOperand(2).getImm() - 1) * 4) + 1;
      MI->getOperand(1).setTargetFlags(srcID.u8all);
      MI->getOperand(2).setTargetFlags(0);
      break;
    case AMDIL::VEXTRACT_v2i64:
    case AMDIL::VEXTRACT_v2f64:
      assert(MI->getOperand(2).getImm() <= 2 
          && "Invalid immediate value encountered for this formula!");
      srcID.bits.swizzle = 15 + (MI->getOperand(2).getImm() * 2);
      MI->getOperand(1).setTargetFlags(srcID.u8all);
      MI->getOperand(2).setTargetFlags(0);
     break;
      break;
    case AMDIL::VINSERT_v2f32:
    case AMDIL::VINSERT_v2i32:
    case AMDIL::VINSERT_v2i16:
    case AMDIL::VINSERT_v2i8:
    case AMDIL::VINSERT_v4f32:
    case AMDIL::VINSERT_v4i16:
    case AMDIL::VINSERT_v4i32:
    case AMDIL::VINSERT_v4i8:
      {
        unsigned swizVal = (unsigned)MI->getOperand(4).getImm();
        OpSwizzle src2ID;
        src2ID.u8all = 0;
        if ((swizVal >> 8 & 0xFF) == 1) {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_X0ZW;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_0X00;
        } else if ((swizVal >> 16 & 0xFF) == 1) {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_XY0W;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_00X0;
        } else if ((swizVal >> 24 & 0xFF) == 1) {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_XYZ0;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_000X;
        } else {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_0YZW;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_X000;
        }
        MI->getOperand(1).setTargetFlags(srcID.u8all);
        MI->getOperand(2).setTargetFlags(src2ID.u8all);
        MI->getOperand(3).setTargetFlags(0);
        MI->getOperand(4).setTargetFlags(0);
      }
      break;
    case AMDIL::VINSERT_v2f64:
    case AMDIL::VINSERT_v2i64:
      {
        unsigned swizVal = (unsigned)MI->getOperand(4).getImm();
        OpSwizzle src2ID;
        src2ID.u8all = 0;
        if ((swizVal >> 8 & 0xFF) == 1) {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_XY00;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_00XY;
        } else {
          srcID.bits.swizzle = AMDIL_SRC_SWIZZLE_00ZW;
          src2ID.bits.swizzle = AMDIL_SRC_SWIZZLE_XY00;
        }
        MI->getOperand(1).setTargetFlags(srcID.u8all);
        MI->getOperand(2).setTargetFlags(src2ID.u8all);
        MI->getOperand(3).setTargetFlags(0);
        MI->getOperand(4).setTargetFlags(0);
      }
      break;
  };
  if (mDebug) {
    for (unsigned i = 0; i < MI->getNumOperands(); ++i) {
      dumpOperand(MI, i);
    }
    dbgs() << "\n";
  }
}

// This function loops through all of the instructions, skipping function
// calls, and encodes the swizzles in the operand.
void encodeSwizzles(MachineFunction &MF, bool mDebug, 
    const AMDILTargetMachine *ATM)
{
  for (MachineFunction::iterator MFI = MF.begin(), MFE = MF.end();
      MFI != MFE; ++MFI) {
    MachineBasicBlock *MBB = MFI;
    for (MachineBasicBlock::iterator MBI = MBB->begin(), MBE = MBB->end();
        MBI != MBE; ++MBI) {
      MachineInstr *MI = MBI;
      if (MI->getOpcode() == AMDIL::RETDYN
          || MI->getOpcode() == AMDIL::RETURN
          || MI->getOpcode() == AMDIL::DBG_VALUE) {
        continue;
      }
      if (mDebug) {
        dbgs() << "Encoding instruction: ";
        MI->print(dbgs());
      }
      if (isCustomVectorInst(MI)) {
        encodeVectorInst(MI, ATM, mDebug);
        continue;
      }
      for (unsigned a = 0, z = MI->getNumOperands(); a < z; ++a) {
        OpSwizzle swizID;
        if (MI->getOperand(a).isReg() && MI->getOperand(a).isDef()) {
          swizID = getDstSwizzleID(MI, ATM);
        } else {
          swizID = getSrcSwizzleID(MI, a, ATM);
        }
        MI->getOperand(a).setTargetFlags(swizID.u8all);
        if (mDebug) {
          dumpOperand(MI, a);
        }
      }
      if (mDebug) {
        dbgs() << "\n";
      }
    }
  }
}
#if 0
void allocateSwizzles(MachineFunction &MF, bool mDebug)
{
  std::map<unsigned, unsigned> scalarSwizMap;
  std::map<unsigned, unsigned> doubleSwizMap;
  allocateDstOperands(MF, scalarSwizMap, doubleSwizMap, mDebug);
  allocateSrcOperands(MF, scalarSwizMap, doubleSwizMap, mDebug);
}

void allocateDstOperands(MachineFunction &MF, 
    std::map<unsigned, unsigned> &scalarSwizMap,
    std::map<unsigned, unsigned> &doubleSwizMap,
    bool mDebug)
{
  unsigned scalarIdx = 0;
  unsigned scalarPairs[4][2] = { {1, 1}, {5, 5}, {8, 9}, {10, 13} };
  unsigned doubleIdx = 0;
  unsigned doublePairs[2][2] = { {2, 17}, {9, 19} };
  for (MachineFunction::iterator MFI = MF.begin(), MFE = MF.end();
      MFI != MFE; ++MFI) {
    MachineBasicBlock *MBB = MFI;
    for (MachineBasicBlock::iterator MBI = MBB->begin(), MBE = MBB->end();
        MBI != MBE; ++MBI) {
      MachineInstr *MI = MBI;
      if (MI->getOpcode() == AMDIL::RETDYN
          || MI->getOpcode() == AMDIL::RETURN
          || !MI->getNumOperands()) {
        continue;
      }
      if (mDebug) {
        dbgs() << "Packing instruction(Dst Stage): ";
        MI->print(dbgs());
      }
      MachineOperand &mOp = MI->getOperand(0);
      if (!mOp.isReg() || !mOp.isDef()) {
        if (mDebug) {
          dbgs() << "\tSkipping instruction, no def found!\n\n";
        }
        continue;
      }
      OpSwizzle flags;
      flags.u8all = mOp.getTargetFlags();
      if ((flags.bits.swizzle == AMDIL_DST_SWIZZLE_X___)) {
        if (mDebug) {
          dbgs() << "\tOld Encoding: ";
          dumpSwizzle(flags);
        }
        scalarIdx = (scalarIdx + 1) & 0x3;
        std::map<unsigned, unsigned>::iterator regIter =
          scalarSwizMap.find(mOp.getReg());
        if (regIter == scalarSwizMap.end()) {
          flags.bits.swizzle = scalarPairs[scalarIdx][0];
          scalarSwizMap[mOp.getReg()] = scalarIdx;
          propogateSrcSwizzles(&mOp, scalarPairs[scalarIdx][1], mDebug);
        } else {
          flags.bits.swizzle = scalarPairs[regIter->second][0];
          propogateSrcSwizzles(&mOp, scalarPairs[scalarIdx][1], mDebug);
        }
        if (mDebug) {
          dbgs() << "\tNew Encoding: ";
          dumpSwizzle(flags);
        }
        mOp.setTargetFlags(flags.u8all);
      } else if ((flags.bits.swizzle == AMDIL_DST_SWIZZLE_XY__)) {
        doubleIdx = (doubleIdx + 1) & 0x1;
        if (mDebug) {
          dbgs() << "\tOld Encoding: ";
          dumpSwizzle(flags);
        }
        std::map<unsigned, unsigned>::iterator regIter =
          doubleSwizMap.find(mOp.getReg());
        if (regIter == scalarSwizMap.end()) {
          flags.bits.swizzle = doublePairs[doubleIdx][0];
          doubleSwizMap[mOp.getReg()] = doubleIdx;
          propogateSrcSwizzles(&mOp, doublePairs[doubleIdx][1], mDebug);
        } else {
          flags.bits.swizzle = doublePairs[regIter->second][0];
          propogateSrcSwizzles(&mOp, doublePairs[regIter->second][1], mDebug);
        }
        if (mDebug) {
          dbgs() << "\tNew Encoding: ";
          dumpSwizzle(flags);
        }
        mOp.setTargetFlags(flags.u8all);
      } else if ((flags.bits.swizzle == AMDIL_DST_SWIZZLE_DEFAULT)) {
        if (mDebug) {
          dbgs() << "\tSkipping instruction, fully packed!\n";
        }
      } else {
        assert(0 && "Found an instruction that didn't have a swizzle!");
      }
      if (mDebug) {
        dbgs() << "\n";
      }
    }
  }
}

void allocateSrcOperands(MachineFunction &MF, 
    std::map<unsigned, unsigned> &scalarSwizMap,
    std::map<unsigned, unsigned> &doubleSwizMap,
    bool mDebug)
{
  unsigned scalarPairs[4][2] = { 
    {AMDIL_DST_SWIZZLE_X___, AMDIL_SRC_SWIZZLE_X000}, 
    {AMDIL_DST_SWIZZLE__Y__, AMDIL_SRC_SWIZZLE_Y000}, 
    {AMDIL_DST_SWIZZLE___Z_, AMDIL_SRC_SWIZZLE_Z000}, 
    {AMDIL_DST_SWIZZLE____W, AMDIL_SRC_SWIZZLE_W000} };
  unsigned doublePairs[2][2] = { 
    {AMDIL_DST_SWIZZLE_XY__, AMDIL_SRC_SWIZZLE_XY00}, 
    {AMDIL_DST_SWIZZLE___ZW, AMDIL_SRC_SWIZZLE_ZW00} };
  for (MachineFunction::iterator MFI = MF.begin(), MFE = MF.end();
      MFI != MFE; ++MFI) {
    MachineBasicBlock *MBB = MFI;
    for (MachineBasicBlock::iterator MBI = MBB->begin(), MBE = MBB->end();
        MBI != MBE; ++MBI) {
      MachineInstr *MI = MBI;
      if (MI->getOpcode() == AMDIL::RETDYN
          || MI->getOpcode() == AMDIL::RETURN
          || !MI->getNumOperands()) {
        continue;
      }
      if (mDebug) {
        dbgs() << "Packing instruction(Src Stage): ";
        MI->print(dbgs());
      }
      unsigned dstIdx = 0;
      for (unsigned a = 0, z = MI->getNumOperands(); a < z; ++a) {
        MachineOperand &mOp = MI->getOperand(a);
        if (mOp.isReg() && mOp.isDef()) {
          if (mDebug) {
            dbgs() << "\tEncoding: ";
            dumpOperand(MI, a);
            dbgs() << "\tSkipping operand: " << a << ", def found!\n\n";
          }
          OpSwizzle dstSwiz;
          dstSwiz.u8all = mOp.getTargetFlags();
          dstIdx = dstSwiz.bits.swizzle;
          continue;
        }
        if (!mOp.isReg()) {
          if (mDebug) {
            dbgs() << "\tEncoding: ";
            dumpOperand(MI, a);
            dbgs() << "\tSkipping operand: " << a << ", no reg found!\n\n";
          }
          continue;
        }
        if (mDebug) {
          dbgs() << "\tOld Encoding: ";
          dumpOperand(MI, a);
        }
        OpSwizzle srcSwiz;
        srcSwiz.u8all = mOp.getTargetFlags();
        unsigned reg = mOp.getReg();
        unsigned srcOffset = 0;
        std::map<unsigned, unsigned>::iterator regIter;
        switch (dstIdx) {
          default:
            break;
          case AMDIL_DST_SWIZZLE____W:
          case AMDIL_DST_SWIZZLE___Z_:
          case AMDIL_DST_SWIZZLE__Y__:
          case AMDIL_DST_SWIZZLE_X___:
            srcOffset = dstIdx / 3;
            regIter = scalarSwizMap.find(reg);
            if (regIter != scalarSwizMap.end()) {
              srcSwiz.bits.swizzle = scalarPairs[regIter->second][1] 
                + srcOffset;
              mOp.setTargetFlags(srcSwiz.u8all);
            } else {
              if (mDebug) {
                MI->dump();
              }
            }
            break;
          case AMDIL_DST_SWIZZLE___ZW:
            srcOffset = 1;
          case AMDIL_DST_SWIZZLE_XY__:
            regIter = doubleSwizMap.find(reg);
            if (regIter != doubleSwizMap.end()) {
              srcSwiz.bits.swizzle = doublePairs[regIter->second][1] 
                + srcOffset;
              mOp.setTargetFlags(srcSwiz.u8all);
            } else {
              if (mDebug) {
                MI->dump();
              }
            }
            break;
        }
        if (mDebug) {
          dbgs() << "\tNew Encoding: ";
          dumpOperand(MI, a);
        }
        if (mDebug) {
          dbgs() << "\n";
        }
      }
    }
  }
}

void propogateSrcSwizzles(MachineOperand *MO, unsigned idx, bool mDebug)
{
  MachineOperand *nMO = MO->getNextOperandForReg();
  while (nMO && !nMO->isDef()) {
    OpSwizzle flags;
    flags.u8all = nMO->getTargetFlags();
    if (mDebug) {
      dbgs() << "\t\tOld Swizzle: ";
      dumpSwizzle(flags);
    }
    flags.bits.swizzle = idx;
    if (mDebug) {
      dbgs() << "\t\tNew Swizzle: ";
      dumpSwizzle(flags);
    }
    nMO->setTargetFlags(flags.u8all);
    nMO = nMO->getNextOperandForReg();
  }
}
#endif
