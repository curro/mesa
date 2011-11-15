/*
 * Copyright 2011 Christoph Bumiller
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

#include "nv50/codegen/nv50_ir.h"
#include "nv50/codegen/nv50_ir_build_util.h"

#include "nv50_ir_target_nv50.h"

namespace nv50_ir {

// nv50 doesn't support 32 bit integer multiplication
//
//       ah al * bh bl = LO32: (al * bh + ah * bl) << 16 + (al * bl)
// -------------------
//    al*bh 00           HI32: (al * bh + ah * bl) >> 16 + (ah * bh) +
// ah*bh 00 00                 (           carry1) << 16 + ( carry2)
//       al*bl
//    ah*bl 00
//
// fffe0001 + fffe0001
static bool
expandIntegerMUL(BuildUtil *bld, Instruction *mul)
{
   const bool highResult = mul->subOp == NV50_IR_SUBOP_MUL_HIGH;

   DataType fTy = mul->sType; // full type
   DataType hTy;
   switch (fTy) {
   case TYPE_S32: hTy = TYPE_S16; break;
   case TYPE_U32: hTy = TYPE_U16; break;
   case TYPE_U64: hTy = TYPE_U32; break;
   case TYPE_S64: hTy = TYPE_S32; break;
   default:
      return false;
   }
   unsigned int fullSize = typeSizeof(fTy);
   unsigned int halfSize = typeSizeof(hTy);

   Instruction *i[9];

   Value *a[2] = { bld->getSSA(halfSize), bld->getSSA(halfSize) };
   Value *b[2] = { bld->getSSA(halfSize), bld->getSSA(halfSize) };
   Value *c[2];
   Value *t[4];
   for (int j = 0; j < 4; ++j)
      t[j] = bld->getSSA(fullSize);

   (i[0] = bld->mkOp1(OP_SPLIT, fTy, a[0], mul->getSrc(0)))->setDef(1, a[1]);
   (i[1] = bld->mkOp1(OP_SPLIT, fTy, b[0], mul->getSrc(1)))->setDef(1, b[1]);

   i[2] = bld->mkOp2(OP_MUL, fTy, t[0], a[0], b[1]);
   i[3] = bld->mkOp3(OP_MAD, fTy, t[1], a[1], b[0], t[0]);
   i[7] = bld->mkOp2(OP_SHL, fTy, t[2], t[1], bld->mkImm(halfSize * 8));
   i[4] = bld->mkOp3(OP_MAD, fTy, t[3], a[0], b[0], t[2]);

   if (highResult) {
      Value *r[3];
      Value *imm = bld->loadImm(NULL, 1 << (halfSize * 8));
      c[0] = bld->getSSA(1, FILE_FLAGS);
      c[1] = bld->getSSA(1, FILE_FLAGS);
      for (int j = 0; j < 3; ++j)
         r[j] = bld->getSSA(fullSize);

      i[8] = bld->mkOp2(OP_SHR, fTy, r[0], t[1], bld->mkImm(halfSize * 8));
      i[6] = bld->mkOp2(OP_ADD, fTy, r[1], r[0], imm);
      bld->mkOp2(OP_UNION, TYPE_U32, r[2], r[1], r[0]);
      i[5] = bld->mkOp3(OP_MAD, fTy, mul->getDef(0), a[1], b[1], r[2]);

      // set carry defs / sources
      i[3]->setFlagsDef(1, c[0]);
      i[4]->setFlagsDef(0, c[1]); // actual result not required, just the carry
      i[6]->setPredicate(CC_C, c[0]);
      i[5]->setFlagsSrc(3, c[1]);
   } else {
      bld->mkMov(mul->getDef(0), t[3]);
   }
   delete_Instruction(bld->getProgram(), mul);

   for (int j = 2; j <= (highResult ? 5 : 4); ++j)
      i[j]->sType = hTy;

   return true;
}

#define QOP_ADD  0
#define QOP_SUBR 1
#define QOP_SUB  2
#define QOP_MOV2 3

#define QUADOP(q, r, s, t)            \
   ((QOP_##q << 0) | (QOP_##r << 2) | \
    (QOP_##s << 4) | (QOP_##t << 6))

class NV50LegalizePostRA : public Pass
{
private:
   virtual bool visit(Function *);
   virtual bool visit(BasicBlock *);

   void replaceZero(Instruction *);
   void split64BitOp(Instruction *);

   LValue *r63;
};

bool
NV50LegalizePostRA::visit(Function *fn)
{
   r63 = new_LValue(fn, FILE_GPR);
   r63->reg.data.id = 63;

   return true;
}

void
NV50LegalizePostRA::replaceZero(Instruction *i)
{
   for (int s = 0; i->srcExists(s); ++s) {
      ImmediateValue *imm = i->getSrc(s)->asImm();
      if (imm && imm->reg.data.u64 == 0)
         i->setSrc(s, r63);
   }
}

void
NV50LegalizePostRA::split64BitOp(Instruction *i)
{
   if (i->dType == TYPE_F64) {
      if (i->op == OP_MAD)
         i->op = OP_FMA;
      if (i->op == OP_ADD || i->op == OP_MUL || i->op == OP_FMA ||
          i->op == OP_CVT || i->op == OP_MIN || i->op == OP_MAX ||
          i->op == OP_SET)
         return;
      i->dType = i->sType = TYPE_U32;

      i->bb->insertAfter(i, cloneForward(func, i));
   }
}

bool
NV50LegalizePostRA::visit(BasicBlock *bb)
{
   Instruction *i, *next;

   // remove pseudo operations and non-fixed no-ops, split 64 bit operations
   for (i = bb->getFirst(); i; i = next) {
      next = i->next;
      if (i->isNop()) {
         bb->remove(i);
      } else {
         if (i->op != OP_MOV && i->op != OP_PFETCH)
            replaceZero(i);
         if (typeSizeof(i->dType) == 8)
            split64BitOp(i);
      }
   }
   if (!bb->getEntry())
      return true;

   return true;
}

class NV50LegalizeSSA : public Pass
{
public:
   virtual bool visit(BasicBlock *bb);

private:
   void propagateWriteToOutput(Instruction *);
};

void
NV50LegalizeSSA::propagateWriteToOutput(Instruction *st)
{
   // TODO
}

bool
NV50LegalizeSSA::visit(BasicBlock *bb)
{
   Instruction *insn, *next;
   for (insn = bb->getFirst(); insn; insn = next) {
      next = insn->next;
      if (insn->op == OP_EXPORT)
         propagateWriteToOutput(insn);
   }
   return true;
}

class NV50LoweringPreSSA : public Pass
{
public:
   NV50LoweringPreSSA(Program *);

private:
   virtual bool visit(Instruction *);
   virtual bool visit(Function *);

   bool handleRDSV(Instruction *);
   bool handleWRSV(Instruction *);

   bool handleEXPORT(Instruction *);

   bool handleMUL(Instruction *);
   bool handleDIV(Instruction *);
   bool handleSQRT(Instruction *);
   bool handlePOW(Instruction *);

   bool handleSET(Instruction *);
   bool handleSLCT(CmpInstruction *);
   bool handleSELP(Instruction *);

   bool handleTEX(TexInstruction *);
   bool handleTXB(TexInstruction *); // I really
   bool handleTXL(TexInstruction *); // hate
   bool handleTXD(TexInstruction *); // these 3

   bool handleCALL(Instruction *);

   void checkPredicate(Instruction *);

private:
   const Target *const targ;

   BuildUtil bld;

   Value *tid;
};

NV50LoweringPreSSA::NV50LoweringPreSSA(Program *prog) :
   targ(prog->getTarget()), tid(NULL)
{
   bld.setProgram(prog);
}

bool
NV50LoweringPreSSA::visit(Function *f)
{
   BasicBlock *root = BasicBlock::get(func->cfg.getRoot());

   if (prog->getType() == Program::TYPE_COMPUTE) {
      // Add implicit "thread id" argument in $r0 to the function
      Value *arg = new_LValue(func, FILE_GPR);
      arg->reg.data.id = 0;
      f->ins.push_back(arg);

      bld.setPosition(root, false);
      tid = bld.mkMov(bld.getScratch(), arg, TYPE_U32)->getDef(0);
   }

   return true;
}

// move array source to first slot, convert to u16, add indirections
bool
NV50LoweringPreSSA::handleTEX(TexInstruction *i)
{
   const int arg = i->tex.target.getArgCount();
   const int dref = arg;
   const int lod = i->tex.target.isShadow() ? (arg + 1) : arg;

   // dref comes before bias/lod
   if (i->tex.target.isShadow())
      if (i->op == OP_TXB || i->op == OP_TXL)
         i->swapSources(dref, lod);

   // array index must be converted to u32
   if (i->tex.target.isArray()) {
      Value *layer = i->getSrc(arg - 1);
      LValue *src = new_LValue(func, FILE_GPR);
      bld.mkCvt(OP_CVT, TYPE_U16, src, TYPE_F32, layer);
      i->setSrc(arg - 1, src);

      if (i->tex.target.isCube()) {
         // Value *face = layer;
         Value *x, *y;
         x = new_LValue(func, FILE_GPR);
         y = new_LValue(func, FILE_GPR);
         layer = new_LValue(func, FILE_GPR);

         i->tex.target = TEX_TARGET_2D_ARRAY;

         // TODO: use TEXPREP to convert x,y,z,face -> x,y,layer
         bld.mkMov(x, i->getSrc(0));
         bld.mkMov(y, i->getSrc(1));
         bld.mkMov(layer, i->getSrc(3));

         i->setSrc(0, x);
         i->setSrc(1, y);
         i->setSrc(2, layer);
         i->setSrc(3, i->getSrc(4));
         i->setSrc(4, NULL);
      }
   }

   // texel offsets are 3 immediate fields in the instruction,
   // nv50 cannot do textureGatherOffsets
   assert(i->tex.useOffsets <= 1);

   return true;
}

// Bias must be equal for all threads of a quad or lod calculation will fail.
//
// The lanes of a quad are grouped by the bit in the condition register they
// have set, which is selected by differing bias values.
// Move the input values for TEX into a new register set for each group and
// execute TEX only for a specific group.
// We always need to use 4 new registers for the inputs/outputs because the
// implicitly calculated derivatives must be correct.
bool
NV50LoweringPreSSA::handleTXB(TexInstruction *i)
{
   const CondCode cc[4] = { CC_EQU, CC_S, CC_C, CC_O };
   int l, d;

   handleTEX(i);
   Value *bias = i->getSrc(i->tex.target.getArgCount() - 1);
   if (bias->isUniform())
      return true;

   Instruction *cond = bld.mkOp(OP_UNION, TYPE_U32, bld.getScratch());
   bld.setPosition(cond, false);

   for (l = 0; l < 4; ++l) {
      const uint8_t qop = QUADOP(SUBR, SUBR, SUBR, SUBR);
      Value *bit = bld.getSSA();
      Value *pred = bld.getScratch(1, FILE_FLAGS);
      Value *imm = bld.loadImm(NULL, (1 << l));
      bld.mkQuadop(qop, pred, l, bias, bias)->flagsDef = 0;
      bld.mkMov(bit, imm)->setPredicate(CC_EQ, pred);
      cond->setSrc(l, bit);
   }
   Value *flags = bld.getScratch(1, FILE_FLAGS);
   bld.setPosition(cond, true);
   bld.mkMov(flags, cond->getDef(0));

   Instruction *tex[4];
   for (l = 0; l < 4; ++l)
      (tex[l] = cloneForward(func, i))->setPredicate(cc[l], flags);

   Value *res[4][4];
   for (d = 0; i->defExists(d); ++d)
      res[0][d] = tex[0]->getDef(d);
   for (l = 1; l < 4; ++l)
      bld.mkMov(res[l][d], tex[l]->getDef(d))->setPredicate(cc[l], flags);

   for (d = 0; i->defExists(d); ++d) {
      Instruction *dst = bld.mkOp(OP_UNION, TYPE_U32, i->getDef(d));
      for (l = 0; l < 4; ++l)
         dst->setSrc(l, res[l][d]);
   }
   delete_Instruction(prog, i);
   return true;
}

// LOD must be equal for all threads of a quad.
// Unlike with TXB, here we can just diverge since there's no LOD calculation
// that would require all 4 threads' sources to be set up properly.
bool
NV50LoweringPreSSA::handleTXL(TexInstruction *i)
{
   handleTEX(i);
   Value *lod = i->getSrc(i->tex.target.getArgCount() - 1);
   if (lod->isUniform())
      return true;

   BasicBlock *currBB = i->bb;
   BasicBlock *texiBB = i->bb->splitBefore(i, false);
   BasicBlock *joinBB = i->bb->splitAfter(i);

   for (int l = 0; l <= 3; ++l) {
      const uint8_t qop = QUADOP(SUBR, SUBR, SUBR, SUBR);
      Value *pred = bld.getScratch(1, FILE_FLAGS);
      bld.setPosition(currBB, true);
      bld.mkQuadop(qop, pred, l, lod, lod)->flagsDef = 0;
      bld.mkFlow(OP_BRA, texiBB, CC_EQ, pred)->fixed = 1;
      currBB->cfg.attach(&texiBB->cfg, Graph::Edge::FORWARD);
      if (l <= 2) {
         BasicBlock *laneBB = new BasicBlock(func);
         currBB->cfg.attach(&laneBB->cfg, Graph::Edge::TREE);
         laneBB = currBB;
      }
   }
   bld.setPosition(joinBB, false);
   bld.mkOp(OP_JOIN, TYPE_NONE, NULL);
   return true;
}

bool
NV50LoweringPreSSA::handleTXD(TexInstruction *i)
{
   static const uint8_t qOps[4][2] =
   {
      { QUADOP(MOV2, ADD,  MOV2, ADD),  QUADOP(MOV2, MOV2, ADD,  ADD) }, // l0
      { QUADOP(SUBR, MOV2, SUBR, MOV2), QUADOP(MOV2, MOV2, ADD,  ADD) }, // l1
      { QUADOP(MOV2, ADD,  MOV2, ADD),  QUADOP(SUBR, SUBR, MOV2, MOV2) }, // l2
      { QUADOP(SUBR, MOV2, SUBR, MOV2), QUADOP(SUBR, SUBR, MOV2, MOV2) }, // l3
   };
   Value *def[4][4];
   Value *crd[3];
   Instruction *tex;
   Value *zero = bld.loadImm(bld.getSSA(), 0);
   int l, c;
   const int dim = i->tex.target.getDim();

   handleTEX(i);
   i->op = OP_TEX; // no need to clone dPdx/dPdy later

   for (c = 0; c < dim; ++c)
      crd[c] = bld.getScratch();

   bld.mkOp(OP_QUADON, TYPE_NONE, NULL);
   for (l = 0; l < 4; ++l) {
      // mov coordinates from lane l to all lanes
      for (c = 0; c < dim; ++c)
         bld.mkQuadop(0x00, crd[c], l, i->getSrc(c), zero);
      // add dPdx from lane l to lanes dx
      for (c = 0; c < dim; ++c)
         bld.mkQuadop(qOps[l][0], crd[c], l, i->dPdx[c].get(), crd[c]);
      // add dPdy from lane l to lanes dy
      for (c = 0; c < dim; ++c)
         bld.mkQuadop(qOps[l][1], crd[c], l, i->dPdy[c].get(), crd[c]);
      // texture
      bld.insert(tex = cloneForward(func, i));
      for (c = 0; c < dim; ++c)
         tex->setSrc(c, crd[c]);
      // save results
      for (c = 0; i->defExists(c); ++c) {
         Instruction *mov;
         def[c][l] = bld.getSSA();
         mov = bld.mkMov(def[c][l], tex->getDef(c));
         mov->fixed = 1;
         mov->lanes = 1 << l;
      }
   }
   bld.mkOp(OP_QUADPOP, TYPE_NONE, NULL);

   for (c = 0; i->defExists(c); ++c) {
      Instruction *u = bld.mkOp(OP_UNION, TYPE_U32, i->getDef(c));
      for (l = 0; l < 4; ++l)
         u->setSrc(l, def[c][l]);
   }

   i->bb->remove(i);
   return true;
}

bool
NV50LoweringPreSSA::handleSET(Instruction *i)
{
   if (i->dType == TYPE_F32) {
      bld.setPosition(i, true);
      i->dType = TYPE_U32;
      bld.mkOp1(OP_NEG, TYPE_S32, i->getDef(0), i->getDef(0));
      bld.mkCvt(OP_CVT, TYPE_F32, i->getDef(0), TYPE_S32, i->getDef(0));
   }
   return true;
}

bool
NV50LoweringPreSSA::handleSLCT(CmpInstruction *i)
{
   Value *src1 = bld.getSSA();
   Value *pred = bld.getScratch(FILE_FLAGS);
   bld.mkCmp(OP_SET, i->setCond, i->sType,
             pred, i->getSrc(2), bld.loadImm(NULL, 0));
   bld.mkMov(src1, i->getSrc(1))->setPredicate(CC_EQ, pred);
   bld.mkOp2(OP_UNION, i->dType, i->getDef(0), i->getSrc(0), src1);
   return true;
}

bool
NV50LoweringPreSSA::handleSELP(Instruction *i)
{
   Value *src1 = bld.getSSA();
   bld.mkMov(src1, i->getSrc(1))->setPredicate(CC_EQ, i->getPredicate());
   bld.mkOp2(OP_UNION, i->dType, i->getDef(0), i->getSrc(0), src1);
   return true;
}

bool
NV50LoweringPreSSA::handleWRSV(Instruction *i)
{
   Symbol *sym = i->getSrc(0)->asSym();

   // these are all shader outputs, $sreg are not writeable
   uint32_t addr = targ->getSVAddress(FILE_SHADER_OUTPUT, sym);
   if (addr >= 0x400)
      return false;
   sym = bld.mkSymbol(FILE_SHADER_OUTPUT, 0, i->sType, addr);

   bld.mkStore(OP_EXPORT, i->dType, sym, i->getIndirect(0, 0), i->getSrc(1));

   bld.getBB()->remove(i);
   return true;
}

bool
NV50LoweringPreSSA::handleCALL(Instruction *i)
{
   if (prog->getType() == Program::TYPE_COMPUTE) {
      // Add implicit "thread id" argument in $r0 to the function
      i->setSrc(i->srcCount(), tid);
   }
   return true;
}

bool
NV50LoweringPreSSA::handleRDSV(Instruction *i)
{
   Symbol *sym = i->getSrc(0)->asSym();
   uint32_t addr = targ->getSVAddress(FILE_SHADER_INPUT, sym);
   Value *def = i->getDef(0);
   SVSemantic sv = sym->reg.data.sv.sv;
   int idx = sym->reg.data.sv.index;

   if (addr >= 0x400) // mov $sreg
      return true;

   switch (sv) {
   case SV_POSITION:
      assert(prog->getType() == Program::TYPE_FRAGMENT);
      bld.mkInterp(NV50_IR_INTERP_LINEAR, i->getDef(0), addr, NULL);
      break;
   case SV_FACE:
      bld.mkInterp(NV50_IR_INTERP_FLAT, def, addr, NULL);
      if (i->dType == TYPE_F32) {
         bld.mkOp2(OP_AND, TYPE_U32, def, def, bld.mkImm(0x80000000));
         bld.mkOp2(OP_XOR, TYPE_U32, def, def, bld.mkImm(0xbf800000));
      }
      break;
   case SV_NCTAID:
   case SV_CTAID:
   case SV_NTID:
      if ((sv == SV_NCTAID && idx >= 2) ||
          (sv == SV_NTID && idx >= 3)) {
         bld.mkMov(def, bld.mkImm(1));
      } else if (sv == SV_CTAID && idx >= 2) {
         bld.mkMov(def, bld.mkImm(0));
      } else {
         Value *x = bld.getSSA(2);
         bld.mkOp1(OP_LOAD, TYPE_U16, x,
                   bld.mkSymbol(FILE_MEMORY_SHARED, 0, TYPE_U16, addr));
         bld.mkCvt(OP_CVT, TYPE_U32, def, TYPE_U16, x);
      }
      break;
   case SV_TID:
      if (idx == 0) {
         bld.mkOp2(OP_AND, TYPE_U32, def, tid, bld.mkImm(0x0000ffff));
      } else if (idx == 1) {
         bld.mkOp2(OP_AND, TYPE_U32, def, tid, bld.mkImm(0x03ff0000));
         bld.mkOp2(OP_SHR, TYPE_U32, def, def, bld.mkImm(16));
      } else if (idx == 2) {
         bld.mkOp2(OP_SHR, TYPE_U32, def, tid, bld.mkImm(26));
      } else {
         bld.mkMov(def, bld.mkImm(0));
      }
      break;
   default:
      bld.mkFetch(i->getDef(0), i->dType,
                  FILE_SHADER_INPUT, addr, i->getIndirect(0, 0), NULL);
      break;
   }
   bld.getBB()->remove(i);
   return true;
}

bool
NV50LoweringPreSSA::handleMUL(Instruction *i)
{
   if (!isFloatType(i->dType) && typeSizeof(i->sType) > 2)
      return expandIntegerMUL(&bld, i);
   return true;
}

bool
NV50LoweringPreSSA::handleDIV(Instruction *i)
{
   if (!isFloatType(i->dType))
      return true;
   bld.setPosition(i, false);
   Instruction *rcp = bld.mkOp1(OP_RCP, i->dType, bld.getSSA(), i->getSrc(1));
   i->op = OP_MUL;
   i->setSrc(1, rcp->getDef(0));
   return true;
}

bool
NV50LoweringPreSSA::handleSQRT(Instruction *i)
{
   Instruction *rsq = bld.mkOp1(OP_RSQ, TYPE_F32,
                                bld.getSSA(), i->getSrc(0));
   i->op = OP_MUL;
   i->setSrc(1, rsq->getDef(0));

   return true;
}

bool
NV50LoweringPreSSA::handlePOW(Instruction *i)
{
   LValue *val = bld.getScratch();

   bld.mkOp1(OP_LG2, TYPE_F32, val, i->getSrc(0));
   bld.mkOp2(OP_MUL, TYPE_F32, val, i->getSrc(1), val)->dnz = 1;
   bld.mkOp1(OP_PREEX2, TYPE_F32, val, val);

   i->op = OP_EX2;
   i->setSrc(0, val);
   i->setSrc(1, NULL);

   return true;
}

bool
NV50LoweringPreSSA::handleEXPORT(Instruction *i)
{
   if (prog->getType() == Program::TYPE_FRAGMENT) {
      if (i->getIndirect(0, 0)) {
         // TODO: redirect to l[] here, load to GPRs at exit
         return false;
      } else {
         int id = i->getSrc(0)->reg.data.offset / 4; // in 32 bit reg units

         i->op = OP_MOV;
         i->src(0).set(i->src(1));
         i->setSrc(1, NULL);
         i->setDef(0, new_LValue(func, FILE_GPR));
         i->getDef(0)->reg.data.id = id;

         prog->maxGPR = MAX2(prog->maxGPR, id);
      }
   }
   return true;
}

// Set flags according to predicate and make the instruction read $cX.
void
NV50LoweringPreSSA::checkPredicate(Instruction *insn)
{
   Value *pred = insn->getPredicate();
   Value *cdst;

   if (!pred || pred->reg.file == FILE_FLAGS)
      return;
   cdst = new_LValue(func, FILE_FLAGS);

   bld.mkCmp(OP_SET, CC_NEU, TYPE_U32, cdst, bld.loadImm(NULL, 0), pred);

   insn->setPredicate(insn->cc, cdst);
}

//
// - add quadop dance for texturing
// - put FP outputs in GPRs
// - convert instruction sequences
//
bool
NV50LoweringPreSSA::visit(Instruction *i)
{
   if (i->prev)
      bld.setPosition(i->prev, true);
   else
   if (i->next)
      bld.setPosition(i->next, false);
   else
      bld.setPosition(i->bb, true);

   if (i->cc != CC_ALWAYS)
      checkPredicate(i);

   switch (i->op) {
   case OP_TEX:
   case OP_TXF:
   case OP_TXG:
      return handleTEX(i->asTex());
   case OP_TXB:
      return handleTXB(i->asTex());
   case OP_TXL:
      return handleTXL(i->asTex());
   case OP_TXD:
      return handleTXD(i->asTex());
   case OP_EX2:
      bld.mkOp1(OP_PREEX2, TYPE_F32, i->getDef(0), i->getSrc(0));
      i->setSrc(0, i->getDef(0));
      break;
   case OP_SET:
      return handleSET(i);
   case OP_SLCT:
      return handleSLCT(i->asCmp());
   case OP_SELP:
      return handleSELP(i);
   case OP_POW:
      return handlePOW(i);
   case OP_MUL:
      return handleMUL(i);
   case OP_DIV:
      return handleDIV(i);
   case OP_SQRT:
      return handleSQRT(i);
   case OP_EXPORT:
      return handleEXPORT(i);
   case OP_RDSV:
      return handleRDSV(i);
   case OP_WRSV:
      return handleWRSV(i);
   case OP_CALL:
      return handleCALL(i);
   default:
      break;
   }   
   return true;
}

bool
TargetNV50::runLegalizePass(Program *prog, CGStage stage) const
{
   if (stage == CG_STAGE_PRE_SSA) {
      NV50LoweringPreSSA pass(prog);
      return pass.run(prog, false, true);
   } else
   if (stage == CG_STAGE_SSA) {
      NV50LegalizeSSA pass;
      return pass.run(prog, false, true);
   } else
   if (stage == CG_STAGE_POST_RA) {
      NV50LegalizePostRA pass;
      return pass.run(prog, false, true);
   }
   return false;
}

} // namespace nv50_ir
