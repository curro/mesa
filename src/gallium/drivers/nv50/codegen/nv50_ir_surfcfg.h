/*
 * Copyright 2011 Francisco Jerez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __NV50_IR_SURFCFG_H__
#define __NV50_IR_SURFCFG_H__

#define NV50_IR_SURFCFG_WIDTH__OFF              0x00

#define NV50_IR_SURFCFG_HEIGHT__OFF             0x04

#define NV50_IR_SURFCFG_LAYOUT__OFF             0x08
#define NV50_IR_SURFCFG_LAYOUT_X5Y6Z5           0
#define NV50_IR_SURFCFG_LAYOUT_X5Y5Z5W1         1
#define NV50_IR_SURFCFG_LAYOUT_X4Y4Z4W4         2
#define NV50_IR_SURFCFG_LAYOUT_X1Y5Z5W5         3
#define NV50_IR_SURFCFG_LAYOUT_X24Y8            4
#define NV50_IR_SURFCFG_LAYOUT_X8Y24            5
#define NV50_IR_SURFCFG_LAYOUT_X8Y8Z8W8         6
#define NV50_IR_SURFCFG_LAYOUT_X16Y16Z16W16     7
#define NV50_IR_SURFCFG_LAYOUT_X32Y32Z32W32     8
#define NV50_IR_SURFCFG_LAYOUT__NUM             9

#define NV50_IR_SURFCFG_CPP__OFF                0x0c

#define NV50_IR_SURFCFG_CVT__OFF                0x10
#define NV50_IR_SURFCFG_CVT(x, y, z, w) \
   ((NV50_IR_SURFCFG_CVT_##x) << 0 |    \
    (NV50_IR_SURFCFG_CVT_##y) << 8 |    \
    (NV50_IR_SURFCFG_CVT_##z) << 16 |   \
    (NV50_IR_SURFCFG_CVT_##w) << 24)
#define NV50_IR_SURFCFG_CVT_UINT                0
#define NV50_IR_SURFCFG_CVT_SINT                1
#define NV50_IR_SURFCFG_CVT_UNORM               2
#define NV50_IR_SURFCFG_CVT_SNORM               3
#define NV50_IR_SURFCFG_CVT_FLOAT               4
#define NV50_IR_SURFCFG_CVT_0                   5
#define NV50_IR_SURFCFG_CVT_1I                  6
#define NV50_IR_SURFCFG_CVT_1F                  7
#define NV50_IR_SURFCFG_CVT__NUM                8

#define NV50_IR_SURFCFG_SCALE__OFF              0x14
#define NV50_IR_SURFCFG_SCALE(x, y, z, w)       \
   (x << 0 | y << 8 | z << 16 | w << 24)
#define NV50_IR_SURFCFG_SCALE_X5Y6Z5            \
   NV50_IR_SURFCFG_SCALE(5, 6, 5, 0)
#define NV50_IR_SURFCFG_SCALE_X5Y5Z5W1          \
   NV50_IR_SURFCFG_SCALE(5, 5, 5, 1)
#define NV50_IR_SURFCFG_SCALE_X4Y4Z4W4          \
   NV50_IR_SURFCFG_SCALE(4, 4, 4, 4)
#define NV50_IR_SURFCFG_SCALE_X1Y5Z5W5          \
   NV50_IR_SURFCFG_SCALE(1, 5, 5, 5)
#define NV50_IR_SURFCFG_SCALE_X24Y8             \
   NV50_IR_SURFCFG_SCALE(24, 8, 0, 0)
#define NV50_IR_SURFCFG_SCALE_X8Y24             \
   NV50_IR_SURFCFG_SCALE(8, 24, 0, 0)
#define NV50_IR_SURFCFG_SCALE_X8Y8Z8W8          \
   NV50_IR_SURFCFG_SCALE(8, 8, 8, 8)
#define NV50_IR_SURFCFG_SCALE_X16Y16Z16W16      \
   NV50_IR_SURFCFG_SCALE(16, 16, 16, 16)
#define NV50_IR_SURFCFG_SCALE_X32Y32Z32W32      \
   NV50_IR_SURFCFG_SCALE(32, 32, 32, 32)

#define NV50_IR_SURFCFG_SWZ__OFF                0x18
#define NV50_IR_SURFCFG_SWZ(x, y, z, w) \
   ((NV50_IR_SURFCFG_SWZ_##x) << 0 |    \
    (NV50_IR_SURFCFG_SWZ_##y) << 8 |    \
    (NV50_IR_SURFCFG_SWZ_##z) << 16 |   \
    (NV50_IR_SURFCFG_SWZ_##w) << 24)
#define NV50_IR_SURFCFG_SWZ_X                   0
#define NV50_IR_SURFCFG_SWZ_Y                   1
#define NV50_IR_SURFCFG_SWZ_Z                   2
#define NV50_IR_SURFCFG_SWZ_W                   3
#define NV50_IR_SURFCFG_SWZ__NUM                4

#define NV50_IR_SURFCFG__SIZE                   0x20

#endif
