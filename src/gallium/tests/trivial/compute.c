/*
 * Copyright (C) 2011 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <inttypes.h>
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_defines.h"
#include "pipe/p_shader_tokens.h"
#include "cso_cache/cso_context.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_sampler.h"
#include "util/u_format.h"
#include "tgsi/tgsi_text.h"
#include "loader/ws_loader.h"

#define MAX_RESOURCES 4

struct context {
        struct ws_loader_device *dev;
	struct pipe_screen *screen;
	struct pipe_context *pipe;
        void *hwcs;
        void *hwsmp[MAX_RESOURCES];
        struct pipe_resource *tex[MAX_RESOURCES];
        struct pipe_sampler_view *view[MAX_RESOURCES];
};

#define DUMP_COMPUTE_PARAM(p, c) do {                                   \
                uint64_t __v[4];                                        \
                int __i, __n;                                           \
                                                                        \
                __n = ctx->screen->get_compute_param(ctx->screen, c, __v); \
                printf("%s: {", #c);                                    \
                                                                        \
                for (__i = 0; __i < __n; ++__i)                         \
                        printf(" %"PRIu64, __v[__i]);                   \
                                                                        \
                printf(" }\n");                                         \
        } while (0)

static void init_ctx(struct context *ctx)
{
        int ret;

        ret = ws_loader_probe(&ctx->dev, 1);
        assert(ret);

	ctx->screen = ws_loader_create_screen(ctx->dev, PIPE_PREFIX, ".");
        assert(ctx->screen);

	ctx->pipe = ctx->screen->context_create(ctx->screen, NULL);
        assert(ctx->pipe);

        DUMP_COMPUTE_PARAM(p, PIPE_COMPUTE_CAP_GRID_DIMENSION);
        DUMP_COMPUTE_PARAM(p, PIPE_COMPUTE_CAP_MAX_GRID_SIZE);
        DUMP_COMPUTE_PARAM(p, PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE);
}

static void destroy_ctx(struct context *ctx)
{
	ctx->pipe->destroy(ctx->pipe);
	ctx->screen->destroy(ctx->screen);
        ws_loader_release(&ctx->dev, 1);
	FREE(ctx);
}

static void init_prog(struct context *ctx, unsigned local_sz,
                      unsigned private_sz, unsigned input_sz,
                      const char *src)
{
        struct pipe_context *pipe = ctx->pipe;
        struct tgsi_token prog[1024];
        struct pipe_compute_state cs = {
                .tokens = prog,
                .req_local_mem = local_sz,
                .req_private_mem = private_sz,
                .req_input_mem = input_sz
        };
        int ret;

        ret = tgsi_text_translate(src, prog, Elements(prog));
        assert(ret);

        ctx->hwcs = pipe->create_compute_state(pipe, &cs);
        assert(ctx->hwcs);

        pipe->bind_compute_state(pipe, ctx->hwcs);
}

static void destroy_prog(struct context *ctx)
{
        struct pipe_context *pipe = ctx->pipe;

        pipe->delete_compute_state(pipe, ctx->hwcs);
        ctx->hwcs = NULL;
}

static void init_tex(struct context *ctx, int slot,
                     enum pipe_texture_target target,
                     enum pipe_format format, int w, int h,
                     void (*init)(void *, int, int, int))
{
        struct pipe_context *pipe = ctx->pipe;
        struct pipe_resource **tex = &ctx->tex[slot];
        struct pipe_resource ttex = {
                .target = target,
                .format = format,
                .width0 = w,
                .height0 = h,
                .depth0 = 1,
                .array_size = 1,
                .bind = (PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_WRITABLE_VIEW |
                         PIPE_BIND_GLOBAL)
        };
        int dx = util_format_get_blocksize(format);
        int dy = util_format_get_stride(format, w);
        int nx = (target == PIPE_BUFFER ? (w / dx) :
                  util_format_get_nblocksx(format, w));
        int ny = (target == PIPE_BUFFER ? 1 :
                  util_format_get_nblocksy(format, h));
        struct pipe_transfer *xfer;
        char *map;
        int x, y;

        *tex = ctx->screen->resource_create(ctx->screen, &ttex);
        assert(*tex);

        xfer = pipe->get_transfer(pipe, *tex, 0, PIPE_TRANSFER_WRITE,
                                  &(struct pipe_box) { .width = w,
                                                  .height = h,
                                                  .depth = 1 });
        assert(xfer);

        map = pipe->transfer_map(pipe, xfer);
        assert(map);

        for (y = 0; y < ny; ++y) {
                for (x = 0; x < nx; ++x) {
                        init(map + y * dy + x * dx, slot, x, y);
                }
        }

        pipe->transfer_unmap(pipe, xfer);
        pipe->transfer_destroy(pipe, xfer);
}

static void check_tex(struct context *ctx, int slot,
                      void (*check)(void *, int, int, int))
{
        struct pipe_context *pipe = ctx->pipe;
        struct pipe_resource *tex = ctx->tex[slot];
        int dx = util_format_get_blocksize(tex->format);
        int dy = util_format_get_stride(tex->format, tex->width0);
        int nx = (tex->target == PIPE_BUFFER ? (tex->width0 / dx) :
                  util_format_get_nblocksx(tex->format, tex->width0));
        int ny = (tex->target == PIPE_BUFFER ? 1 :
                  util_format_get_nblocksy(tex->format, tex->height0));
        struct pipe_transfer *xfer;
        char *map;
        int x, y;
        int err = 0;

        xfer = pipe->get_transfer(pipe, tex, 0, PIPE_TRANSFER_READ,
                                  &(struct pipe_box) { .width = tex->width0,
                                        .height = tex->height0,
                                        .depth = 1 });
        assert(xfer);

        map = pipe->transfer_map(pipe, xfer);
        assert(map);

        for (y = 0; y < ny; ++y) {
                for (x = 0; x < nx; ++x) {
                        uint32_t expect[4];
                        uint32_t *result = (uint32_t *)(map + y * dy + x * dx);

                        check(expect, slot, x, y);
                        if (memcmp(result, expect, dx) && (++err) < 20) {
                                printf("(%d, %d): got 0x%x/%d/%f,"
                                       " expected 0x%x/%d/%f\n", x, y,
                                       *result, *result, *(float *)result,
                                       *expect, *expect, *(float *)expect);
                        }
                }
        }

        pipe->transfer_unmap(pipe, xfer);
        pipe->transfer_destroy(pipe, xfer);

        if (err)
                printf("(%d, %d): \x1b[31mFAIL\x1b[0m (%d)\n", x, y, err);
        else
                printf("(%d, %d): \x1b[32mOK\x1b[0m\n", x, y);
}

static void destroy_tex(struct context *ctx)
{
        int i;

        for (i = 0; i < MAX_RESOURCES; ++i) {
                if (ctx->tex[i])
                        pipe_resource_reference(&ctx->tex[i], NULL);
        }
}

static void init_sampler_views(struct context *ctx, const int *slots)
{
        struct pipe_context *pipe = ctx->pipe;
        struct pipe_sampler_view tview;
        int i;

        for (i = 0; *slots >= 0; ++i, ++slots) {
                u_sampler_view_default_template(&tview, ctx->tex[*slots],
                                                ctx->tex[*slots]->format);
                tview.writable = 1;

                ctx->view[i] = pipe->create_sampler_view(pipe, ctx->tex[*slots],
                                                         &tview);
                assert(ctx->view);
        }

        pipe->set_compute_sampler_views(pipe, i, ctx->view);
}

static void destroy_sampler_views(struct context *ctx)
{
        struct pipe_context *pipe = ctx->pipe;
        int i;

        pipe->set_compute_sampler_views(pipe, 0, NULL);

        for (i = 0; i < MAX_RESOURCES; ++i) {
                if (ctx->view[i]) {
                        pipe->sampler_view_destroy(pipe, ctx->view[i]);
                        ctx->view[i] = NULL;
                }
        }
}

static void init_sampler_states(struct context *ctx, int n)
{
        struct pipe_context *pipe = ctx->pipe;
        struct pipe_sampler_state smp = {
                .normalized_coords = 1,
        };
        int i;

        for (i = 0; i < n; ++i) {
                ctx->hwsmp[i] = pipe->create_sampler_state(pipe, &smp);
                assert(ctx->hwsmp[i]);
        }

        pipe->bind_compute_sampler_states(pipe, i, ctx->hwsmp);
}

static void destroy_sampler_states(struct context *ctx)
{
        struct pipe_context *pipe = ctx->pipe;
        int i;

        pipe->bind_compute_sampler_states(pipe, 0, NULL);

        for (i = 0; i < MAX_RESOURCES; ++i) {
                if (ctx->hwsmp[i]) {
                        pipe->delete_sampler_state(pipe, ctx->hwsmp[i]);
                        ctx->hwsmp[i] = NULL;
                }
        }
}

static void init_globals(struct context *ctx, const int *slots,
                         uint32_t **handles)
{
        struct pipe_context *pipe = ctx->pipe;
        struct pipe_resource *res[MAX_RESOURCES];
        int i;

        for (i = 0; *slots >= 0; ++i, ++slots)
                res[i] = ctx->tex[*slots];

        pipe->set_global_binding(pipe, i, res, handles);
}

static void destroy_globals(struct context *ctx)
{
        struct pipe_context *pipe = ctx->pipe;

        pipe->set_global_binding(pipe, 0, NULL, NULL);
}

static void launch_grid(struct context *ctx, const uint *block_layout,
                        const uint *grid_layout, uint32_t pc,
                        const void *input)
{
        struct pipe_context *pipe = ctx->pipe;

        pipe->launch_grid(pipe, block_layout, grid_layout, pc, input);
}

static void test_system_values(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], BUFFER, RAW, WR\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL SV[1], BLOCK_SIZE[0]\n"
                "DCL SV[2], GRID_SIZE[0]\n"
                "DCL SV[3], THREAD_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "IMM UINT32 { 64, 0, 0, 0 }\n"
                "IMM UINT32 { 16, 0, 0, 0 }\n"
                "IMM UINT32 { 0, 0, 0, 0 }\n"
                "\n"
                "BGNSUB"
                "  UMUL TEMP[0], SV[0], SV[1]\n"
                "  UADD TEMP[0], TEMP[0], SV[3]\n"
                "  UMUL TEMP[1], SV[1], SV[2]\n"
                "  UMUL TEMP[0].w, TEMP[0], TEMP[1].zzzz\n"
                "  UMUL TEMP[0].zw, TEMP[0], TEMP[1].yyyy\n"
                "  UMUL TEMP[0].yzw, TEMP[0], TEMP[1].xxxx\n"
                "  UADD TEMP[0].xy, TEMP[0].xyxy, TEMP[0].zwzw\n"
                "  UADD TEMP[0].x, TEMP[0].xxxx, TEMP[0].yyyy\n"
                "  UMUL TEMP[0].x, TEMP[0], IMM[0]\n"
                "  STORE RES[0].xyzw, TEMP[0], SV[0]\n"
                "  UADD TEMP[0].x, TEMP[0], IMM[1]\n"
                "  STORE RES[0].xyzw, TEMP[0], SV[1]\n"
                "  UADD TEMP[0].x, TEMP[0], IMM[1]\n"
                "  STORE RES[0].xyzw, TEMP[0], SV[2]\n"
                "  UADD TEMP[0].x, TEMP[0], IMM[1]\n"
                "  STORE RES[0].xyzw, TEMP[0], SV[3]\n"
                "  RET\n"
                "ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef;
        }
        void check(void *p, int s, int x, int y) {
                int id = x / 16, sv = (x % 16) / 4, c = x % 4;
                int tid[] = { id % 20, (id % 240) / 20, id / 240, 0 };
                int bsz[] = { 4, 3, 5, 1};
                int gsz[] = { 5, 4, 1, 1};

                switch (sv) {
                case 0:
                        *(uint32_t *)p = tid[c] / bsz[c];
                        break;
                case 1:
                        *(uint32_t *)p = bsz[c];
                        break;
                case 2:
                        *(uint32_t *)p = gsz[c];
                        break;
                case 3:
                        *(uint32_t *)p = tid[c] % bsz[c];
                        break;
                }
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 0, 0, src);
        init_tex(ctx, 0, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 76800, 0, init);
        init_sampler_views(ctx, (int []) { 0, -1 });
        launch_grid(ctx, (uint []){4, 3, 5}, (uint []){5, 4, 1}, 0, NULL);
        check_tex(ctx, 0, check);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_resource_access(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], BUFFER, RAW, WR\n"
                "DCL RES[1], 2D, RAW, WR\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "IMM UINT32 { 15, 0, 0, 0 }\n"
                "IMM UINT32 { 4, 1, 0, 0 }\n"
                "\n"
                "    BGNSUB\n"
                "       UADD TEMP[0].x, SV[0].xxxx, SV[0].yyyy\n"
                "       AND TEMP[0].x, TEMP[0], IMM[0]\n"
                "       UMUL TEMP[0].x, TEMP[0], IMM[1]\n"
                "       LOAD TEMP[0].x, RES[0], TEMP[0]\n"
                "       UMUL TEMP[1], SV[0], IMM[1]\n"
                "       STORE RES[1].x, TEMP[1], TEMP[0]\n"
                "       RET\n"
                "    ENDSUB\n";
        void init0(void *p, int s, int x, int y) {
                *(float *)p = 8.0 - (float)x;
        }
        void init1(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef;
        }
        void check(void *p, int s, int x, int y) {
                *(float *)p = 8.0 - (float)((x + y) & 0xf);
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 0, 0, src);
        init_tex(ctx, 0, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 64, 0, init0);
        init_tex(ctx, 1, PIPE_TEXTURE_2D, PIPE_FORMAT_R32_FLOAT, 15, 12, init1);
        init_sampler_views(ctx, (int []) { 0, 1, -1 });
        launch_grid(ctx, (uint []){1, 1, 1}, (uint []){15, 12, 1}, 0, NULL);
        check_tex(ctx, 1, check);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_function_calls(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], 2D, RAW, WR\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL SV[1], BLOCK_SIZE[0]\n"
                "DCL SV[2], GRID_SIZE[0]\n"
                "DCL SV[3], THREAD_ID[0]\n"
                "DCL TEMP[0]\n"
                "DCL TEMP[1]\n"
                "DCL TEMP[2], LOCAL\n"
                "IMM UINT32 { 0, 11, 22, 33 }\n"
                "IMM FLT32 { 11, 33, 55, 99 }\n"
                "IMM UINT32 { 4, 1, 0, 0 }\n"
                "IMM UINT32 { 12, 0, 0, 0 }\n"
                "\n"
                "00: BGNSUB\n"
                "01:  UMUL TEMP[0].x, TEMP[0], TEMP[0]\n"
                "02:  UADD TEMP[1].x, TEMP[1], IMM[2].yyyy\n"
                "03:  USLT TEMP[0].x, TEMP[0], IMM[0]\n"
                "04:  RET\n"
                "05: ENDSUB\n"
                "06: BGNSUB\n"
                "07:  UMUL TEMP[0].x, TEMP[0], TEMP[0]\n"
                "08:  UADD TEMP[1].x, TEMP[1], IMM[2].yyyy\n"
                "09:  USLT TEMP[0].x, TEMP[0], IMM[0].yyyy\n"
                "10:  IF TEMP[0].xxxx\n"
                "11:   CAL :0\n"
                "12:  ENDIF\n"
                "13:  RET\n"
                "14: ENDSUB\n"
                "15: BGNSUB\n"
                "16:  UMUL TEMP[2], SV[0], SV[1]\n"
                "17:  UADD TEMP[2], TEMP[2], SV[3]\n"
                "18:  UMUL TEMP[2], TEMP[2], IMM[2]\n"
                "00:  MOV TEMP[1].x, IMM[2].wwww\n"
                "19:  LOAD TEMP[0].x, RES[0].xxxx, TEMP[2]\n"
                "20:  CAL :6\n"
                "21:  STORE RES[0].x, TEMP[2], TEMP[1].xxxx\n"
                "22:  RET\n"
                "23: ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(uint32_t *)p = 15 * y + x;
        }
        void check(void *p, int s, int x, int y) {
                *(uint32_t *)p = (15 * y + x) < 4 ? 2 : 1 ;
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 0, 0, src);
        init_tex(ctx, 0, PIPE_TEXTURE_2D, PIPE_FORMAT_R32_FLOAT, 15, 12, init);
        init_sampler_views(ctx, (int []) { 0, -1 });
        launch_grid(ctx, (uint []){3, 3, 3}, (uint []){5, 4, 1}, 15, NULL);
        check_tex(ctx, 0, check);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_input_global(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL SV[0], THREAD_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "IMM UINT32 { 8, 0, 0, 0 }\n"
                "\n"
                "    BGNSUB\n"
                "       UMUL TEMP[0], SV[0], IMM[0]\n"
                "       LOAD TEMP[1].xy, RES[-4], TEMP[0]\n"
                "       LOAD TEMP[0].x, RES[-1], TEMP[1].yyyy\n"
                "       UADD TEMP[1].x, TEMP[0], -TEMP[1]\n"
                "       STORE RES[-1].x, TEMP[1].yyyy, TEMP[1]\n"
                "       RET\n"
                "    ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef;
        }
        void check(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef - (x == 0 ? 0x10001 + 2 * s : 0);
        }
        uint32_t input[8] = { 0x10001, 0x10002, 0x10003, 0x10004,
                              0x10005, 0x10006, 0x10007, 0x10008 };

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 0, 32, src);
        init_tex(ctx, 0, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 32, 0, init);
        init_tex(ctx, 1, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 32, 0, init);
        init_tex(ctx, 2, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 32, 0, init);
        init_tex(ctx, 3, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 32, 0, init);
        init_globals(ctx, (int []){ 0, 1, 2, 3, -1 },
                     (uint32_t *[]){ &input[1], &input[3],
                                     &input[5], &input[7] });
        launch_grid(ctx, (uint []){4, 1, 1}, (uint []){1, 1, 1}, 0, input);
        check_tex(ctx, 0, check);
        check_tex(ctx, 1, check);
        check_tex(ctx, 2, check);
        check_tex(ctx, 3, check);
        destroy_globals(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_private(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], BUFFER, RAW, WR\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL SV[1], BLOCK_SIZE[0]\n"
                "DCL SV[2], THREAD_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "DCL TEMP[2], LOCAL\n"
                "IMM UINT32 { 128, 0, 0, 0 }\n"
                "IMM UINT32 { 4, 0, 0, 0 }\n"
                "\n"
                "    BGNSUB\n"
                "       UMUL TEMP[0].x, SV[0], SV[1]\n"
                "       UADD TEMP[0].x, TEMP[0], SV[2]\n"
                "       MOV TEMP[1].x, IMM[0].wwww\n"
                "       BGNLOOP\n"
                "               USEQ TEMP[2].x, TEMP[1], IMM[0]\n"
                "               IF TEMP[2]\n"
                "                       BRK\n"
                "               ENDIF\n"
                "               UDIV TEMP[2].x, TEMP[1], IMM[1]\n"
                "               UADD TEMP[2].x, TEMP[2], TEMP[0]\n"
                "               STORE RES[-3].x, TEMP[1], TEMP[2]\n"
                "               UADD TEMP[1].x, TEMP[1], IMM[1]\n"
                "       ENDLOOP\n"
                "       MOV TEMP[1].x, IMM[0].wwww\n"
                "       UMUL TEMP[0].x, TEMP[0], IMM[0]\n"
                "       BGNLOOP\n"
                "               USEQ TEMP[2].x, TEMP[1], IMM[0]\n"
                "               IF TEMP[2]\n"
                "                       BRK\n"
                "               ENDIF\n"
                "               LOAD TEMP[2].x, RES[-3], TEMP[1]\n"
                "               STORE RES[0].x, TEMP[0], TEMP[2]\n"
                "               UADD TEMP[0].x, TEMP[0], IMM[1]\n"
                "               UADD TEMP[1].x, TEMP[1], IMM[1]\n"
                "       ENDLOOP\n"
                "       RET\n"
                "    ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef;
        }
        void check(void *p, int s, int x, int y) {
                *(uint32_t *)p = (x / 32) + x % 32;
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 128, 0, src);
        init_tex(ctx, 0, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 32768, 0, init);
        init_sampler_views(ctx, (int []) { 0, -1 });
        launch_grid(ctx, (uint []){16, 1, 1}, (uint []){16, 1, 1}, 0, NULL);
        check_tex(ctx, 0, check);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_local(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], BUFFER, RAW, WR\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL SV[1], BLOCK_SIZE[0]\n"
                "DCL SV[2], THREAD_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "DCL TEMP[2], LOCAL\n"
                "IMM UINT32 { 1, 0, 0, 0 }\n"
                "IMM UINT32 { 2, 0, 0, 0 }\n"
                "IMM UINT32 { 4, 0, 0, 0 }\n"
                "IMM UINT32 { 32, 0, 0, 0 }\n"
                "\n"
                "    BGNSUB\n"
                "       UMUL TEMP[0].x, SV[2], IMM[2]\n"
                "       STORE RES[-2].x, TEMP[0], IMM[0].wwww\n"
                "       USLT TEMP[1].x, SV[2], IMM[3]\n"
                "       IF TEMP[1]\n"
                "               UADD TEMP[1].x, TEMP[0], IMM[3]\n"
                "               BGNLOOP\n"
                "                       LOAD TEMP[2].x, RES[-2], TEMP[1]\n"
                "                       USEQ TEMP[2].x, TEMP[2], IMM[0]\n"
                "                       IF TEMP[1]\n"
                "                               BRK\n"
                "                       ENDIF\n"
                "               ENDLOOP\n"
                "               STORE RES[-2].x, TEMP[0], IMM[0]\n"
                "               BGNLOOP\n"
                "                       LOAD TEMP[2].x, RES[-2], TEMP[1]\n"
                "                       USEQ TEMP[2].x, TEMP[2], IMM[1]\n"
                "                       IF TEMP[1]\n"
                "                               BRK\n"
                "                       ENDIF\n"
                "               ENDLOOP\n"
                "       ELSE\n"
                "               UADD TEMP[1].x, TEMP[0], -IMM[3]\n"
                "               BGNLOOP\n"
                "                       LOAD TEMP[2].x, RES[-2], TEMP[1]\n"
                "                       USEQ TEMP[2].x, TEMP[2], IMM[0].wwww\n"
                "                       IF TEMP[1]\n"
                "                               BRK\n"
                "                       ENDIF\n"
                "               ENDLOOP\n"
                "               STORE RES[-2].x, TEMP[0], IMM[0]\n"
                "               BGNLOOP\n"
                "                       LOAD TEMP[2].x, RES[-2], TEMP[1]\n"
                "                       USEQ TEMP[2].x, TEMP[2], IMM[0]\n"
                "                       IF TEMP[1]\n"
                "                               BRK\n"
                "                       ENDIF\n"
                "               ENDLOOP\n"
                "               STORE RES[-2].x, TEMP[0], IMM[1]\n"
                "       ENDIF\n"
                "       UMUL TEMP[1].x, SV[0], SV[1]\n"
                "       UMUL TEMP[1].x, TEMP[1], IMM[2]\n"
                "       UADD TEMP[1].x, TEMP[1], TEMP[0]\n"
                "       LOAD TEMP[0].x, RES[-2], TEMP[0]\n"
                "       STORE RES[0].x, TEMP[1], TEMP[0]\n"
                "       RET\n"
                "    ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(uint32_t *)p = 0xdeadbeef;
        }
        void check(void *p, int s, int x, int y) {
                *(uint32_t *)p = x & 0x20 ? 2 : 1;
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 256, 0, 0, src);
        init_tex(ctx, 0, PIPE_BUFFER, PIPE_FORMAT_R32_FLOAT, 4096, 0, init);
        init_sampler_views(ctx, (int []) { 0, -1 });
        launch_grid(ctx, (uint []){64, 1, 1}, (uint []){16, 1, 1}, 0, NULL);
        check_tex(ctx, 0, check);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

static void test_sample(struct context *ctx)
{
        const char *src = "COMP\n"
                "DCL RES[0], 2D, WR, FLOAT\n"
                "DCL RES[1], 2D, RAW, WR\n"
                "DCL SAMP[0]\n"
                "DCL SV[0], BLOCK_ID[0]\n"
                "DCL TEMP[0], LOCAL\n"
                "DCL TEMP[1], LOCAL\n"
                "IMM UINT32 { 16, 1, 0, 0 }\n"
                "IMM FLT32 { 128, 32, 0, 0 }\n"
                "\n"
                "    BGNSUB\n"
                "       I2F TEMP[1], SV[0]\n"
                "       DIV TEMP[1], TEMP[1], IMM[1]\n"
                "       SAMPLE TEMP[1], TEMP[1], RES[0], SAMP[0]\n"
                "       UMUL TEMP[0], SV[0], IMM[0]\n"
                "       STORE RES[1].xyzw, TEMP[0], TEMP[1]\n"
                "       RET\n"
                "    ENDSUB\n";
        void init(void *p, int s, int x, int y) {
                *(float *)p = s ? 1 : x * y;
        }
        void check(void *p, int s, int x, int y) {
                switch (x % 4) {
                case 0:
                        *(float *)p = x / 4 * y;
                        break;
                case 1:
                case 2:
                        *(float *)p = 0;
                        break;
                case 3:
                        *(float *)p = 1;
                        break;
                }
        }

        printf("- %s\n", __func__);

        init_prog(ctx, 0, 0, 0, src);
        init_tex(ctx, 0, PIPE_TEXTURE_2D, PIPE_FORMAT_R32_FLOAT, 128, 32, init);
        init_tex(ctx, 1, PIPE_TEXTURE_2D, PIPE_FORMAT_R32_FLOAT, 512, 32, init);
        init_sampler_views(ctx, (int []) { 0, 1, -1 });
        init_sampler_states(ctx, 2);
        launch_grid(ctx, (uint []){1, 1, 1}, (uint []){128, 32, 1}, 0, NULL);
        check_tex(ctx, 1, check);
        destroy_sampler_states(ctx);
        destroy_sampler_views(ctx);
        destroy_tex(ctx);
        destroy_prog(ctx);
}

int main(int argc, char** argv)
{
	struct context *ctx = CALLOC_STRUCT(context);

	init_ctx(ctx);
	test_system_values(ctx);
	test_resource_access(ctx);
	test_function_calls(ctx);
	test_input_global(ctx);
	test_private(ctx);
	test_local(ctx);
	test_sample(ctx);
	destroy_ctx(ctx);

	return 0;
}