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


#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetMachine.h"

#include "AMDILInstrInfo.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILUtilityFunctions.h"
#include "AMDISA.h"
#include "AMDISAUtil.h"

#include <stdio.h>

#define SRC_BYTE_COUNT 11
#define DST_BYTE_COUNT 5

using namespace llvm;

namespace {

  /* XXX: Temp HACK to work around tablegen name generation */
  class AMDILCodeEmitter {
  public:
#if LLVM_VERSION > 3000
    uint64_t
#else
    unsigned
#endif
    getBinaryCodeForInstr(const MachineInstr &MI) const;
  };

  class R600CodeEmitter : public MachineFunctionPass, public AMDILCodeEmitter {

  private:

  static char ID;
  formatted_raw_ostream &_OS;
  const TargetMachine * TM;
  const MachineRegisterInfo * MRI;
  AMDILMachineFunctionInfo * MFI;
  const AMDISARegisterInfo * TRI;
  bool evergreenEncoding;

  bool isReduction;
  unsigned reductionElement;
  bool isLast;

  public:

  R600CodeEmitter(formatted_raw_ostream &OS) : MachineFunctionPass(ID),
      _OS(OS), TM(NULL), evergreenEncoding(false), isReduction(false),
      isLast(true) { }

  const char *getPassName() const { return "AMDISA Machine Code Emitter"; }

  bool runOnMachineFunction(MachineFunction &MF);

  private:

  void emitALUInstr(MachineInstr  &MI);
  void emitSrc(const MachineOperand & MO);
  void emitDst(const MachineOperand & MO);
  void emitALU(MachineInstr &MI, unsigned numSrc);
  void emitTexInstr(MachineInstr &MI);
  void emitFCInstr(MachineInstr &MI);

  unsigned int getHWInst(const MachineInstr &MI);

  void emitNullBytes(unsigned int byteCount);

  void emitByte(unsigned int byte);

  void emitTwoBytes(uint32_t bytes);

  void emit(uint32_t value);

  unsigned getHWReg(unsigned regNo);

  unsigned getElement(unsigned regNo);
  int getElement(MachineInstr &MI);

};

} /* End anonymous namespace */

#define WRITE_MASK_X 0x1
#define WRITE_MASK_Y 0x2
#define WRITE_MASK_Z 0x4
#define WRITE_MASK_W 0x8

enum RegElement {
  ELEMENT_X = 0,
  ELEMENT_Y,
  ELEMENT_Z,
  ELEMENT_W
};

enum InstrTypes {
  INSTR_ALU = 0,
  INSTR_TEX,
  INSTR_FC
};

enum FCInstr {
  FC_IF = 0,
  FC_ELSE,
  FC_ENDIF,
  FC_BGNLOOP,
  FC_ENDLOOP,
  FC_BREAK,
  FC_CONTINUE
};

enum TextureTypes {
  TEXTURE_1D = 1,
  TEXTURE_2D, 
  TEXTURE_3D,
  TEXTURE_CUBE,
  TEXTURE_RECT,
  TEXTURE_SHADOW1D,
  TEXTURE_SHADOW2D,
  TEXTURE_SHADOWRECT,     
  TEXTURE_1D_ARRAY,       
  TEXTURE_2D_ARRAY,
  TEXTURE_SHADOW1D_ARRAY,
  TEXTURE_SHADOW2D_ARRAY
};

char R600CodeEmitter::ID = 0;

FunctionPass *llvm::createR600CodeEmitterPass(formatted_raw_ostream &OS) {
  return new R600CodeEmitter(OS);
}

bool R600CodeEmitter::runOnMachineFunction(MachineFunction &MF) {

  TM = &MF.getTarget();
  MRI = &MF.getRegInfo();
  MFI = MF.getInfo<AMDILMachineFunctionInfo>();
  TRI = static_cast<const AMDISARegisterInfo *>(TM->getRegisterInfo());
  const AMDILSubtarget &STM = TM->getSubtarget<AMDILSubtarget>();
  std::string gpu = STM.getDeviceName();
  if (!gpu.compare(0,3, "rv7")) {
    evergreenEncoding = false;
  } else {
    evergreenEncoding = true;
  }
  const AMDISATargetMachine *amdtm =
    static_cast<const AMDISATargetMachine *>(&MF.getTarget());

  if (amdtm->shouldDumpCode()) {
    MF.dump();
  }

  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
     MachineBasicBlock &MBB = *BB;
     for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
                                                       I != E; ++I) {
          MachineInstr &MI = *I;
          if (isTexOp(MI.getOpcode())) {
            emitTexInstr(MI);
          } else if (isFCOp(MI.getOpcode())){
            emitFCInstr(MI);
          } else if (isReductionOp(MI.getOpcode())) {
            isReduction = true;
            isLast = false;
            for (reductionElement = 0; reductionElement < 4; reductionElement++) {
              isLast = (reductionElement == 3);
              emitALUInstr(MI);
            }
            isReduction = false;
          } else if (MI.getOpcode() == AMDIL::RETURN) {
            continue;
          } else {
            emitALUInstr(MI);
          }
     }
  }
  return false;
}

void R600CodeEmitter::emitALUInstr(MachineInstr &MI)
{

  unsigned numOperands = MI.getNumOperands();

   /* Some instructions are just place holder instructions that represent
    * operations that the GPU does automatically.  They should be ignored. */
  if (isPlaceHolderOpcode(MI.getOpcode())) {
    return;
  }

  /* We need to handle some opcodes differently */
  switch (MI.getOpcode()) {
    default: break;

    /* Custom swizzle instructions, ignore the last two operands */
    case AMDIL::SET_CHAN:
      numOperands = 2;
      break;

    case AMDIL::VEXTRACT_v4f32:
      numOperands = 2;
      break;

    /* XXX: Temp Hack */
    case AMDIL::STORE_OUTPUT:
      numOperands = 2;
      break;
  }

  /* XXX Check if instruction writes a result */
  if (numOperands < 1) {
    return;
  }
  const MachineOperand dstOp = MI.getOperand(0);

  /* Emit instruction type */
  emitByte(0);

  unsigned int opIndex;
  for (opIndex = 1; opIndex < numOperands; opIndex++) {
    emitSrc(MI.getOperand(opIndex));
  }

    /* Emit zeros for unused sources */
  for ( ; opIndex < 4; opIndex++) {
    emitNullBytes(SRC_BYTE_COUNT);
  }

  emitDst(dstOp);

  emitALU(MI, numOperands - 1);
}

void R600CodeEmitter::emitSrc(const MachineOperand & MO)
{

  uint32_t value = 0;
  /* Emit the source select (2 bytes).  For GPRs, this is the register index.
   * For other potential instruction operands, (e.g. constant registers) the
   * value of the source select is defined in the r600isa docs. */
  if (MO.isReg()) {
    emitTwoBytes(getHWReg(MO.getReg()));
  } else if (MO.isImm()) {
    /* XXX: Magic number, comment this */
    emitTwoBytes(253);
    value = getLiteral(MFI, MO.getImm());
  } else {
    /* XXX: Handle other operand types. */
    emitTwoBytes(0);
  }

  /* Emit the source channel (1 byte) */
  if (isReduction) {
    emitByte(reductionElement);
  } else if (MO.isReg()) {
    const MachineInstr * parent = MO.getParent();
    /* The source channel for EXTRACT is stored in operand 2. */
    if (parent->getOpcode() == AMDIL::VEXTRACT_v4f32) {
      emitByte(parent->getOperand(2).getImm());
    } else {
      emitByte(getRegElement(TRI, MO.getReg()));
    }
  } else {
    emitByte(0);
  }

  /* XXX: Emit isNegated (1 byte) */
  if ((!(MO.getTargetFlags() & MO_FLAG_ABS))
      && (MO.getTargetFlags() & MO_FLAG_NEG ||
     (MO.isReg() &&
      (MO.getReg() == AMDIL::NEG_ONE || MO.getReg() == AMDIL::NEG_HALF)))){
    emitByte(1);
  } else {
    emitByte(0);
  }

  /* Emit isAbsolute (1 byte) */
  if (MO.getTargetFlags() & MO_FLAG_ABS) {
    emitByte(1);
  } else {
    emitByte(0);
  }

  /* XXX: Emit relative addressing mode (1 byte) */
  emitByte(0);

  /* Emit kc_bank, This will be adjusted later by r600_asm */
  emitByte(0);

  /* Emit the literal value, if applicable (4 bytes).  */
  emit(value);

}

void R600CodeEmitter::emitDst(const MachineOperand & MO)
{
  if (MO.isReg()) {
    /* Emit the destination register index (1 byte) */
    emitByte(getHWReg(MO.getReg()));

    /* Emit the element of the destination register (1 byte)*/
    const MachineInstr * parent = MO.getParent();
    if (isReduction) {
      emitByte(reductionElement);

    /* The destination element for SET_CHAN is stored in the 3rd operand. */
    } else if (parent->getOpcode() == AMDIL::SET_CHAN) {
      emitByte(parent->getOperand(2).getImm());
    } else if (parent->getOpcode() == AMDIL::VCREATE_v4f32) {
      emitByte(ELEMENT_X);
    } else {
      emitByte(getRegElement(TRI, MO.getReg()));
    }

    /* Emit isClamped (1 byte) */
    if (MO.getTargetFlags() & MO_FLAG_CLAMP) {
      emitByte(1);
    } else {
      emitByte(0);
    }

    /* Emit writemask (1 byte).  */
    if ((isReduction && reductionElement != getRegElement(TRI, MO.getReg()))
         || MO.getTargetFlags() & MO_FLAG_MASK) {
      emitByte(0);
    } else {
      emitByte(1);
    }

    /* XXX: Emit relative addressing mode */
    emitByte(0);
  } else {
    /* XXX: Handle other operand types.  Are there any for destination regs? */
    emitNullBytes(DST_BYTE_COUNT);
  }
}

void R600CodeEmitter::emitALU(MachineInstr &MI, unsigned numSrc)
{
  /* Emit the instruction (2 bytes) */
  emitTwoBytes(getHWInst(MI));

  /* Emit isLast (for this instruction group) (1 byte) */
  if (isLast) {
    emitByte(1);
  } else {
    emitByte(0);
  }
  /* Emit isOp3 (1 byte) */
  if (numSrc == 3) {
    emitByte(1);
  } else {
    emitByte(0);
  }

  /* XXX: Emit predicate (1 byte) */
  emitByte(0);

  /* XXX: Emit bank swizzle. (1 byte)  Do we need this?  It looks like
   * r600_asm.c sets it. */
  emitByte(0);

  /* XXX: Emit bank_swizzle_force (1 byte) Not sure what this is for. */
  emitByte(0);

  /* XXX: Emit OMOD (1 byte) Not implemented. */
  emitByte(0);

  /* XXX: Emit index_mode.  I think this is for indirect addressing, so we
   * don't need to worry about it. */
  emitByte(0);
}

void R600CodeEmitter::emitTexInstr(MachineInstr &MI)
{

  int64_t sampler = MI.getOperand(2).getImm();
  int64_t textureType = MI.getOperand(3).getImm();
  unsigned opcode = MI.getOpcode();
  unsigned srcSelect[4] = {0, 1, 2, 3};

  /* Emit instruction type */
  emitByte(1);

  /* Emit instruction */
  emitByte(getHWInst(MI));

  /* XXX: Emit resource id r600_shader.c uses sampler + 1.  Why? */
  emitByte(sampler + 1 + 1);

  /* Emit source register */
  emitByte(getHWReg(MI.getOperand(1).getReg()));

  /* XXX: Emit src isRelativeAddress */
  emitByte(0);

  /* Emit destination register */
  emitByte(getHWReg(MI.getOperand(0).getReg()));

  /* XXX: Emit dst isRealtiveAddress */
  emitByte(0);

  /* XXX: Emit dst select */
  emitByte(0); /* X */
  emitByte(1); /* Y */
  emitByte(2); /* Z */
  emitByte(3); /* W */

  /* XXX: Emit lod bias */
  emitByte(0);

  /* XXX: Emit coord types */
  unsigned coordType[4] = {1, 1, 1, 1};

  if (textureType == TEXTURE_RECT
      || textureType == TEXTURE_SHADOWRECT) {
    coordType[ELEMENT_X] = 0;
    coordType[ELEMENT_Y] = 0;
  }

  if (textureType == TEXTURE_1D_ARRAY
      || textureType == TEXTURE_SHADOW1D_ARRAY) {
    if (opcode == AMDIL::TEX_SAMPLE_C_L || opcode == AMDIL::TEX_SAMPLE_C_LB) {
      coordType[ELEMENT_Y] = 0;
    } else {
      coordType[ELEMENT_Z] = 0;
      srcSelect[ELEMENT_Z] = ELEMENT_Y;
    }
  } else if (textureType == TEXTURE_2D_ARRAY
             || textureType == TEXTURE_SHADOW2D_ARRAY) {
    coordType[ELEMENT_Z] = 0;
  }

  for (unsigned i = 0; i < 4; i++) {
    emitByte(coordType[i]);
  }

  /* XXX: Emit offsets */
  emitByte(0); /* X */
  emitByte(0); /* Y */
  emitByte(0); /* Z */
  /* There is no OFFSET_W */

  /* Emit sampler id */
  emitByte(sampler);

  /* XXX:Emit source select */
  if ((textureType == TEXTURE_SHADOW1D
      || textureType == TEXTURE_SHADOW2D
      || textureType == TEXTURE_SHADOWRECT
      || textureType == TEXTURE_SHADOW1D_ARRAY)
      && opcode != AMDIL::TEX_SAMPLE_C_L
      && opcode != AMDIL::TEX_SAMPLE_C_LB) {
    srcSelect[ELEMENT_W] = ELEMENT_Z;
  }

  for (unsigned i = 0; i < 4; i++) {
    emitByte(srcSelect[i]);
  }
}

void R600CodeEmitter::emitFCInstr(MachineInstr &MI)
{
  /* Emit instruction type */
  emitByte(INSTR_FC);

  /* Emit SRC */
  unsigned numOperands = MI.getNumOperands();
  if (numOperands > 0) {
    assert(numOperands == 1);
    emitSrc(MI.getOperand(0));
  } else {
    emitNullBytes(SRC_BYTE_COUNT);
  }

  /* Emit FC Instruction */
  enum FCInstr instr;
  switch (MI.getOpcode()) {
  case AMDIL::BREAK_LOGICALZ_f32:
    instr = FC_BREAK;
    break;
  case AMDIL::CONTINUE_LOGICALNZ_f32:
    instr = FC_CONTINUE;
    break;
  /* XXX: This assumes that all IFs will be if (x != 0).  If we add
   * optimizations this might not be the case */
  case AMDIL::IF_LOGICALNZ_f32:
    instr = FC_IF;
    break;
  case AMDIL::IF_LOGICALZ_f32:
    abort();
    break;
  case AMDIL::ELSE:
    instr = FC_ELSE;
    break;
  case AMDIL::ENDIF:
    instr = FC_ENDIF;
    break;
  case AMDIL::ENDLOOP:
    instr = FC_ENDLOOP;
    break;
  case AMDIL::WHILELOOP:
    instr = FC_BGNLOOP;
    break;
  default:
    abort();
    break;
  }
  emitByte(instr);
}

#define INSTR_FLOAT2_V(inst, hw) \
  case AMDIL:: inst##_v4f32: \
  case AMDIL:: inst##_v2f32: return HW_INST2(hw);

#define INSTR_FLOAT2_S(inst, hw) \
  case AMDIL:: inst##_f32: return HW_INST2(hw);

#define INSTR_FLOAT2(inst, hw) \
  INSTR_FLOAT2_V(inst, hw) \
  INSTR_FLOAT2_S(inst, hw)

unsigned int R600CodeEmitter::getHWInst(const MachineInstr &MI)
{

  /* XXX: Lower these to MOV before the code emitter. */
  switch (MI.getOpcode()) {
    case AMDIL::STORE_OUTPUT:
    case AMDIL::VCREATE_v4i32:
    case AMDIL::VCREATE_v4f32:
    case AMDIL::VEXTRACT_v4f32:
    case AMDIL::VINSERT_v4f32:
    case AMDIL::LOADCONST_i32:
    case AMDIL::LOADCONST_f32:
    case AMDIL::MOVE_v4i32:
    case AMDIL::SET_CHAN:
    /* Instructons to reinterpret bits as ... */
    case AMDIL::IL_ASINT_f32:
    case AMDIL::IL_ASINT_i32:
    case AMDIL::IL_ASFLOAT_f32:
    case AMDIL::IL_ASFLOAT_i32:
      return 0x19;

  default:
    return getBinaryCodeForInstr(MI);
  }
}

void R600CodeEmitter::emitNullBytes(unsigned int byteCount)
{
  for (unsigned int i = 0; i < byteCount; i++) {
    emitByte(0);
  }
}

void R600CodeEmitter::emitByte(unsigned int byte)
{
  _OS.write((uint8_t) byte & 0xff);
}
void R600CodeEmitter::emitTwoBytes(unsigned int bytes)
{
  _OS.write((uint8_t) (bytes & 0xff));
  _OS.write((uint8_t) ((bytes >> 8) & 0xff));
}

void R600CodeEmitter::emit(uint32_t value)
{
  for (unsigned i = 0; i < 4; i++) {
    _OS.write((uint8_t) ((value >> (8 * i)) & 0xff));
  }
}

unsigned R600CodeEmitter::getHWReg(unsigned regNo)
{
  unsigned hwReg;

  if (AMDIL::SPECIALRegClass.contains(regNo)) {
    switch(regNo) {
    case AMDIL::ZERO: return 248;
    case AMDIL::ONE:
    case AMDIL::NEG_ONE: return 249;
    case AMDIL::HALF: 
    case AMDIL::NEG_HALF: return 252;
    default:
      abort();
      return 0;
    }
  }

  hwReg = getHWRegNum(TRI, regNo);
  /* XXX: Clean this up */
  if (AMDIL::REPLRegClass.contains(regNo)) {
    return hwReg;
  }
  hwReg = hwReg / 4;
  if (AMDIL::R600_CReg_32RegClass.contains(regNo)) {
    hwReg += 512;
  }
  return hwReg;
}

int R600CodeEmitter::getElement(MachineInstr &MI)
{
  if (MI.getNumOperands() == 0 || !MI.getOperand(0).isReg()) {
    return -1;
  } else {
    switch(MI.getOpcode()) {
    case AMDIL::EXPORT_REG:
    case AMDIL::SWIZZLE:
      return -1;
    default:
      return getRegElement(TRI, MI.getOperand(0).getReg());
    }
  }
}

RegElement maskBitToElement(unsigned int maskBit)
{
  switch (maskBit) {
    case WRITE_MASK_X: return ELEMENT_X;
    case WRITE_MASK_Y: return ELEMENT_Y;
    case WRITE_MASK_Z: return ELEMENT_Z;
    case WRITE_MASK_W: return ELEMENT_W;
    default:
      assert("Invalid maskBit");
      return ELEMENT_X;
  }
}

unsigned int dstSwizzleToWriteMask(unsigned swizzle)
{
  switch(swizzle) {
  default:
  case AMDIL_DST_SWIZZLE_DEFAULT:
    return WRITE_MASK_X | WRITE_MASK_Y | WRITE_MASK_Z | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE_X___:
    return WRITE_MASK_X;
  case AMDIL_DST_SWIZZLE_XY__:
    return WRITE_MASK_X | WRITE_MASK_Y;
  case AMDIL_DST_SWIZZLE_XYZ_:
    return WRITE_MASK_X | WRITE_MASK_Y | WRITE_MASK_Z;
  case AMDIL_DST_SWIZZLE_XYZW:
    return WRITE_MASK_X | WRITE_MASK_Y | WRITE_MASK_Z | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE__Y__:
    return WRITE_MASK_Y;
  case AMDIL_DST_SWIZZLE__YZ_:
    return WRITE_MASK_Y | WRITE_MASK_Z;
  case AMDIL_DST_SWIZZLE__YZW:
    return WRITE_MASK_Y | WRITE_MASK_Z | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE___Z_:
    return WRITE_MASK_Z;
  case AMDIL_DST_SWIZZLE___ZW:
    return WRITE_MASK_Z | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE____W:
    return WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE_X_ZW:
    return WRITE_MASK_X | WRITE_MASK_Z | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE_XY_W:
    return WRITE_MASK_X | WRITE_MASK_Y | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE_X_Z_:
    return WRITE_MASK_X | WRITE_MASK_Z;
  case AMDIL_DST_SWIZZLE_X__W:
    return WRITE_MASK_X | WRITE_MASK_W;
  case AMDIL_DST_SWIZZLE__Y_W:
    return WRITE_MASK_Y | WRITE_MASK_W;
  }
}

#include "AMDISAGenCodeEmitter.inc"

