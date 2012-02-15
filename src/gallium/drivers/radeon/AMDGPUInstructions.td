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

include "AMDGPUInstrEnums.td"

class AMDGPUInst <dag outs, dag ins, string asm, list<dag> pattern> : Instruction {
  field bits<16> AMDILOp = 0;
  field bits<3> Gen = 0;
  field bit PreloadReg = 0;

  let Namespace = "AMDIL";
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asm;
  let Pattern = pattern;
  let TSFlags{32} = PreloadReg;
  let TSFlags{42-40} = Gen;
  let TSFlags{63-48} = AMDILOp;
}

class AMDGPUShaderInst <dag outs, dag ins, string asm, list<dag> pattern>
    : AMDGPUInst<outs, ins, asm, pattern> {

  field bits<32> Inst = 0xffffffff;

}

let isCodeGenOnly = 1 in {

  def EXPORT_REG : AMDGPUShaderInst <
    (outs),
    (ins GPRF32:$src),
    "EXPORT_REG $src",
    [(int_AMDGPU_export_reg GPRF32:$src)]
  >;

  def LOAD_INPUT : AMDGPUShaderInst <
    (outs GPRF32:$dst),
    (ins i32imm:$src),
    "LOAD_INPUT $dst, $src",
    [] >{
    let PreloadReg = 1;
  }

  def MASK_WRITE : AMDGPUShaderInst <
    (outs),
    (ins GPRF32:$src),
    "MASK_WRITE $src",
    []
  >;

  def RESERVE_REG : AMDGPUShaderInst <
    (outs GPRF32:$dst),
    (ins i32imm:$src),
    "RESERVE_REG $dst, $src",
    [(set GPRF32:$dst, (int_AMDGPU_reserve_reg imm:$src))]> {
    let PreloadReg = 1;
  }

  def STORE_OUTPUT: AMDGPUShaderInst <
    (outs GPRF32:$dst),
    (ins GPRF32:$src0, i32imm:$src1),
    "STORE_OUTPUT $dst, $src0, $src1",
    [(set GPRF32:$dst, (int_AMDGPU_store_output GPRF32:$src0, imm:$src1))]
  >;
}

include "R600Instructions.td"


