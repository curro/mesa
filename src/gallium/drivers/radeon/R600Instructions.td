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


include "R600Intrinsics.td"

class InstR600 <bits<32> inst, dag outs, dag ins, string asm, list<dag> pattern,
                InstrItinClass itin>
    : AMDISAInst <outs, ins, asm, pattern> {

  field bits<32> Inst;
	bit Trig = 0;
  bit Op3 = 0;

  let Inst = inst;
  let Namespace = "AMDIL";
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asm;
  let Pattern = pattern;
  let Itinerary = itin;

  let TSFlags{4} = Trig;
  let TSFlags{5} = Op3;
}

class R600_1OP <bits<32> inst, string opName, list<dag> pattern,
                InstrItinClass itin = AnyALU> :
  InstR600 <inst,
          (outs R600_Reg32:$dst),
          (ins R600_Reg32:$src),
          !strconcat(opName, " $dst, $src"),
          pattern,
          itin
  >;

class R600_2OP <bits<32> inst, string opName, list<dag> pattern,
                InstrItinClass itin = AnyALU> :
  InstR600 <inst,
          (outs R600_Reg32:$dst),
          (ins R600_Reg32:$src0, R600_Reg32:$src1),
          !strconcat(opName, " $dst, $src0, $src1"),
          pattern,
          itin
  >;

class R600_3OP <bits<32> inst, string opName, list<dag> pattern,
                InstrItinClass itin = AnyALU> :
  InstR600 <inst,
          (outs R600_Reg32:$dst),
          (ins R600_Reg32:$src0, R600_Reg32:$src1, R600_Reg32:$src2),
          !strconcat(opName, "$dst $src0, $src1, $src2"),
          pattern,
          itin>{

    let Op3 = 1;
  }

class R600_REDUCTION <bits<32> inst, dag ins, string asm, list<dag> pattern,
                      InstrItinClass itin = AnyALU> :
  InstR600 <inst,
          (outs R600_Reg32:$dst),
          ins,
          asm,
          pattern,
          itin

  >;

class R600_TEX <bits<32> inst, string opName, list<dag> pattern,
                InstrItinClass itin = AnyALU> :
  InstR600 <inst,
          (outs REPL:$dst),
          (ins REPL:$src0, i32imm:$src1, i32imm:$src2),
          !strconcat(opName, "$dst, $src0, $src1, $src2"),
          pattern,
          itin
  >;

def TEX_SHADOW : PatLeaf<
  (imm),
  [{uint32_t TType = (uint32_t)N->getZExtValue();
    return (TType >= 6 && TType <= 8) || TType == 11 || TType == 12;
  }]
>;

def isR600 : Predicate<"Subtarget.device()"
                            "->getGeneration() == AMDILDeviceInfo::HD4XXX">;
def isEG : Predicate<"Subtarget.device()"
                            "->getGeneration() == AMDILDeviceInfo::HD5XXX">;
def isCayman : Predicate<"Subtarget.device()"
                            "->getGeneration() == AMDILDeviceInfo::HD6XXX">;
def isEGorCayman : Predicate<"Subtarget.device()"
                            "->getGeneration() >= AMDILDeviceInfo::HD5XXX">;

def isR600toCayman : Predicate<
            "Subtarget.device()->getGeneration() >= AMDILDeviceInfo::HD4XXX"
       " && Subtarget.device()->getGeneration() <= AMDILDeviceInfo::HD6XXX">;


let Predicates = [isR600toCayman] in { 

/* ------------------------------------------- */
/* Common Instructions R600, R700, Evergreen, Cayman */
/* ------------------------------------------- */
let Gen = AMDISAGen.R600_CAYMAN  in {

def ADD : R600_2OP <
  0x0, "ADD",
  [(set R600_Reg32:$dst, (fadd R600_Reg32:$src0, R600_Reg32:$src1))] > {
  let AMDILOp = AMDILInst.ADD_f32;
}
// Non-IEEE MUL: 0 * anything = 0
def MUL : R600_2OP <
  0x1, "MUL NON-IEEE",
  [(set R600_Reg32:$dst, (int_AMDISA_mul R600_Reg32:$src0, R600_Reg32:$src1))]
>;

def MUL_IEEE : R600_2OP <
  0x2, "MUL_IEEE",
  [(set R600_Reg32:$dst, (fmul R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.MUL_IEEE_f32;
}

def MAX : R600_2OP <
  0x3, "MAX",
  [(set R600_Reg32:$dst, (int_AMDIL_max R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.MAX_f32;
}

def MIN : R600_2OP <
  0x4, "MIN",
  [(set R600_Reg32:$dst, (int_AMDIL_min R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.MIN_f32;
}

/* For the SET* instructions there is a naming conflict in TargetSelectionDAG.td,
 * so some of the instruction names don't match the asm string.
 * XXX: Use the defs in TargetSelectionDAG.td instead of intrinsics.
 */

def SETE : R600_2OP <
  0x08, "SETE",
  [(set R600_Reg32:$dst, (int_AMDISA_seq R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.FEQ;
}

def SGT : R600_2OP <
  0x09, "SETGT",
  [(set R600_Reg32:$dst, (int_AMDISA_sgt R600_Reg32:$src0, R600_Reg32:$src1))]
>;

def SGE : R600_2OP <
  0xA, "SETGE",
  [(set R600_Reg32:$dst, (int_AMDISA_sge R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.FGE;
}

def SNE : R600_2OP <
  0xB, "SETNE",
  [(set R600_Reg32:$dst, (int_AMDISA_sne R600_Reg32:$src0, R600_Reg32:$src1))]> {
  let AMDILOp = AMDILInst.FNE;
}

def FRACT : R600_1OP <
  0x10, "FRACT",
  []> {
  let AMDILOp = AMDILInst.FRAC_f32;
}

def TRUNC : R600_1OP <
  0x11, "TRUNC",
  [(set R600_Reg32:$dst, (int_AMDISA_trunc R600_Reg32:$src))]
>;

def FLOOR : R600_1OP <
  0x14, "FLOOR",
  [(set R600_Reg32:$dst, (int_AMDISA_floor R600_Reg32:$src))]
>;

def MOV : R600_1OP <0x19, "MOV", []>;

def KILLGT : R600_2OP <
  0x2D, "KILLGT",
  []
>;

def AND_INT : R600_2OP <
  0x30, "AND_INT",
  []> {
  let AMDILOp = AMDILInst.AND_i32;
}

/* Texture instructions */

def TEX_SAMPLE : R600_TEX <
  0x10, "TEX_SAMPLE",
  [(set REPL:$dst, (int_AMDISA_tex REPL:$src0, imm:$src1, imm:$src2))]
>;

def TEX_SAMPLE_C : R600_TEX <
  0x18, "TEX_SAMPLE_C",
  [(set REPL:$dst, (int_AMDISA_tex REPL:$src0, imm:$src1, TEX_SHADOW:$src2))]
>;

def TEX_SAMPLE_L : R600_TEX <
  0x11, "TEX_SAMPLE_L",
  [(set REPL:$dst, (int_AMDISA_txl REPL:$src0, imm:$src1, imm:$src2))]
>;

def TEX_SAMPLE_C_L : R600_TEX <
  0x19, "TEX_SAMPLE_C_L",
  [(set REPL:$dst, (int_AMDISA_txl REPL:$src0, imm:$src1, TEX_SHADOW:$src2))]
>;

def TEX_SAMPLE_LB : R600_TEX <
  0x12, "TEX_SAMPLE_LB",
  [(set REPL:$dst, (int_AMDISA_txb REPL:$src0, imm:$src1, imm:$src2))]
>;

def TEX_SAMPLE_C_LB : R600_TEX <
  0x1A, "TEX_SAMPLE_C_LB",
  [(set REPL:$dst, (int_AMDISA_txb REPL:$src0, imm:$src1, TEX_SHADOW:$src2))]
>;

def TEX_SAMPLE_G : R600_TEX <
  0x14, "TEX_SAMPLE_G",
  [(set REPL:$dst, (int_AMDISA_txd REPL:$src0, imm:$src1, imm:$src2))]
>;

def TEX_SAMPLE_C_G : R600_TEX <
  0x1C, "TEX_SAMPLE_C_G",
  [(set REPL:$dst, (int_AMDISA_txd REPL:$src0, imm:$src1, TEX_SHADOW:$src2))]
>;

} // End Gen R600_CAYMAN

def KILP : Pat <
  (int_AMDISA_kilp),
  (MASK_WRITE (KILLGT ONE, ZERO))
>;

/* Helper classes for common instructions */

class MUL_LIT_Common <bits<32> inst> : R600_3OP <
  inst, "MUL_LIT",
  []
>;

class MULADD_Common <bits<32> inst> : R600_3OP <
  inst, "MULADD",
  []> {
  let AMDILOp = AMDILInst.MAD_f32;
}

class CNDE_Common <bits<32> inst> : R600_3OP <
  inst, "CNDE",
  []> {
  let AMDILOp = AMDILInst.CMOVLOG_f32;
}

class CNDGT_Common <bits<32> inst> : R600_3OP <
  inst, "CNDGT",
  []
>;
  
class CNDGE_Common <bits<32> inst> : R600_3OP <
  inst, "CNDGE",
  [(set R600_Reg32:$dst, (int_AMDISA_cndlt R600_Reg32:$src0, R600_Reg32:$src2, R600_Reg32:$src1))]
>;

class DOT4_Common <bits<32> inst> : R600_REDUCTION <
  inst,
  (ins REPL:$src0, REPL:$src1),
  "DOT4 $dst $src0, $src1",
  [(set R600_Reg32:$dst, (int_AMDISA_dp4 REPL:$src0, REPL:$src1))]
>;

class EXP_IEEE_Common <bits<32> inst> : R600_1OP <
  inst, "EXP_IEEE",
  []> {
  let AMDILOp = AMDILInst.EXP_f32;
}

class FLT_TO_INT_Common <bits<32> inst> : R600_1OP <
  inst, "FLT_TO_INT", []> {
  let AMDILOp = AMDILInst.FTOI;
}

class INT_TO_FLT_Common <bits<32> inst> : R600_1OP <
  inst, "INT_TO_FLT", []> {
  let AMDILOp = AMDILInst.ITOF;
}

class LOG_CLAMPED_Common <bits<32> inst> : R600_1OP <
  inst, "LOG_CLAMPED",
  []
>;

class LOG_IEEE_Common <bits<32> inst> : R600_1OP <
  inst, "LOG_IEEE",
  []> {
  let AMDILOp = AMDILInst.LOG_f32;
}

class RECIP_CLAMPED_Common <bits<32> inst> : R600_1OP <
  inst, "RECIP_CLAMPED",
  []
>;

class RECIP_IEEE_Common <bits<32> inst> : R600_1OP <
  inst, "RECIP_IEEE",
  [(set R600_Reg32:$dst, (int_AMDISA_rcp R600_Reg32:$src))]> {
  let AMDILOp = AMDILInst.RSQ_f32;
}

class RECIPSQRT_CLAMPED_Common <bits<32> inst> : R600_1OP <
  inst, "RECIPSQRT_CLAMPED",
  [(set R600_Reg32:$dst, (int_AMDISA_rsq R600_Reg32:$src))]
>;

class RECIPSQRT_IEEE_Common <bits<32> inst> : R600_1OP <
  inst, "RECIPSQRT_IEEE",
  []
>;

class SIN_Common <bits<32> inst> : R600_1OP <
  inst, "SIN",
  []>{
  let AMDILOp = AMDILInst.SIN_f32;
  let Trig = 1;
}

class COS_Common <bits<32> inst> : R600_1OP <
  inst, "COS",
  []> {
  let AMDILOp = AMDILInst.COS_f32;
  let Trig = 1;
}

/* Helper patterns for complex intrinsics */
/* -------------------------------------- */

class DIV_Common <InstR600 recip_ieee> : Pat<
  (int_AMDISA_div R600_Reg32:$src0, R600_Reg32:$src1),
  (MUL R600_Reg32:$src0, (recip_ieee R600_Reg32:$src1))
>;

class LRP_Common <InstR600 muladd> : Pat <
  (int_AMDISA_lrp R600_Reg32:$src0, R600_Reg32:$src1, R600_Reg32:$src2),
  (muladd R600_Reg32:$src0, R600_Reg32:$src1, (MUL (SUB_f32 ONE, R600_Reg32:$src0), R600_Reg32:$src2))
>;

class POW_Common <InstR600 log_ieee, InstR600 exp_ieee> : Pat <
  (int_AMDISA_pow R600_Reg32:$src0, R600_Reg32:$src1),
  (exp_ieee (MUL R600_Reg32:$src1, (log_ieee R600_Reg32:$src0)))
>;

class SSG_Common <InstR600 cndgt, InstR600 cndge> : Pat <
  (int_AMDISA_ssg R600_Reg32:$src),
  (cndgt R600_Reg32:$src, ONE, (cndge R600_Reg32:$src, ZERO, NEG_ONE))
>;

class TGSI_LIT_Z_Common <InstR600 mul_lit, InstR600 log_clamped, InstR600 exp_ieee> : Pat <
  (int_TGSI_lit_z R600_Reg32:$src_x, R600_Reg32:$src_y, R600_Reg32:$src_w),
  (exp_ieee (mul_lit (log_clamped (MAX R600_Reg32:$src_y, ZERO)), R600_Reg32:$src_w, R600_Reg32:$src_x))
>;

/* ---------------------- */
/* R600 / R700 Only Instructions */
/* ---------------------- */

let Predicates = [isR600] in {

let Gen = AMDISAGen.R600 in {

  def MUL_LIT_r600 : MUL_LIT_Common<0x0C>;
  def MULADD_r600 : MULADD_Common<0x10>;
  def CNDE_r600 : CNDE_Common<0x18>;
  def CNDGT_r600 : CNDGT_Common<0x19>;
  def CNDGE_r600 : CNDGE_Common<0x1A>;
  def DOT4_r600 : DOT4_Common<0x50>;
  def EXP_IEEE_r600 : EXP_IEEE_Common<0x61>;
  def LOG_CLAMPED_r600 : LOG_CLAMPED_Common<0x62>;
  def LOG_IEEE_r600 : LOG_IEEE_Common<0x63>;
  def RECIP_CLAMPED_r600 : RECIP_CLAMPED_Common<0x64>;
  def RECIP_IEEE_r600 : RECIP_IEEE_Common<0x66>;
  def RECIPSQRT_CLAMPED_r600 : RECIPSQRT_CLAMPED_Common<0x67>;
  def RECIPSQRT_IEEE_r600 : RECIPSQRT_IEEE_Common<0x69>;
  def FLT_TO_INT_r600 : FLT_TO_INT_Common<0x6b>;
  def INT_TO_FLT_r600 : INT_TO_FLT_Common<0x6c>;
  def SIN_r600 : SIN_Common<0x6E>;
  def COS_r600 : COS_Common<0x6F>;

} // End AMDISAGen.R600

  def DIV_r600 : DIV_Common<RECIP_IEEE_r600>;
  def LRP_r600 : LRP_Common<MULADD_r600>;
  def POW_r600 : POW_Common<LOG_IEEE_r600, EXP_IEEE_r600>;
  def SSG_r600 : SSG_Common<CNDGT_r600, CNDGE_r600>;
  def TGSI_LIT_Z_r600 : TGSI_LIT_Z_Common<MUL_LIT_r600, LOG_CLAMPED_r600, EXP_IEEE_r600>;

}

/* ----------------- */
/* R700+ Trig helper */
/* ----------------- */

/*
class TRIG_HELPER_r700 <InstR600 trig_inst>: Pat <
  (trig_inst R600_Reg32:$src),
  (trig_inst (fmul R600_Reg32:$src, (PI))))
>;
*/

/* ------------------------------- */
/* Evergreen / Cayman Instructions */
/* ------------------------------- */

let Predicates = [isEGorCayman] in {
  
class TRIG_eg <InstR600 trig, Intrinsic intr> : Pat<
  (intr R600_Reg32:$src),
  (trig (MUL (MOV (LOADCONST_i32 CONST.TWO_PI_INV)), R600_Reg32:$src))
>;

let Gen = AMDISAGen.EG_CAYMAN in {

  def MULADD_eg : MULADD_Common<0x14>;
  def CNDE_eg : CNDE_Common<0x19>;
  def CNDGT_eg : CNDGT_Common<0x1A>;
  def CNDGE_eg : CNDGE_Common<0x1B>;
  def MUL_LIT_eg : MUL_LIT_Common<0x1F>;
  def FLT_TO_INT_eg : FLT_TO_INT_Common<0x50>;
  def EXP_IEEE_eg : EXP_IEEE_Common<0x81>;
  def LOG_CLAMPED_eg : LOG_CLAMPED_Common<0x82>;
  def LOG_IEEE_eg : LOG_IEEE_Common<0x83>;
  def RECIP_CLAMPED_eg : RECIP_CLAMPED_Common<0x84>;
  def RECIP_IEEE_eg : RECIP_IEEE_Common<0x86>;
  def RECIPSQRT_CLAMPED_eg : RECIPSQRT_CLAMPED_Common<0x87>;
  def RECIPSQRT_IEEE_eg : RECIPSQRT_IEEE_Common<0x89>;
  def SIN_eg : SIN_Common<0x8D>;
  def COS_eg : COS_Common<0x8E>;
  def INT_TO_FLT_eg : INT_TO_FLT_Common<0x9B>;
  def DOT4_eg : DOT4_Common<0xBE>;

} // End AMDISAGen.EG_CAYMAN

  def DIV_eg : DIV_Common<RECIP_IEEE_eg>;
  def LRP_eg : LRP_Common<MULADD_eg>;
  def POW_eg : POW_Common<LOG_IEEE_eg, EXP_IEEE_eg>;
  def SSG_eg : SSG_Common<CNDGT_eg, CNDGE_eg>;
  def TGSI_LIT_Z_eg : TGSI_LIT_Z_Common<MUL_LIT_eg, LOG_CLAMPED_eg, EXP_IEEE_eg>;

  def : TRIG_eg <SIN_eg, int_AMDISA_sin>;
  def : TRIG_eg <COS_eg, int_AMDISA_cos>;

}
/* Other Instructions */

let isCodeGenOnly = 1 in {

  def ARL : AMDISAShaderInst <
    (outs RELADDR:$dst),
    (ins R600_Reg32:$src),
    "ARL $dst, $src",
    [(set RELADDR:$dst, (int_AMDISA_arl R600_Reg32:$src))]
  >;

  def SWIZZLE : AMDISAShaderInst <
    (outs GPRV4F32:$dst),
    (ins GPRV4F32:$src0, i32imm:$src1),
    "SWIZZLE $dst, $src0, $src1",
    [(set GPRV4F32:$dst, (int_AMDISA_swizzle GPRV4F32:$src0, imm:$src1))]
  >;


  def LAST : AMDISAShaderInst <
    (outs),
    (ins),
    "LAST",
    []
  >;

  def GET_CHAN : AMDISAShaderInst <
    (outs R600_Reg32:$dst),
    (ins REPL:$src0, i32imm:$src1),
    "GET_CHAN $dst, $src0, $src1",
    []
  >;

  def SET_CHAN : AMDISAShaderInst <
    (outs REPL:$dst),
    (ins R600_Reg32:$src0, i32imm:$src1),
    "SET_CHAN $dst, $src0, $src1",
    []
  >;

  def MULLIT : AMDISAShaderInst <
    (outs REPL:$dst),
    (ins R600_Reg32:$src0, R600_Reg32:$src1, R600_Reg32:$src2),
    "MULLIT $dst, $src0, $src1",
    [(set REPL:$dst, (int_AMDISA_mullit R600_Reg32:$src0, R600_Reg32:$src1, R600_Reg32:$src2))]
  >;

}

include "R600ShaderPatterns.td"

// We need this pattern to avoid having real registers in PHI nodes.
// For some reason this pattern only works when it comes after the other
// instruction defs.
def : Pat <
  (int_R600_load_input imm:$src),
  (LOAD_INPUT imm:$src)
>;

} // End isR600toCayman Predicate
