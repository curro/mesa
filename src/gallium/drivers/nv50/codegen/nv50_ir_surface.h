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

#ifndef __NV50_IR_SURFACE_H__
#define __NV50_IR_SURFACE_H__

#include <vector>

#include "nv50_ir.h"
#include "nv50_ir_build_util.h"

namespace nv50_ir {

class NV50SurfaceLowering : public Pass {
private:
   Value *getConfig(TexInstruction *i, unsigned off, int c = -1);

   void load(TexInstruction *i, int order, std::vector<Value *> &y);
   void store(TexInstruction *i, int order, const std::vector<Value *> &x);

   void pack(TexInstruction *i, int layout, std::vector<Value *> &y,
             const std::vector<Value *> &x);
   void unpack(TexInstruction *i, int layout, std::vector<Value *> &y,
               std::vector<Value *> &sz, std::vector<Value *> &f,
               const std::vector<Value *> &x);

   void convert(TexInstruction *i, int c, int cvt, Value *scale,
                std::vector<Value *> &y, const std::vector<Value *> &x);
   void deconvert(TexInstruction *i, int c, int cvt, std::vector<Value *> &y,
                  const std::vector<Value *> &x, const std::vector<Value *> &sz,
                  const std::vector<Value *> &f);

   bool handleSULDB(TexInstruction *i);
   bool handleSUSTB(TexInstruction *i);
   bool handleSULDP(TexInstruction *i);
   bool handleSUSTP(TexInstruction *i);

   virtual bool visit(Instruction *i);

   BuildUtil bld;
};

}

#endif
