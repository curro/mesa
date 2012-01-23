/*
 * Copyright 2012 Francisco Jerez
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

#include "core/memory.hpp"
#include "core/format.hpp"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "util/u_sampler.h"

using namespace clover;

namespace {
   class box {
   public:
      box(const resource::point &origin, const resource::point &size) :
         pipe({ (unsigned)origin[0], (unsigned)origin[1],
                (unsigned)origin[2], (unsigned)size[0],
                (unsigned)size[1], (unsigned)size[2] }) {
      }

      operator const pipe_box *() {
         return &pipe;
      }

   protected:
      pipe_box pipe;
   };
}

_cl_mem::_cl_mem(clover::context &ctx, cl_mem_flags flags,
                 size_t size, void *host_ptr) :
   ctx(ctx), __flags(flags),
   __size(size), __host_ptr(host_ptr),
   __destroy_notify([]{}),
   data((char *)host_ptr, (host_ptr ? size : 0)) {
}

void
_cl_mem::destroy_notify(std::function<void ()> f) {
   __destroy_notify = f;
}

cl_mem_flags
_cl_mem::flags() const {
   return __flags;
}

size_t
_cl_mem::size() const {
   return __size;
}

void *
_cl_mem::host_ptr() const {
   return __host_ptr;
}

resource::resource(clover::device &dev, clover::memory_obj &obj) :
   dev(dev), obj(obj), pipe(NULL), offset{0} {
}

resource::~resource() {
}

void
resource::copy(command_queue &q, const point &origin, const point &region,
               resource &src_res, const point &src_origin) {
   point p = offset + origin;

   q.pipe->resource_copy_region(q.pipe, pipe, 0, p[0], p[1], p[2],
                                src_res.pipe, 0,
                                box(src_res.offset + src_origin, region));
}

resource::transfer
resource::get_map(command_queue &q, cl_map_flags flags, bool blocking,
                  const point &origin, const point &region) {
   return { q, *this, flags, blocking, origin, region };
}

void *
resource::add_map(command_queue &q, cl_map_flags flags, bool blocking,
                  const point &origin, const point &region) {
   maps.emplace_back(get_map(q, flags, blocking, origin, region));
   return maps.back();
}

void
resource::del_map(void *p) {
   auto it = std::find(maps.begin(), maps.end(), p);
   if (it != maps.end())
      maps.erase(it);
}

unsigned
resource::map_count() {
   return maps.size();
}

pipe_sampler_view *
resource::bind(clover::command_queue &q, bool rw) {
   pipe_sampler_view info;

   u_sampler_view_default_template(&info, pipe, pipe->format);
   if (rw)
      info.writable = 1;

   return q.pipe->create_sampler_view(q.pipe, pipe, &info);
}

void
resource::unbind(clover::command_queue &q, pipe_sampler_view *st) {
   q.pipe->sampler_view_destroy(q.pipe, st);
}

resource::transfer::transfer(command_queue &q, resource &r,
                             cl_map_flags flags, bool blocking,
                             const point &origin,
                             const point &region) :
   ctx(q.pipe) {
   unsigned usage = ((flags & CL_MAP_WRITE ? PIPE_TRANSFER_WRITE : 0 ) |
                     (flags & CL_MAP_READ ? PIPE_TRANSFER_READ : 0 ) |
                     (blocking ? PIPE_TRANSFER_UNSYNCHRONIZED : 0));

   xfer = ctx->get_transfer(ctx, r.pipe, 0, usage,
                            box(origin + r.offset, region));
   if (!xfer)
      throw error(CL_OUT_OF_RESOURCES);

   map = ctx->transfer_map(ctx, xfer);
   if (!map) {
      ctx->transfer_destroy(ctx, xfer);
      throw error(CL_OUT_OF_RESOURCES);
   }
}

resource::transfer::transfer(transfer &&xfer) :
   ctx(xfer.ctx), xfer(xfer.xfer), map(xfer.map) {
   xfer.map = NULL;
   xfer.xfer = NULL;
}

resource::transfer::~transfer() {
   if (xfer) {
      ctx->transfer_unmap(ctx, xfer);
      ctx->transfer_destroy(ctx, xfer);
   }
}

root_resource::root_resource(clover::device &dev, clover::memory_obj &obj,
                             std::string data) :
   resource(dev, obj) {
   pipe_resource info {};

   if (image *img = dynamic_cast<image *>(&obj)) {
      info.format = translate_format(img->format());
      info.width0 = img->width();
      info.height0 = img->height();
      info.depth0 = img->depth();
   } else {
      info.width0 = obj.size();
   }

   info.target = translate_target(obj.type());
   info.bind = (PIPE_BIND_SAMPLER_VIEW |
                PIPE_BIND_WRITABLE_VIEW |
                PIPE_BIND_GLOBAL |
                PIPE_BIND_TRANSFER_READ |
                PIPE_BIND_TRANSFER_WRITE);

   pipe = dev.pipe->resource_create(dev.pipe, &info);
   if (!pipe)
      throw error(CL_OUT_OF_RESOURCES);

   assert(data.empty());
}

root_resource::root_resource(clover::device &dev, clover::memory_obj &obj,
                             clover::root_resource &) :
   resource(dev, obj) {
   assert(0);
}

root_resource::~root_resource() {
   dev.pipe->resource_destroy(dev.pipe, pipe);
}

sub_resource::sub_resource(clover::resource &r, point offset) :
   resource(r.dev, r.obj) {
   pipe = r.pipe;
   offset = r.offset + offset;
}

buffer::buffer(clover::context &ctx, cl_mem_flags flags,
               size_t size, void *host_ptr) :
   memory_obj(ctx, flags, size, host_ptr) {
}

cl_mem_object_type
buffer::type() const {
   return CL_MEM_OBJECT_BUFFER;
}

root_buffer::root_buffer(clover::context &ctx, cl_mem_flags flags,
                         size_t size, void *host_ptr) :
   buffer(ctx, flags, size, host_ptr) {
}

clover::resource &
root_buffer::resource(cl_command_queue q) {
   if (!resources.count(&q->dev)) {
      auto r = (!resources.empty() ?
                new root_resource(q->dev, *this, *resources.begin()->second) :
                new root_resource(q->dev, *this, data));

      resources.insert(std::make_pair(&q->dev, r));
      data.clear();
   }

   return *resources.find(&q->dev)->second;
}

sub_buffer::sub_buffer(clover::root_buffer &parent, cl_mem_flags flags,
                       size_t offset, size_t size) :
   buffer(parent.ctx, flags, size,
          (char *)parent.host_ptr() + offset),
   parent(parent), __offset(offset) {
}

clover::resource &
sub_buffer::resource(cl_command_queue q) {
   if (!resources.count(&q->dev))
      resources.insert(std::make_pair(&q->dev,
                       new sub_resource(parent.resource(q), { offset() })));

   return *resources.find(&q->dev)->second;
}

size_t
sub_buffer::offset() const {
   return __offset;
}

image::image(clover::context &ctx, cl_mem_flags flags,
             const cl_image_format *format,
             size_t width, size_t height, size_t depth,
             size_t row_pitch, size_t slice_pitch, size_t size,
             void *host_ptr) :
   memory_obj(ctx, flags, size, host_ptr),
   __format(*format), __width(width), __height(height), __depth(depth),
   __row_pitch(row_pitch), __slice_pitch(slice_pitch) {
}

clover::resource &
image::resource(cl_command_queue q) {
   if (!resources.count(&q->dev)) {
      auto r = (!resources.empty() ?
                new root_resource(q->dev, *this, *resources.begin()->second) :
                new root_resource(q->dev, *this, data));

      resources.insert(std::make_pair(&q->dev, r));
      data.clear();
   }

   return *resources.find(&q->dev)->second;
}

cl_image_format
image::format() const {
   return __format;
}

size_t
image::width() const {
   return __width;
}

size_t
image::height() const {
   return __height;
}

size_t
image::depth() const {
   return __depth;
}

size_t
image::row_pitch() const {
   return __row_pitch;
}

size_t
image::slice_pitch() const {
   return __slice_pitch;
}

image2d::image2d(clover::context &ctx, cl_mem_flags flags,
                 const cl_image_format *format, size_t width,
                 size_t height, size_t row_pitch,
                 void *host_ptr) :
   image(ctx, flags, format, width, height, 0,
         row_pitch, 0, height * row_pitch, host_ptr) {
}

cl_mem_object_type
image2d::type() const {
   return CL_MEM_OBJECT_IMAGE2D;
}

image3d::image3d(clover::context &ctx, cl_mem_flags flags,
                 const cl_image_format *format,
                 size_t width, size_t height, size_t depth,
                 size_t row_pitch, size_t slice_pitch,
                 void *host_ptr) :
   image(ctx, flags, format, width, height, depth,
         row_pitch, slice_pitch, depth * slice_pitch,
         host_ptr) {
}

cl_mem_object_type
image3d::type() const {
   return CL_MEM_OBJECT_IMAGE3D;
}
