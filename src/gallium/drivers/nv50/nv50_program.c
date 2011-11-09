/*
 * Copyright 2010 Christoph Bumiller
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

#include "nv50_program.h"
#include "nv50_context.h"

#include "codegen/nv50_ir_driver.h"

static INLINE unsigned
bitcount4(const uint32_t val)
{
   static const uint8_t cnt[16]
   = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
   return cnt[val & 0xf];
}

static int
nv50_vertprog_assign_slots(struct nv50_ir_prog_info *info)
{
   struct nv50_program *prog = (struct nv50_program *)info->driverPriv;
   unsigned i, n, c;

   n = 0;
   for (i = 0; i < info->numInputs; ++i) {
      switch (info->in[i].sn) {
      case TGSI_SEMANTIC_INSTANCEID:
         prog->vp.attrs[2] |= NV50_3D_VP_GP_BUILTIN_ATTR_EN_VERTEX_ID;
         continue;
      case NV50_SEMANTIC_VERTEXID:
         prog->vp.attrs[2] |= NV50_3D_VP_GP_BUILTIN_ATTR_EN_INSTANCE_ID;
         continue;
      default:
         break;
      }
      prog->in[i].id = i;
      prog->in[i].sn = info->in[i].sn;
      prog->in[i].si = info->in[i].si;
      prog->in[i].hw = n;
      prog->in[i].mask = info->in[i].mask;

      prog->vp.attrs[(4 * i) / 32] |= info->in[i].mask << ((4 * i) % 32);

      for (c = 0; c < 4; ++c)
         if (info->in[i].mask & (1 << c))
            info->in[i].slot[c] = n++;
   }
   prog->in_nr = info->numInputs;

   for (i = 0; i < info->numSysVals; ++i) {
      switch (info->sv[i].sn) {
      case TGSI_SEMANTIC_INSTANCEID:
         prog->vp.attrs[2] |= NV50_3D_VP_GP_BUILTIN_ATTR_EN_VERTEX_ID;
         continue;
      case NV50_SEMANTIC_VERTEXID:
         prog->vp.attrs[2] |= NV50_3D_VP_GP_BUILTIN_ATTR_EN_INSTANCE_ID;
         continue;
      default:
         break;
      }
   }
   /* TODO: communicate slots for InstanceID and VertexID */

   n = 0;
   for (i = 0; i < info->numOutputs; ++i) {
      switch (info->out[i].sn) {
      case TGSI_SEMANTIC_PSIZE:
         prog->vp.psiz = i;
         break;
      case NV50_SEMANTIC_CLIPDISTANCE:
         prog->vp.clpd = i;
         break;
      case TGSI_SEMANTIC_EDGEFLAG:
         prog->vp.edgeflag = i;
         break;
      case TGSI_SEMANTIC_BCOLOR:
         prog->vp.bfc[info->in[i].si] = i;
         break;
      default:
         break;
      }
      prog->out[i].id = i;
      prog->out[i].sn = info->out[i].sn;
      prog->out[i].si = info->out[i].si;
      prog->out[i].hw = n;
      prog->out[i].mask = info->out[i].mask;

      for (c = 0; c < 4; ++c)
         if (info->out[i].mask & (1 << c))
            info->out[i].slot[c] = n++;
   }
   prog->out_nr = info->numOutputs;
   prog->max_out = n;

   if (prog->vp.psiz < info->numOutputs)
      prog->vp.psiz = prog->out[prog->vp.psiz].hw;

   return 0;
}

static int
nv50_fragprog_assign_slots(struct nv50_ir_prog_info *info)
{
   struct nv50_program *prog = (struct nv50_program *)info->driverPriv;
   unsigned i, n, m, c;
   unsigned nvary;
   unsigned nintp = 0;

   /* count non-flat inputs */
   for (m = 0, i = 0; i < info->numInputs; ++i)
      m += info->in[i].flat ? 0 : 1;

   /* Fill prog->in[] so that non-flat inputs are first and
    * kick out special inputs that don't use the RESULT_MAP.
    */
   for (n = 0, i = 0; i < info->numInputs; ++i) {
      if (info->in[i].sn == TGSI_SEMANTIC_POSITION) {
         prog->fp.interp |= info->in[i].mask << 24;
         for (c = 0; c < 4; ++c)
            if (info->in[i].mask & (1 << c))
               info->in[i].slot[c] = nintp++;
      } else
      if (info->in[i].sn == TGSI_SEMANTIC_FACE) {
         info->in[i].slot[0] = 255;
      } else {
         unsigned j = info->in[i].flat ? m++ : n++;

         if (info->in[i].sn == TGSI_SEMANTIC_COLOR)
            prog->vp.bfc[info->in[i].si] = j;

         prog->in[j].id = i;
         prog->in[j].mask = info->in[i].mask;
         prog->in[j].sn = info->in[i].sn;
         prog->in[j].si = info->in[i].si;
         prog->in[j].linear = info->in[i].linear;

         prog->in_nr++;
      }
   }
   if (!(prog->fp.interp & (8 << 24))) {
      ++nintp;
      prog->fp.interp |= 8 << 24;
   }

   for (i = 0; i < prog->in_nr; ++i) {
      int j = prog->in[i].id;

      prog->in[j].hw = nintp;
      for (c = 0; c < 4; ++c)
         if (info->in[i].mask & (1 << c))
            info->in[j].slot[c] = nintp++;
   }
   nintp -= bitcount4(prog->fp.interp >> 24); /* subtract position inputs */
   nvary = nintp;
   if (n < m)
      nvary -= prog->in[n].hw;

   prog->fp.interp |= nvary << NV50_3D_FP_INTERPOLANT_CTRL_COUNT_NONFLAT__SHIFT;
   prog->fp.interp |= nintp << NV50_3D_FP_INTERPOLANT_CTRL_COUNT__SHIFT;

   /* put front/back colors right after HPOS */
   prog->fp.colors = 4 << NV50_3D_MAP_SEMANTIC_0_FFC0_ID__SHIFT;
   for (i = 0; i < 2; ++i)
      if (prog->vp.bfc[i] < 0x80)
         prog->fp.colors += bitcount4(prog->in[prog->vp.bfc[i]].mask) << 16;

   /* FP outputs */

   if (info->prop.fp.numColourResults > 1)
      prog->fp.flags[0] |= NV50_3D_FP_CONTROL_MULTIPLE_RESULTS;

   for (i = 0; i < info->numOutputs; ++i) {
      prog->out[i].id = i;
      prog->out[i].sn = info->out[i].sn;
      prog->out[i].si = info->out[i].si;
      prog->out[i].mask = info->out[i].mask;

      if (i == info->io.fragDepth || i == info->io.sampleMask)
         continue;
      prog->out[i].hw = info->out[i].si * 4;

      for (c = 0; c < 4; ++c)
         info->out[i].slot[c] = prog->out[i].hw + c;

      prog->max_out = MAX2(prog->max_out, prog->out[i].hw + 4);
   }

   if (info->io.sampleMask < PIPE_MAX_SHADER_OUTPUTS)
      info->out[info->io.sampleMask].slot[0] = prog->max_out++;

   if (info->io.fragDepth < PIPE_MAX_SHADER_OUTPUTS)
      info->out[info->io.fragDepth].slot[2] = prog->max_out++;

   if (!prog->max_out)
      prog->max_out = 4;

   return 0;
}

static int
nv50_program_assign_varying_slots(struct nv50_ir_prog_info *info)
{
   switch (info->type) {
   case PIPE_SHADER_VERTEX:
      return nv50_vertprog_assign_slots(info);
   case PIPE_SHADER_GEOMETRY:
      return nv50_vertprog_assign_slots(info);
   case PIPE_SHADER_FRAGMENT:
      return nv50_fragprog_assign_slots(info);
   default:
      return -1;
   }
}

boolean
nv50_program_translate(struct nv50_program *prog, uint16_t chipset)
{
   struct nv50_ir_prog_info *info;
   int ret;

   info = CALLOC_STRUCT(nv50_ir_prog_info);
   if (!info)
      return FALSE;

   info->type = prog->type;
   info->target = chipset;
   info->bin.sourceRep = NV50_PROGRAM_IR_TGSI;
   info->bin.source = (void *)prog->pipe.tokens;

   info->io.clipDistanceCount = prog->vp.clpd_nr;

   info->assignSlots = nv50_program_assign_varying_slots;

   prog->vp.bfc[0] = 0x80;
   prog->vp.bfc[1] = 0x80;
   prog->vp.psiz = 0x80;
   prog->vp.edgeflag = 0x80;
   prog->gp.primid = 0x80;

   info->driverPriv = prog;

#ifdef DEBUG
   info->optLevel = debug_get_num_option("NV50_PROG_OPTIMIZE", 3);
   info->dbgFlags = debug_get_num_option("NV50_PROG_DEBUG", 0);
#else
   info->optLevel = 3;
#endif

   ret = nv50_ir_generate_code(info);
   if (ret) {
      NOUVEAU_ERR("shader translation failed: %i\n", ret);
      goto out;
   }
   prog->code = info->bin.code;
   prog->code_size = info->bin.codeSize;
   prog->fixups = info->bin.relocData;
   prog->max_gpr = MAX2(4, info->bin.maxGPR + 1);

   if (prog->type == PIPE_SHADER_FRAGMENT) {
      if (info->prop.fp.writesDepth) {
         prog->fp.flags[0] |= NV50_3D_FP_CONTROL_EXPORTS_Z;
         prog->fp.flags[1] = 0x11;
      }
      if (info->prop.fp.usesDiscard)
         prog->fp.flags[0] |= NV50_3D_FP_CONTROL_USES_KIL;
   }

out:
   FREE(info);
   return !ret;
}

boolean
nv50_program_upload_code(struct nv50_context *nv50, struct nv50_program *prog)
{
   struct nouveau_resource *heap;
   int ret;
   uint32_t size = align(prog->code_size, 0x40);

   switch (prog->type) {
   case PIPE_SHADER_VERTEX:   heap = nv50->screen->vp_code_heap; break;
   case PIPE_SHADER_GEOMETRY: heap = nv50->screen->fp_code_heap; break;
   case PIPE_SHADER_FRAGMENT: heap = nv50->screen->gp_code_heap; break;
   default:
      assert(!"invalid program type");
      return FALSE;
   }

   ret = nouveau_resource_alloc(heap, size, prog, &prog->res);
   if (ret) {
      /* Out of space: evict everything to compactify the code segment, hoping
       * the working set is much smaller and drifts slowly. Improve me !
       */
      while (heap->next) {
         struct nv50_program *evict = heap->next->priv;
         if (evict)
            nouveau_resource_free(&evict->res);
      }
      debug_printf("WARNING: out of code space, evicting all shaders.\n");
   }
   prog->code_base = prog->res->start;

   if (prog->fixups)
      nv50_ir_relocate_code(prog->fixups, prog->code, prog->code_base, 0, 0);

   nv50_sifc_linear_u8(&nv50->base, nv50->screen->code,
                       (prog->type << NV50_CODE_BO_SIZE_LOG2) + prog->code_base,
                       NOUVEAU_BO_VRAM, prog->code_size, prog->code);

   BEGIN_RING(nv50->screen->base.channel, RING_3D(CODE_CB_FLUSH), 1);
   OUT_RING  (nv50->screen->base.channel, 0);

   return TRUE;
}

void
nv50_program_destroy(struct nv50_context *nv50, struct nv50_program *p)
{
   if (p->res)
      nouveau_resource_free(&p->res);

   if (p->code)
      FREE(p->code);

   if (p->fixups)
      FREE(p->fixups);

   p->translated = FALSE;
}
