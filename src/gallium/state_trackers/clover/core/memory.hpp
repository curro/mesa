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

#ifndef __CORE_MEMORY_HPP__
#define __CORE_MEMORY_HPP__

#include <functional>
#include <list>
#include <map>
#include <memory>

#include "core/base.hpp"
#include "core/context.hpp"
#include "core/geometry.hpp"
#include "pipe/p_state.h"

namespace clover {
   typedef struct _cl_mem memory_obj;

   class resource;
   class sub_resource;
}

struct _cl_mem : public clover::ref_counter {
protected:
   _cl_mem(clover::context &ctx, cl_mem_flags flags,
           size_t size, void *host_ptr);
   _cl_mem(const _cl_mem &obj) = delete;

public:
   virtual ~_cl_mem() {
      __destroy_notify();
   }

   virtual cl_mem_object_type type() const = 0;
   virtual clover::resource &resource(cl_command_queue q) = 0;

   void destroy_notify(std::function<void ()> f);
   cl_mem_flags flags() const;
   size_t size() const;
   void *host_ptr() const;

   clover::context &ctx;

private:
   cl_mem_flags __flags;
   size_t __size;
   void *__host_ptr;
   std::function<void ()> __destroy_notify;

protected:
   std::string data;
};

namespace clover {
   class resource {
   public:
      typedef clover::point<size_t, 3> point;

      class transfer {
      public:
         transfer(command_queue &q, resource &r,
                  cl_map_flags flags, bool blocking,
                  const point &origin, const point &region);
         transfer(const transfer &xfer) = delete;
         transfer(transfer &&xfer);
         ~transfer();

         operator void *() {
            return map;
         }

         operator char *() {
            return (char *)map;
         }

      private:
         pipe_context *ctx;
         pipe_transfer *xfer;
         void *map;
      };

      resource(const resource &r) = delete;
      virtual ~resource();

      void copy(command_queue &q, const point &origin, const point &region,
                resource &src_resource, const point &src_origin);

      transfer get_map(command_queue &q, cl_map_flags flags, bool blocking,
                       const point &origin, const point &region);

      void *add_map(command_queue &q, cl_map_flags flags, bool blocking,
                    const point &origin, const point &region);
      void del_map(void *p);
      unsigned map_count();

      clover::device &dev;
      clover::memory_obj &obj;

      friend class sub_resource;
      friend struct ::_cl_kernel;

   protected:
      resource(clover::device &dev, clover::memory_obj &obj);

      pipe_sampler_view *bind(clover::command_queue &q, bool rw);
      void unbind(clover::command_queue &q, pipe_sampler_view *st);

      pipe_resource *pipe;
      point offset;

   private:
      std::list<transfer> maps;
   };

   class root_resource : public resource {
   public:
      root_resource(clover::device &dev, clover::memory_obj &obj,
                    std::string data);
      root_resource(clover::device &dev, clover::memory_obj &obj,
                    root_resource &r);
      virtual ~root_resource();
   };

   class sub_resource : public resource {
   public:
      sub_resource(clover::resource &r, point offset);
   };

   struct buffer : public memory_obj {
   protected:
      buffer(clover::context &ctx, cl_mem_flags flags,
             size_t size, void *host_ptr);

   public:
      virtual cl_mem_object_type type() const;
   };

   struct root_buffer : public buffer {
   public:
      root_buffer(clover::context &ctx, cl_mem_flags flags,
                  size_t size, void *host_ptr);

      virtual clover::resource &resource(cl_command_queue q);

   private:
      std::map<clover::device *,
               std::unique_ptr<clover::root_resource>> resources;
   };

   struct sub_buffer : public buffer {
   public:
      sub_buffer(clover::root_buffer &parent, cl_mem_flags flags,
                 size_t offset, size_t size);

      virtual clover::resource &resource(cl_command_queue q);
      size_t offset() const;

      clover::root_buffer &parent;

   private:
      size_t __offset;
      std::map<clover::device *,
               std::unique_ptr<clover::sub_resource>> resources;
   };

   struct image : public memory_obj {
   protected:
      image(clover::context &ctx, cl_mem_flags flags,
            const cl_image_format *format,
            size_t width, size_t height, size_t depth,
            size_t row_pitch, size_t slice_pitch, size_t size,
            void *host_ptr);

   public:
      virtual clover::resource &resource(cl_command_queue q);
      cl_image_format format() const;
      size_t width() const;
      size_t height() const;
      size_t depth() const;
      size_t row_pitch() const;
      size_t slice_pitch() const;

   private:
      cl_image_format __format;
      size_t __width;
      size_t __height;
      size_t __depth;
      size_t __row_pitch;
      size_t __slice_pitch;
      std::map<clover::device *,
               std::unique_ptr<clover::root_resource>> resources;
   };

   struct image2d : public image {
   public:
      image2d(clover::context &ctx, cl_mem_flags flags,
              const cl_image_format *format, size_t width,
              size_t height, size_t row_pitch,
              void *host_ptr);

      virtual cl_mem_object_type type() const;
   };

   struct image3d : public image {
   public:
      image3d(clover::context &ctx, cl_mem_flags flags,
              const cl_image_format *format,
              size_t width, size_t height, size_t depth,
              size_t row_pitch, size_t slice_pitch,
              void *host_ptr);

      virtual cl_mem_object_type type() const;
   };
}

#endif
