/**************************************************************************
 *
 * Copyright 2011 Francisco Jerez
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "loader/ws_loader.h"

#include "util/u_memory.h"
#include "util/u_dl.h"
#include "sw/null/null_sw_winsys.h"
#include "target-helpers/inline_sw_helper.h"
#include "state_tracker/xlib_sw_winsys.h"

struct ws_loader_sw_device {
   struct ws_loader_device base;
   struct util_dl_library *lib;
   struct sw_winsys *ws;
};

#define ws_loader_sw_device(dev) ((struct ws_loader_sw_device *)dev)

static struct ws_loader_ops ws_loader_sw_ops;

static struct sw_winsys *(*backends[])() = {
#ifdef HAVE_WINSYS_XLIB
   x11_sw_create,
#endif
   null_sw_create
};

int
ws_loader_sw_probe(struct ws_loader_device **devs, int ndev)
{
   int i;

   for (i = 0; i < Elements(backends); i++) {
      if (i < ndev) {
         struct ws_loader_sw_device *sdev = CALLOC_STRUCT(ws_loader_sw_device);

         sdev->base.type = WS_LOADER_DEVICE_SOFTWARE;
         sdev->base.driver_name = "swrast";
         sdev->base.ops = &ws_loader_sw_ops;
         sdev->ws = backends[i]();
         devs[i] = &sdev->base;
      }
   }

   return i;
}

static void
ws_loader_sw_release(struct ws_loader_device **dev)
{
   struct ws_loader_sw_device *sdev = ws_loader_sw_device(*dev);

   if (sdev->lib)
      util_dl_close(sdev->lib);
   if (sdev->ws)
      sdev->ws->destroy(sdev->ws);

   FREE(sdev);
   *dev = NULL;
}

static struct pipe_screen *
ws_loader_sw_create_screen(struct ws_loader_device *dev,
                           const char *library_prefix,
                           const char *library_paths)
{
   struct ws_loader_sw_device *sdev = ws_loader_sw_device(dev);
   struct pipe_screen *(*init)(struct sw_winsys *);

   if (!sdev->lib)
      sdev->lib = ws_loader_find_module(dev, library_prefix, library_paths);
   if (!sdev->lib)
      return NULL;

   init = (void *)util_dl_get_proc_address(sdev->lib, "swrast_create_screen");
   if (!init)
      return NULL;

   return init(sdev->ws);
}

static struct ws_loader_ops ws_loader_sw_ops = {
   .create_screen = ws_loader_sw_create_screen,
   .release = ws_loader_sw_release
};
