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

namespace AMDGPUInstrInfo {
  enum AMDILInstructions {
    NONE = 0,
    ADD_f32 = 1,
    ABS_f32 = 2,
    FRAC_f32 = 3,
    PIREDUCE_f32 = 4,
    ACOS_f32 = 5,
    ATAN_f32 = 6,
    ASIN_f32 = 7,
    TAN_f32 = 8,
    SIN_f32 = 9,
    COS_f32 = 10,
    SQRT_f32 = 11,
    EXP_f32 = 12,
    EXPVEC_f32 = 13,
    SQRTVEC_f32 = 14,
    COSVEC_f32 = 15,
    SINVEC_f32 = 16,
    LOGVEC_f32 = 17,
    RSQVEC_f32 = 18,
    EXN_f32 = 19,
    SIGN_f32 = 20,
    LENGTH_f32 = 21,
    POW_f32 = 22,
    MIN_f32 = 23,
    MAX_f32 = 24,
    MAD_f32 = 25,
    LN_f32 = 26,
    LOG_f32 = 27,
    RSQ_f32 = 28,
    DIV_f32 = 29,
    CLAMP_f32 = 30,
    FMA_f32 = 31,
    LERP_f32 = 32,
    NEG_f32 = 33,
    INTTOANY_f32 = 34,
  };
}
namespace AMDGPUInstrInfo {
  enum AMDGPUGen {
    R600_CAYMAN = 0,
    EG_CAYMAN = 1,
    CAYMAN = 2,
  };
}
