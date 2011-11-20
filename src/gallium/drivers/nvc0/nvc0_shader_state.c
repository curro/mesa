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

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"

#include "nvc0_context.h"

static INLINE void
nvc0_program_update_context_state(struct nvc0_context *nvc0,
                                  struct nvc0_program *prog, int stage)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;

   if (prog->hdr[1])
      nvc0->state.tls_required |= 1 << stage;
   else
      nvc0->state.tls_required &= ~(1 << stage);

   if (prog->immd_size) {
      const unsigned rl = NOUVEAU_BO_VRAM | NOUVEAU_BO_RD;

      BEGIN_RING(chan, RING_3D(CB_SIZE), 3);
      /* NOTE: may overlap code of a different shader */
      OUT_RING  (chan, align(prog->immd_size, 0x100));
      OUT_RELOCh(chan, nvc0->screen->text, prog->immd_base, rl);
      OUT_RELOCl(chan, nvc0->screen->text, prog->immd_base, rl);
      BEGIN_RING(chan, RING_3D(CB_BIND(stage)), 1);
      OUT_RING  (chan, (14 << 4) | 1);

      nvc0->state.c14_bound |= 1 << stage;
   } else
   if (nvc0->state.c14_bound & (1 << stage)) {
      BEGIN_RING(chan, RING_3D(CB_BIND(stage)), 1);
      OUT_RING  (chan, (14 << 4) | 0);

      nvc0->state.c14_bound &= ~(1 << stage);
   }
}

static void
nvc0_program_validate_clip(struct nvc0_context *nvc0, struct nvc0_program *vp)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;

   if (nvc0->vertprog->vp.num_ucps)
      return;

   if (nvc0->state.clip_enable != vp->vp.clip_enable) {
      nvc0->state.clip_enable = vp->vp.clip_enable;
      IMMED_RING(chan, RING_3D(CLIP_DISTANCE_ENABLE), vp->vp.clip_enable);
   }
   if (nvc0->state.clip_mode != vp->vp.clip_mode) {
      nvc0->state.clip_mode = vp->vp.clip_mode;
      BEGIN_RING(chan, RING_3D(CLIP_DISTANCE_MODE), 1);
      OUT_RING  (chan, vp->vp.clip_mode);
   }
}

static INLINE boolean
nvc0_program_validate(struct nvc0_context *nvc0, struct nvc0_program *prog)
{
   if (prog->res)
      return TRUE;

   if (!prog->translated) {
      unsigned i;
      for (i = 0; i < 100; ++i) {
         nvc0_program_destroy(nvc0, prog);
         nvc0_program_translate(prog);
      }
      nvc0_program_destroy(nvc0, prog);
      
      prog->translated = nvc0_program_translate(prog);
      if (!prog->translated)
         return FALSE;
   }

   return nvc0_program_upload_code(nvc0, prog);
}

void
nvc0_vertprog_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *vp = nvc0->vertprog;

   if (nvc0->clip.nr > vp->vp.num_ucps) {
      assert(nvc0->clip.nr <= 6);
      vp->vp.num_ucps = 6;

      if (vp->translated)
         nvc0_program_destroy(nvc0, vp);
   }

   if (!nvc0_program_validate(nvc0, vp))
         return;
   nvc0_program_update_context_state(nvc0, vp, 0);

   BEGIN_RING(chan, RING_3D(SP_SELECT(1)), 2);
   OUT_RING  (chan, 0x11);
   OUT_RING  (chan, vp->code_base);
   BEGIN_RING(chan, RING_3D(SP_GPR_ALLOC(1)), 1);
   OUT_RING  (chan, vp->max_gpr);

   if (!nvc0->gmtyprog && !nvc0->tevlprog)
      nvc0_program_validate_clip(nvc0, vp);

   // BEGIN_RING(chan, RING_3D_(0x163c), 1);
   // OUT_RING  (chan, 0);
}

void
nvc0_fragprog_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *fp = nvc0->fragprog;

   if (!nvc0_program_validate(nvc0, fp))
         return;
   nvc0_program_update_context_state(nvc0, fp, 4);

   BEGIN_RING(chan, RING_3D(SP_SELECT(5)), 2);
   OUT_RING  (chan, 0x51);
   OUT_RING  (chan, fp->code_base);
   BEGIN_RING(chan, RING_3D(SP_GPR_ALLOC(5)), 1);
   OUT_RING  (chan, fp->max_gpr);

   BEGIN_RING(chan, RING_3D_(0x0360), 2);
   OUT_RING  (chan, 0x20164010);
   OUT_RING  (chan, 0x20);
   BEGIN_RING(chan, RING_3D_(0x196c), 1);
   OUT_RING  (chan, fp->flags[0]);
}

void
nvc0_tctlprog_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *tp = nvc0->tctlprog;

   if (!tp) {
      BEGIN_RING(chan, RING_3D(SP_SELECT(2)), 1);
      OUT_RING  (chan, 0x20);
      return;
   }
   if (!nvc0_program_validate(nvc0, tp))
         return;
   nvc0_program_update_context_state(nvc0, tp, 1);

   if (tp->tp.tess_mode != ~0) {
      BEGIN_RING(chan, RING_3D(TESS_MODE), 1);
      OUT_RING  (chan, tp->tp.tess_mode);
   }
   BEGIN_RING(chan, RING_3D(SP_SELECT(2)), 2);
   OUT_RING  (chan, 0x21);
   OUT_RING  (chan, tp->code_base);
   BEGIN_RING(chan, RING_3D(SP_GPR_ALLOC(2)), 1);
   OUT_RING  (chan, tp->max_gpr);

   if (tp->tp.input_patch_size <= 32)
      IMMED_RING(chan, RING_3D(PATCH_VERTICES), tp->tp.input_patch_size);
}

void
nvc0_tevlprog_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *tp = nvc0->tevlprog;

   if (!tp) {
      BEGIN_RING(chan, RING_3D(TEP_SELECT), 1);
      OUT_RING  (chan, 0x30);
      return;
   }
   if (!nvc0_program_validate(nvc0, tp))
         return;
   nvc0_program_update_context_state(nvc0, tp, 2);

   if (tp->tp.tess_mode != ~0) {
      BEGIN_RING(chan, RING_3D(TESS_MODE), 1);
      OUT_RING  (chan, tp->tp.tess_mode);
   }
   BEGIN_RING(chan, RING_3D(TEP_SELECT), 1);
   OUT_RING  (chan, 0x31);
   BEGIN_RING(chan, RING_3D(SP_START_ID(3)), 1);
   OUT_RING  (chan, tp->code_base);
   BEGIN_RING(chan, RING_3D(SP_GPR_ALLOC(3)), 1);
   OUT_RING  (chan, tp->max_gpr);

   if (!nvc0->gmtyprog)
      nvc0_program_validate_clip(nvc0, tp);
}

void
nvc0_gmtyprog_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *gp = nvc0->gmtyprog;

   if (!gp) {
      BEGIN_RING(chan, RING_3D(GP_SELECT), 1);
      OUT_RING  (chan, 0x40);
      IMMED_RING(chan, RING_3D(LAYER), 0);
      return;
   }
   if (!nvc0_program_validate(nvc0, gp))
         return;
   nvc0_program_update_context_state(nvc0, gp, 3);

   BEGIN_RING(chan, RING_3D(GP_SELECT), 1);
   OUT_RING  (chan, 0x41);
   BEGIN_RING(chan, RING_3D(SP_START_ID(4)), 1);
   OUT_RING  (chan, gp->code_base);
   BEGIN_RING(chan, RING_3D(SP_GPR_ALLOC(4)), 1);
   OUT_RING  (chan, gp->max_gpr);
   BEGIN_RING(chan, RING_3D(LAYER), 1);
   OUT_RING  (chan, (gp->hdr[13] & (1 << 9)) ? NVC0_3D_LAYER_USE_GP : 0);

   nvc0_program_validate_clip(nvc0, gp);
}

/* It's *is* kind of shader related. We need to inspect the program
 * to get the output locations right.
 */
void
nvc0_tfb_validate(struct nvc0_context *nvc0)
{
   struct nouveau_channel *chan = nvc0->screen->base.channel;
   struct nvc0_program *vp;
   struct nvc0_transform_feedback_state *tfb = nvc0->tfb;
   int b;

   BEGIN_RING(chan, RING_3D(TFB_ENABLE), 1);
   if (!tfb) {
      OUT_RING(chan, 0);
      return;
   }
   OUT_RING(chan, 1);

   vp = nvc0->vertprog ? nvc0->vertprog : nvc0->gmtyprog;

   for (b = 0; b < nvc0->num_tfbbufs; ++b) {
      uint8_t idx, var[128];
      int i, n;
      struct nv04_resource *buf = nv04_resource(nvc0->tfbbuf[b]);

      BEGIN_RING(chan, RING_3D(TFB_BUFFER_ENABLE(b)), 5);
      OUT_RING  (chan, 1);
      OUT_RESRCh(chan, buf, nvc0->tfb_offset[b], NOUVEAU_BO_WR);
      OUT_RESRCl(chan, buf, nvc0->tfb_offset[b], NOUVEAU_BO_WR);
      OUT_RING  (chan, buf->base.width0 - nvc0->tfb_offset[b]);
      OUT_RING  (chan, 0); /* TFB_PRIMITIVE_ID <- offset ? */

      if (!(nvc0->dirty & NVC0_NEW_TFB))
         continue;

      BEGIN_RING(chan, RING_3D(TFB_UNK07X0(b)), 3);
      OUT_RING  (chan, 0);
      OUT_RING  (chan, tfb->varying_count[b]);
      OUT_RING  (chan, tfb->stride[b]);

      n = b ? tfb->varying_count[b - 1] : 0;
      i = 0;
      for (; i < tfb->varying_count[b]; ++i) {
         idx = tfb->varying_index[n + i];
         var[i] = vp->vp.out_pos[idx >> 2] + (idx & 3);
      }
      for (; i & 3; ++i)
         var[i] = 0;

      BEGIN_RING(chan, RING_3D(TFB_VARYING_LOCS(b, 0)), i / 4);
      OUT_RINGp (chan, var, i / 4);
   }
   for (; b < 4; ++b)
      IMMED_RING(chan, RING_3D(TFB_BUFFER_ENABLE(b)), 0);
}
