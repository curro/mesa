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

/**
 * @file
 * Winsys providing device enumeration and winsys multiplexing.
 */

#ifndef WS_LOADER_H
#define WS_LOADER_H

#include "pipe/p_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_screen;
struct ws_loader_backend;

enum ws_loader_device_type {
   WS_LOADER_DEVICE_SOFTWARE,
   WS_LOADER_DEVICE_PCI,
   MAX_WS_LOADER_DEVICE_TYPE
};

struct ws_loader_device {
   enum ws_loader_device_type type;

   union {
      struct {
         int vendor_id;
         int chip_id;
      } pci;
   }; /** Discriminated by \a type */

   const char *driver_name;
   struct ws_loader_ops *ops;
};

int
ws_loader_probe(struct ws_loader_device **devs, int ndev);

struct pipe_screen *
ws_loader_create_screen(struct ws_loader_device *dev,
                        const char *library_prefix, const char *library_paths);

void
ws_loader_release(struct ws_loader_device **devs, int ndev);

struct util_dl_library *
ws_loader_find_module(struct ws_loader_device *dev,
                      const char *library_prefix, const char *library_paths);

struct ws_loader_ops {
   struct pipe_screen *(*create_screen)(struct ws_loader_device *dev,
                                        const char *library_prefix,
                                        const char *library_paths);

   void (*release)(struct ws_loader_device **dev);
};

#ifdef HAVE_WS_LOADER_SW
int
ws_loader_sw_probe(struct ws_loader_device **devs, int ndev);
#endif

#ifdef HAVE_WS_LOADER_DRM
int
ws_loader_drm_probe(struct ws_loader_device **devs, int ndev);

boolean
ws_loader_drm_probe_fd(struct ws_loader_device **dev, int fd);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WS_LOADER_H */
