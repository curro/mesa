#ifndef __NV50_CONTEXT_H__
#define __NV50_CONTEXT_H__

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"

#include "util/u_memory.h"
#include "util/u_math.h"
#include "util/u_inlines.h"
#include "util/u_dynarray.h"

#ifdef NV50_WITH_DRAW_MODULE
#include "draw/draw_vertex.h"
#endif

#include "nv50_debug.h"
#include "nv50_winsys.h"
#include "nv50_stateobj.h"
#include "nv50_screen.h"
#include "nv50_program.h"
#include "nv50_resource.h"
#include "nv50_transfer.h"

#include "nouveau/nouveau_context.h"
#include "nouveau/nv_object.xml.h"
#include "nouveau/nv_m2mf.xml.h"
#include "nv50_3ddefs.xml.h"
#include "nv50_3d.xml.h"
#include "nv50_2d.xml.h"
#include "nv50_compute.xml.h"

#define NV50_NEW_BLEND        (1 << 0)
#define NV50_NEW_RASTERIZER   (1 << 1)
#define NV50_NEW_ZSA          (1 << 2)
#define NV50_NEW_VERTPROG     (1 << 3)
#define NV50_NEW_GMTYPROG     (1 << 6)
#define NV50_NEW_FRAGPROG     (1 << 7)
#define NV50_NEW_COMPPROG     (1 << 8)
#define NV50_NEW_BLEND_COLOUR (1 << 9)
#define NV50_NEW_STENCIL_REF  (1 << 10)
#define NV50_NEW_CLIP         (1 << 11)
#define NV50_NEW_SAMPLE_MASK  (1 << 12)
#define NV50_NEW_FRAMEBUFFER  (1 << 13)
#define NV50_NEW_STIPPLE      (1 << 14)
#define NV50_NEW_SCISSOR      (1 << 15)
#define NV50_NEW_VIEWPORT     (1 << 16)
#define NV50_NEW_ARRAYS       (1 << 17)
#define NV50_NEW_VERTEX       (1 << 18)
#define NV50_NEW_CONSTBUF     (1 << 19)
#define NV50_NEW_TEXTURES     (1 << 20)
#define NV50_NEW_SAMPLERS     (1 << 21)
#define NV50_NEW_STRMOUT      (1 << 22)
#define NV50_NEW_RESOURCES    (1 << 23)
#define NV50_NEW_CONTEXT      (1 << 31)

#define NV50_BIND_FB          0
#define NV50_BIND_VERTEX      1
#define NV50_BIND_VERTEX_TMP  2
#define NV50_BIND_INDEX       3
#define NV50_BIND_TEXTURES    4
#define NV50_BIND_CB(s, i)   (5 + 16 * (s) + (i))
#define NV50_BIND_SO         53
#define NV50_BIND_SCREEN     54
#define NV50_BIND_TLS        55
#define NV50_BIND_COMPUTE    56
#define NV50_BIND_GLOBALS    57
#define NV50_BIND_RESOURCES  58
#define NV50_BIND_COUNT      59
#define NV50_BIND_2D          0
#define NV50_BIND_M2MF        0
#define NV50_BIND_FENCE       1

#define NV50_CB_TMP 123
/* fixed constant buffer binding points - low indices for user's constbufs */
#define NV50_CB_PVP 124
#define NV50_CB_PGP 126
#define NV50_CB_PFP 125
#define NV50_CB_AUX 127


struct nv50_context {
   struct nouveau_context base;

   struct nv50_screen *screen;

   struct nouveau_bufctx *bufctx_3d;
   struct nouveau_bufctx *bufctx;

   uint32_t dirty;

   struct {
      uint32_t instance_elts; /* bitmask of per-instance elements */
      uint32_t instance_base;
      uint32_t interpolant_ctrl;
      uint32_t semantic_color;
      uint32_t semantic_psize;
      int32_t index_bias;
      boolean prim_restart;
      boolean point_sprite;
      boolean rt_serialize;
      boolean flushed;
      boolean rasterizer_discard;
      uint8_t tls_required;
      uint8_t num_vtxbufs;
      uint8_t num_vtxelts;
      uint8_t num_textures[4];
      uint8_t num_samplers[4];
      uint8_t prim_size;
      uint16_t scissor;
   } state;

   struct nv50_blend_stateobj *blend;
   struct nv50_rasterizer_stateobj *rast;
   struct nv50_zsa_stateobj *zsa;
   struct nv50_vertex_stateobj *vertex;

   struct nv50_program *vertprog;
   struct nv50_program *gmtyprog;
   struct nv50_program *fragprog;
   struct nv50_program *compprog;

   struct pipe_resource *constbuf[4][16];
   uint16_t constbuf_dirty[4];
   uint16_t constbuf_valid[4];

   struct pipe_vertex_buffer vtxbuf[PIPE_MAX_ATTRIBS];
   unsigned num_vtxbufs;
   struct pipe_index_buffer idxbuf;
   uint32_t vbo_fifo; /* bitmask of vertex elements to be pushed to FIFO */
   uint32_t vbo_user; /* bitmask of vertex buffers pointing to user memory */
   uint32_t vb_elt_first; /* from pipe_draw_info, for vertex upload */
   uint32_t vb_elt_limit; /* max - min element (count - 1) */

   struct pipe_sampler_view *textures[4][PIPE_MAX_SAMPLERS];
   unsigned num_textures[4];
   struct nv50_tsc_entry *samplers[4][PIPE_MAX_SAMPLERS];
   unsigned num_samplers[4];

   struct pipe_surface *resources[PIPE_MAX_SHADER_RESOURCES];
   unsigned num_resources;

   struct {
      struct pipe_resource *r;
      uint32_t *handle;
   } globals[PIPE_MAX_SHADER_RESOURCES];

   uint8_t num_so_targets;
   uint8_t so_targets_dirty;
   struct pipe_stream_output_target *so_target[4];

   struct pipe_framebuffer_state framebuffer;
   struct pipe_blend_color blend_colour;
   struct pipe_stencil_ref stencil_ref;
   struct pipe_poly_stipple stipple;
   struct pipe_scissor_state scissor;
   struct pipe_viewport_state viewport;
   struct pipe_clip_state clip;

   unsigned sample_mask;

   boolean vbo_push_hint;

#ifdef NV50_WITH_DRAW_MODULE
   struct draw_context *draw;
#endif
};

static INLINE struct nv50_context *
nv50_context(struct pipe_context *pipe)
{
   return (struct nv50_context *)pipe;
}

static INLINE struct nv50_screen *
nv50_context_screen(struct nv50_context *nv50)
{
   return nv50_screen(&nv50->base.screen->base);
}


/* nv50_context.c */
struct pipe_context *nv50_create(struct pipe_screen *, void *);

void nv50_bufctx_fence(struct nouveau_bufctx *, boolean on_flush);

void nv50_default_kick_notify(struct nouveau_pushbuf *);

/* nv50_draw.c */
extern struct draw_stage *nv50_draw_render_stage(struct nv50_context *);

/* nv50_query.c */
void nv50_init_query_functions(struct nv50_context *);
void nv50_query_pushbuf_submit(struct nouveau_pushbuf *,
                               struct pipe_query *, unsigned result_offset);
void nv84_query_fifo_wait(struct nouveau_pushbuf *, struct pipe_query *);
void nva0_so_target_save_offset(struct pipe_context *,
                                struct pipe_stream_output_target *,
                                unsigned index, boolean seralize);

#define NVA0_QUERY_STREAM_OUTPUT_BUFFER_OFFSET (PIPE_QUERY_TYPES + 0)

/* nv50_shader_state.c */
void nv50_vertprog_validate(struct nv50_context *);
void nv50_gmtyprog_validate(struct nv50_context *);
void nv50_fragprog_validate(struct nv50_context *);
void nv50_compprog_validate(struct nv50_context *);
void nv50_fp_linkage_validate(struct nv50_context *);
void nv50_gp_linkage_validate(struct nv50_context *);
void nv50_constbufs_validate(struct nv50_context *);
void nv50_validate_derived_rs(struct nv50_context *);
void nv50_stream_output_validate(struct nv50_context *);

/* nv50_state.c */
extern void nv50_init_state_functions(struct nv50_context *);

/* nv50_state_validate.c */
/* @words: check for space before emitting relocs */
extern boolean nv50_state_validate(struct nv50_context *, uint32_t state_mask,
                                   unsigned space_words);

/* nv50_surface.c */
extern void nv50_clear(struct pipe_context *, unsigned buffers,
                       const union pipe_color_union *color,
                       double depth, unsigned stencil);
extern void nv50_init_surface_functions(struct nv50_context *);

/* nv50_tex.c */
void nv50_validate_textures(struct nv50_context *);
void nv50_validate_samplers(struct nv50_context *);

struct pipe_sampler_view *
nv50_create_sampler_view(struct pipe_context *,
                         struct pipe_resource *,
                         const struct pipe_sampler_view *);

/* nv50_transfer.c */
void
nv50_m2mf_transfer_rect(struct nv50_context *,
                        const struct nv50_m2mf_rect *dst,
                        const struct nv50_m2mf_rect *src,
                        uint32_t nblocksx, uint32_t nblocksy);
void
nv50_sifc_linear_u8(struct nouveau_context *pipe,
                    struct nouveau_bo *dst, unsigned offset, unsigned domain,
                    unsigned size, const void *data);
void
nv50_m2mf_copy_linear(struct nouveau_context *pipe,
                      struct nouveau_bo *dst, unsigned dstoff, unsigned dstdom,
                      struct nouveau_bo *src, unsigned srcoff, unsigned srcdom,
                      unsigned size);
void
nv50_cb_push(struct nouveau_context *nv,
             struct nouveau_bo *bo, unsigned domain,
             unsigned base, unsigned size,
             unsigned offset, unsigned words, const uint32_t *data);

/* nv50_vbo.c */
void nv50_draw_vbo(struct pipe_context *, const struct pipe_draw_info *);

void *
nv50_vertex_state_create(struct pipe_context *pipe,
                         unsigned num_elements,
                         const struct pipe_vertex_element *elements);
void
nv50_vertex_state_delete(struct pipe_context *pipe, void *hwcso);

void nv50_vertex_arrays_validate(struct nv50_context *nv50);

/* nv50_push.c */
void nv50_push_vbo(struct nv50_context *, const struct pipe_draw_info *);

/* nv50_compute.c */
int nv50_compute_init(struct nv50_screen *screen);
void nv50_compute_deinit(struct nv50_screen *screen);
void nv50_compute_validate_resources(struct nv50_context *nv50);
void nv50_init_compute_functions(struct nv50_context *nv50);

#endif
