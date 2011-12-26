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

#include "nv50_ir_surface.h"
#include "nv50_ir_surfcfg.h"
#include "nv50_ir.h"

namespace nv50_ir {

Value *
NV50SurfaceLowering::getConfig(TexInstruction *i, unsigned off, int c)
{
   Value *res = bld.loadImm(NULL, i->tex.r);
   Value *addr = bld.getScratch(4, FILE_ADDRESS);
   Value *v = bld.getScratch();
   Symbol *mem;

   if (i->getIndirectR())
      bld.mkOp2(OP_ADD, TYPE_U32, res, res, i->getIndirectR());

   bld.mkOp2(OP_SHL, TYPE_U32, addr, res, bld.mkImm(5));

   if (c >= 0) {
      Value *w = bld.getScratch(1);
      mem = bld.mkSymbol(FILE_MEMORY_CONST, 0xf, TYPE_U8, off + c);
      bld.mkLoad(TYPE_U8, w, mem, addr);
      bld.mkCvt(OP_CVT, TYPE_U32, v, TYPE_U8, w);
   } else {
      mem = bld.mkSymbol(FILE_MEMORY_CONST, 0xf, TYPE_U32, off);
      bld.mkLoad(TYPE_U32, v, mem, addr);
   }

   if (off == NV50_IR_SURFCFG_SCALE__OFF) {
      bld.mkOp2(OP_SHL, TYPE_U32, v, bld.loadImm(NULL, 1), v);
      bld.mkOp2(OP_SUB, TYPE_U32, v, v, bld.loadImm(NULL, 1));
   }

   return v;
}

void
NV50SurfaceLowering::load(TexInstruction *i, int order,
                          std::vector<Value *> &y)
{
   int sz = 1 << order;
   std::vector<Value *> def(1), src;
   TexInstruction *ld;

   for (unsigned j = 0; j < i->tex.target.getArgCount(); ++j)
      src.push_back(i->getSrc(j));

   src[0] = bld.mkOp2v(OP_SHL, TYPE_U32, bld.getScratch(),
                       src[0], bld.mkImm(order));

   if (sz < 4) {
      def[0] = bld.getScratch(sz);
      ld = bld.mkTex(OP_SULDB, i->tex.target, i->tex.r, -1, def, src);
      ld->setIndirectR(i->getIndirectR());
      ld->setType(typeOfSize(sz), TYPE_U32);
      handleSULDB(ld);
      bld.mkCvt(OP_CVT, TYPE_U32, y[0], typeOfSize(sz), def[0]);
   } else {
      for (int j = 0; j < sz / 4; ++j) {
         def[0] = y[j];
         ld = bld.mkTex(OP_SULDB, i->tex.target, i->tex.r, -1, def, src);
         ld->setIndirectR(i->getIndirectR());
         handleSULDB(ld);
         bld.mkOp2(OP_ADD, TYPE_U32, src[0], src[0], bld.mkImm(4));
      }
   }
}

void
NV50SurfaceLowering::store(TexInstruction *i, int order,
                           const std::vector<Value *> &x)
{
   int sz = 1 << order;
   int s = i->tex.target.getArgCount();
   std::vector<Value *> def, src(s + 1);
   TexInstruction *st;

   for (unsigned j = 0; j < i->tex.target.getArgCount(); ++j)
      src[j] = i->getSrc(j);

   src[0] = bld.mkOp2v(OP_SHL, TYPE_U32, bld.getScratch(),
                       src[0], bld.mkImm(order));

   if (sz < 4) {
      src[s] = x[0];
      st = bld.mkTex(OP_SUSTB, i->tex.target, i->tex.r, -1, def, src);
      st->setIndirectR(i->getIndirectR());
      st->setType(typeOfSize(sz), TYPE_U32);
      handleSUSTB(st);
   } else {
      for (int j = 0; j < sz / 4; ++j) {
         src[s] = x[j];
         st = bld.mkTex(OP_SUSTB, i->tex.target, i->tex.r, -1, def, src);
         st->setIndirectR(i->getIndirectR());
         handleSUSTB(st);
         bld.mkOp2(OP_ADD, TYPE_U32, src[0], src[0], bld.mkImm(4));
      }
   }
}

static const struct Layout {
   int idx;
   int off;
   int sz;
} layouts[NV50_IR_SURFCFG_LAYOUT__NUM][5] = {
   { { 0, 0, 5 }, { 0, 5, 6 }, { 0, 11, 5 } },
   { { 0, 0, 5 }, { 0, 5, 5 }, { 0, 10, 5 }, { 0, 15, 1 } },
   { { 0, 0, 4 }, { 0, 4, 4 }, { 0, 8, 4 }, { 0, 12, 4 } },
   { { 0, 0, 1 }, { 0, 1, 5 }, { 0, 6, 5 }, { 0, 11, 5 } },
   { { 0, 0, 24 }, { 0, 24, 8 } },
   { { 0, 0, 8 }, { 0, 8, 24 } },
   { { 0, 0, 8 }, { 0, 8, 8 }, { 0, 16, 8 }, { 0, 24, 8 } },
   { { 0, 0, 16 }, { 0, 16, 16 }, { 1, 0, 16 }, { 1, 16, 16 } },
   { { 0, 0, 32 }, { 1, 0, 32 }, { 2, 0, 32 }, { 3, 0, 32 } },
};

void
NV50SurfaceLowering::pack(TexInstruction *i, int layout,
                          std::vector<Value *> &y,
                          const std::vector<Value *> &x)
{
   Value *v = bld.getScratch();
   const Layout *l;

   for (int c = 0; c < 4; ++c)
      bld.loadImm(y[c], 0);

   for (int c = 0; (l = &layouts[layout][c])->sz; ++c) {
      bld.mkOp2(OP_AND, TYPE_U32, v, x[c],
                bld.mkImm((uint32_t)((1ull << l->sz) - 1)));
      bld.mkOp2(OP_SHL, TYPE_U32, v, v, bld.mkImm(l->off));
      bld.mkOp2(OP_OR, TYPE_U32, y[l->idx], y[l->idx], v);
   }
}

void
NV50SurfaceLowering::unpack(TexInstruction *i, int layout,
                            std::vector<Value *> &y,
                            std::vector<Value *> &sz,
                            std::vector<Value *> &f,
                            const std::vector<Value *> &x)
{
   const Layout *l;

   for (int c = 0; (l = &layouts[layout][c])->sz; ++c) {
      bld.loadImm(sz[c], l->sz);
      bld.loadImm(f[c], 1.0f / ((1ull << l->sz) - 1));
      bld.mkOp2(OP_AND, TYPE_U32, y[c], x[l->idx],
                bld.mkImm((uint32_t)((1ull << l->sz) - 1) << l->off));
      bld.mkOp2(OP_SHR, TYPE_U32, y[c], y[c], bld.mkImm(l->off));
   }
}

void
NV50SurfaceLowering::convert(TexInstruction *i, int c, int cvt,
                             Value *scale, std::vector<Value *> &y,
                             const std::vector<Value *> &x)
{
   bool float_cvt = (cvt == NV50_IR_SURFCFG_CVT_FLOAT);
   bool signed_cvt = (cvt == NV50_IR_SURFCFG_CVT_SNORM ||
                      cvt == NV50_IR_SURFCFG_CVT_SINT);
   bool unsigned_cvt = (cvt == NV50_IR_SURFCFG_CVT_UNORM ||
                        cvt == NV50_IR_SURFCFG_CVT_UINT);
   bool norm_cvt = (cvt == NV50_IR_SURFCFG_CVT_SNORM ||
                    cvt == NV50_IR_SURFCFG_CVT_UNORM);

   if (float_cvt) {
      bld.mkMov(y[c], x[c]);

   } else if (signed_cvt || unsigned_cvt) {
      Value *min, *max, *p = bld.getScratch(1, FILE_FLAGS);
      DataType dty = (signed_cvt ? TYPE_S32 : TYPE_U32);
      DataType sty = (norm_cvt ? TYPE_F32 : dty);

      if (signed_cvt)
         bld.mkOp2(OP_SHR, TYPE_U32, scale, scale, bld.mkImm(1));

      if (norm_cvt) {
         min = bld.loadImm(NULL, unsigned_cvt ? 0.0f : -1.0f);
         max = bld.loadImm(NULL, 1.0f);
      } else {
         min = (unsigned_cvt ? bld.loadImm(NULL, 0) :
                bld.mkOp1v(OP_NOT, TYPE_U32, bld.getScratch(), scale));
         max = scale;
      }

      bld.mkCmp(OP_SET, CC_LT, sty, p, x[c], min);
      bld.mkOp2(OP_SELP, sty, y[c], min, x[c])->setPredicate(CC_NE, p);
      bld.mkCmp(OP_SET, CC_GT, sty, p, y[c], max);
      bld.mkOp2(OP_SELP, sty, y[c], max, y[c])->setPredicate(CC_NE, p);

      if (norm_cvt) {
         bld.mkCvt(OP_CVT, TYPE_F32, scale, TYPE_U32, scale);
         bld.mkOp2(OP_MUL, TYPE_F32, y[c], y[c], scale);
         bld.mkCvt(OP_CVT, dty, y[c], TYPE_F32, y[c]);
      }
   }
}

void
NV50SurfaceLowering::deconvert(TexInstruction *i, int c, int cvt,
                               std::vector<Value *> &y,
                               const std::vector<Value *> &x,
                               const std::vector<Value *> &sz,
                               const std::vector<Value *> &f)
{
   if (cvt == NV50_IR_SURFCFG_CVT_0)
      bld.loadImm(y[c], 0);
   else if (cvt == NV50_IR_SURFCFG_CVT_1I)
      bld.loadImm(y[c], 1);
   else if (cvt == NV50_IR_SURFCFG_CVT_1F)
      bld.loadImm(y[c], 1.0f);
   else
      bld.mkMov(y[c], x[c]);

   if (cvt == NV50_IR_SURFCFG_CVT_SINT ||
       cvt == NV50_IR_SURFCFG_CVT_SNORM) {
      bld.mkOp2(OP_SUB, TYPE_U32, sz[c], bld.loadImm(NULL, 32), sz[c]);
      bld.mkOp2(OP_SHL, TYPE_U32, y[c], y[c], sz[c]);
      bld.mkOp2(OP_SHR, TYPE_S32, y[c], y[c], sz[c]);
   }

   if (cvt == NV50_IR_SURFCFG_CVT_UNORM) {
      bld.mkCvt(OP_CVT, TYPE_F32, y[c], TYPE_U32, y[c]);
      bld.mkOp2(OP_MUL, TYPE_F32, y[c], y[c], f[c]);
   } else if (cvt == NV50_IR_SURFCFG_CVT_SNORM) {
      bld.mkCvt(OP_CVT, TYPE_F32, y[c], TYPE_S32, y[c]);
      bld.mkOp2(OP_MUL, TYPE_F32, f[c], f[c], bld.mkImm(2.0f));
      bld.mkOp2(OP_MUL, TYPE_F32, y[c], y[c], f[c]);
   }
}

bool
NV50SurfaceLowering::handleSULDB(TexInstruction *i)
{
   Value *off = i->getSrc(0);

   if (i->tex.target == TEX_TARGET_2D) {
      off = bld.getScratch();
      bld.mkOp2(OP_SHL, TYPE_U32, off, i->getSrc(1), bld.mkImm(16));
      bld.mkOp2(OP_OR, TYPE_U32, off, off, i->getSrc(0));
   }

   bld.mkLoad(i->dType, i->getDef(0),
              new_Symbol(prog, FILE_MEMORY_GLOBAL, i->tex.r),
              off, i->getIndirectR());
   delete_Instruction(prog, i);

   return true;
}

bool
NV50SurfaceLowering::handleSUSTB(TexInstruction *i)
{
   int s = i->tex.target.getArgCount();
   Value *off = i->getSrc(0);

   if (i->tex.target == TEX_TARGET_2D) {
      off = bld.getScratch();
      bld.mkOp2(OP_SHL, TYPE_U32, off, i->getSrc(1), bld.mkImm(16));
      bld.mkOp2(OP_OR, TYPE_U32, off, off, i->getSrc(0));
   }

   bld.mkStore(OP_STORE, i->dType,
               new_Symbol(prog, FILE_MEMORY_GLOBAL, i->tex.r),
               off, i->getSrc(s), i->getIndirectR());
   delete_Instruction(prog, i);

   return true;
}

bool
NV50SurfaceLowering::handleSULDP(TexInstruction *i)
{
   std::vector<Value *> vsz = bld.getScratchv(4), vf = bld.getScratchv(4),
      vraw = bld.getScratchv(4), vup = bld.getScratchv(4);
   BasicBlock *tbb, *fbb, *exit;

   bld.setPosition(i, false);
   exit = i->bb->splitAfter(i);
   bld.mkFlow(OP_JOINAT, exit, CC_ALWAYS, NULL);

   // Fetch the image data.
   Value *cpp = getConfig(i, NV50_IR_SURFCFG_CPP__OFF);

   for (int j = 0; j < 5; ++j) {
      bld.mkIfBlock(CC_EQ, TYPE_U32, cpp, bld.loadImm(NULL, 1 << j),
                    &tbb, &fbb);
      bld.setPosition(tbb, true);
      load(i, j, vraw);
      bld.setPosition(fbb, true);
   }

   // Unpack the image data.
   bld.setPosition(i, false);
   Value *l = getConfig(i, NV50_IR_SURFCFG_LAYOUT__OFF);

   for (int j = 0; j < NV50_IR_SURFCFG_LAYOUT__NUM; ++j) {
      bld.mkIfBlock(CC_EQ, TYPE_U32, l, bld.loadImm(NULL, j), &tbb, &fbb);
      bld.setPosition(tbb, true);
      unpack(i, j, vup, vsz, vf, vraw);
      bld.setPosition(fbb, true);
   }

   // Apply the specified channel conversion.
   for (int c = 0; i->defExists(c); ++c) {
      bld.setPosition(i, false);
      Value *cvt = getConfig(i, NV50_IR_SURFCFG_CVT__OFF, c);

      for (int j = 0; j < NV50_IR_SURFCFG_CVT__NUM; ++j) {
         bld.mkIfBlock(CC_EQ, TYPE_U32, cvt, bld.loadImm(NULL, j), &tbb, &fbb);
         bld.setPosition(tbb, true);
         deconvert(i, c, j, vup, vup, vsz, vf);
         bld.setPosition(fbb, true);
      }
   }

   // Apply the specified swizzle.
   for (int c = 0; i->defExists(c); ++c) {
      bld.setPosition(i, false);
      Value *swz = getConfig(i, NV50_IR_SURFCFG_SWZ__OFF, c);

      for (int j = 0; j < NV50_IR_SURFCFG_SWZ__NUM; ++j) {
         bld.mkIfBlock(CC_EQ, TYPE_U32, swz, bld.loadImm(NULL, j), &tbb, &fbb);
         bld.setPosition(tbb, true);
         bld.mkMov(i->getDef(c), vup[j]);
         bld.setPosition(fbb, true);
      }
   }

   bld.setPosition(exit, false);
   bld.mkFlow(OP_JOIN, NULL, CC_ALWAYS, NULL)->fixed = 1;
   delete_Instruction(prog, i);

   return true;
}

bool
NV50SurfaceLowering::handleSUSTP(TexInstruction *i)
{
   std::vector<Value *> vraw = bld.getScratchv(4), vs = bld.getScratchv(4);
   int s = i->tex.target.getArgCount();
   BasicBlock *tbb, *fbb, *exit;

   bld.setPosition(i, false);
   exit = i->bb->splitAfter(i);
   bld.mkFlow(OP_JOINAT, exit, CC_ALWAYS, NULL);

   // Apply the specified swizzle.
   for (int c = 0; i->srcExists(s + c); ++c) {
      bld.setPosition(i, false);
      Value *swz = getConfig(i, NV50_IR_SURFCFG_SWZ__OFF, c);

      for (int j = 0; j < NV50_IR_SURFCFG_SWZ__NUM; ++j) {
         bld.mkIfBlock(CC_EQ, TYPE_U32, swz, bld.loadImm(NULL, j), &tbb, &fbb);
         bld.setPosition(tbb, true);
         bld.mkMov(vs[j], i->getSrc(s + c));
         bld.setPosition(fbb, true);
      }
   }

   // Apply the specified channel conversion.
   for (int c = 0; i->srcExists(s + c); ++c) {
      bld.setPosition(i, false);
      Value *cvt = getConfig(i, NV50_IR_SURFCFG_CVT__OFF, c);
      Value *scale = getConfig(i, NV50_IR_SURFCFG_SCALE__OFF, c);

      for (int j = 0; j < NV50_IR_SURFCFG_CVT__NUM; ++j) {
         bld.mkIfBlock(CC_EQ, TYPE_U32, cvt, bld.loadImm(NULL, j), &tbb, &fbb);
         bld.setPosition(tbb, true);
         convert(i, c, j, scale, vs, vs);
         bld.setPosition(fbb, true);
      }
   }

   // Pack the image data.
   bld.setPosition(i, false);
   Value *l = getConfig(i, NV50_IR_SURFCFG_LAYOUT__OFF);

   for (int j = 0; j < NV50_IR_SURFCFG_LAYOUT__NUM; ++j) {
      bld.mkIfBlock(CC_EQ, TYPE_U32, l, bld.loadImm(NULL, j), &tbb, &fbb);
      bld.setPosition(tbb, true);
      pack(i, j, vraw, vs);
      bld.setPosition(fbb, true);
   }

   // Store the image data.
   bld.setPosition(i, false);
   Value *cpp = getConfig(i, NV50_IR_SURFCFG_CPP__OFF);

   for (int j = 0; j < 5; ++j) {
      bld.mkIfBlock(CC_EQ, TYPE_U32, cpp, bld.loadImm(NULL, 1 << j),
                    &tbb, &fbb);
      bld.setPosition(tbb, true);
      store(i, j, vraw);
      bld.setPosition(fbb, true);
   }

   bld.setPosition(exit, false);
   bld.mkFlow(OP_JOIN, NULL, CC_ALWAYS, NULL)->fixed = 1;
   delete_Instruction(prog, i);

   return true;
}

bool
NV50SurfaceLowering::visit(Instruction *i)
{
   bld.setPosition(i, false);

   switch (i->op) {
   case OP_SULDB:
      return handleSULDB(i->asTex());
   case OP_SUSTB:
      return handleSUSTB(i->asTex());
   case OP_SULDP:
      return handleSULDP(i->asTex());
   case OP_SUSTP:
      return handleSUSTP(i->asTex());
   default:
      break;
   }
   return true;
}

}
