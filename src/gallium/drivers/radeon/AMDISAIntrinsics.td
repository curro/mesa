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


let TargetPrefix = "AMDISA", isTarget = 1 in {

  def int_AMDISA_export_reg : Intrinsic<[], [llvm_float_ty], []>;
  def int_AMDISA_load_const : Intrinsic<[llvm_float_ty], [llvm_i32_ty], []>;
  def int_AMDISA_load_imm : Intrinsic<[llvm_v4f32_ty], [llvm_i32_ty], []>;
  def int_AMDISA_reserve_reg : Intrinsic<[llvm_float_ty], [llvm_i32_ty], []>;
  def int_AMDISA_store_output : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_i32_ty], []>;
  def int_AMDISA_swizzle : Intrinsic<[llvm_v4f32_ty], [llvm_v4f32_ty, llvm_i32_ty], []>;

  def int_AMDISA_arl : Intrinsic<[llvm_i32_ty], [llvm_float_ty], []>;
  def int_AMDISA_cndlt : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_cos : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_div : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_dp4 : Intrinsic<[llvm_float_ty], [llvm_v4f32_ty, llvm_v4f32_ty], []>;
  def int_AMDISA_floor : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_kill : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_kilp : Intrinsic<[], [], []>;
  def int_AMDISA_lrp : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_mul : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_pow : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_rcp : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_rsq : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_seq : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_sgt : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_sge : BinaryIntFloat;
  def int_AMDISA_sin : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_sle : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_sne : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_ssg : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
  def int_AMDISA_mullit : Intrinsic<[llvm_v4f32_ty], [llvm_float_ty, llvm_float_ty, llvm_float_ty], []>;
  def int_AMDISA_tex : Intrinsic<[llvm_v4f32_ty], [llvm_v4f32_ty, llvm_i32_ty, llvm_i32_ty], []>;
  def int_AMDISA_txb : Intrinsic<[llvm_v4f32_ty], [llvm_v4f32_ty, llvm_i32_ty, llvm_i32_ty], []>;
  def int_AMDISA_txd : Intrinsic<[llvm_v4f32_ty], [llvm_v4f32_ty, llvm_i32_ty, llvm_i32_ty], []>;
  def int_AMDISA_txl : Intrinsic<[llvm_v4f32_ty], [llvm_v4f32_ty, llvm_i32_ty, llvm_i32_ty], []>;
  def int_AMDISA_trunc : Intrinsic<[llvm_float_ty], [llvm_float_ty], []>;
}

let TargetPrefix = "TGSI", isTarget = 1 in {

  def int_TGSI_lit_z : Intrinsic<[llvm_float_ty], [llvm_float_ty, llvm_float_ty, llvm_float_ty],[]>;
}
