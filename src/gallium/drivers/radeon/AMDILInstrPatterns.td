//===- AMDILInstrPatterns.td - AMDIL Target ------------===//
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
//===--------------------------------------------------------------------===//
// This file holds all the custom patterns that are used by the amdil backend
//
//===--------------------------------------------------------------------===//
//===--------------------------------------------------------------------===//
// Custom patterns for conversion operations
//===--------------------------------------------------------------------===////
// Pattern to remap integer or to IL_or
def : Pat<(i32 (or GPRI32:$src0, GPRI32:$src1)),
          (i32 (BINARY_OR_i32 GPRI32:$src0, GPRI32:$src1))>;
// float ==> long patterns
// unsigned: f32 -> i64
def FTOUL : Pat<(i64 (fp_to_uint GPRF32:$src)),
    (LCREATE (FTOU GPRF32:$src), (LOADCONST_i32 0))>;
// signed: f32 -> i64
def FTOL : Pat<(i64 (fp_to_sint GPRF32:$src)),
    (LCREATE (FTOI GPRF32:$src), (LOADCONST_i32 0))>;
// unsigned: i64 -> f32
def ULTOF : Pat<(f32 (uint_to_fp GPRI64:$src)),
    (UTOF (LLO GPRI64:$src))>;
// signed: i64 -> f32
def LTOF : Pat<(f32 (sint_to_fp GPRI64:$src)),
    (ITOF (LLO GPRI64:$src))>;

// integer subtraction
// a - b ==> a + (-b)
def SUB_i8 : Pat<(sub GPRI8:$src0, GPRI8:$src1),
    (ADD_i8 GPRI8:$src0, (NEGATE_i8 GPRI8:$src1))>;
def SUB_v2i8 : Pat<(sub GPRV2I8:$src0, GPRV2I8:$src1),
    (ADD_v2i8 GPRV2I8:$src0, (NEGATE_v2i8 GPRV2I8:$src1))>;
def SUB_v4i8 : Pat<(sub GPRV4I8:$src0, GPRV4I8:$src1),
    (ADD_v4i8 GPRV4I8:$src0, (NEGATE_v4i8 GPRV4I8:$src1))>;
def SUB_i16 : Pat<(sub GPRI16:$src0, GPRI16:$src1),
    (ADD_i16 GPRI16:$src0, (NEGATE_i16 GPRI16:$src1))>;
def SUB_v2i16 : Pat<(sub GPRV2I16:$src0, GPRV2I16:$src1),
    (ADD_v2i16 GPRV2I16:$src0, (NEGATE_v2i16 GPRV2I16:$src1))>;
def SUB_v4i16 : Pat<(sub GPRV4I16:$src0, GPRV4I16:$src1),
    (ADD_v4i16 GPRV4I16:$src0, (NEGATE_v4i16 GPRV4I16:$src1))>;
def SUB_i32 : Pat<(sub GPRI32:$src0, GPRI32:$src1),
    (ADD_i32 GPRI32:$src0, (NEGATE_i32 GPRI32:$src1))>;
def SUB_v2i32 : Pat<(sub GPRV2I32:$src0, GPRV2I32:$src1),
    (ADD_v2i32 GPRV2I32:$src0, (NEGATE_v2i32 GPRV2I32:$src1))>;
def SUB_v4i32 : Pat<(sub GPRV4I32:$src0, GPRV4I32:$src1),
    (ADD_v4i32 GPRV4I32:$src0, (NEGATE_v4i32 GPRV4I32:$src1))>;
// LLVM isn't lowering this correctly, so writing a pattern that
// matches it isntead.
def : Pat<(build_vector (i32 imm:$src)),
    (VCREATE_v4i32 (LOADCONST_i32 imm:$src))>;

// Calls:
def : Pat<(IL_call tglobaladdr:$dst),
    (CALL tglobaladdr:$dst)>;
def : Pat<(IL_call texternalsym:$dst),
    (CALL texternalsym:$dst)>;
def : Pat<(IL_call tconstpool:$dst),
  (CALL tconstpool:$dst)>;

include "AMDILConversions.td"
