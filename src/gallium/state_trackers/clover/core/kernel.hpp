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

#ifndef __CORE_KERNEL_HPP__
#define __CORE_KERNEL_HPP__

#include <memory>

#include "core/base.hpp"
#include "core/program.hpp"
#include "core/memory.hpp"
#include "core/sampler.hpp"

namespace clover {
   typedef struct _cl_kernel kernel;
   class argument;
}

struct _cl_kernel : public clover::ref_counter {
private:
   struct exec_context {
      exec_context();
      ~exec_context();

      void *bind(clover::kernel *kern, clover::command_queue *q);
      void unbind();

      clover::kernel *kern;
      clover::command_queue *q;

      std::vector<uint8_t> input;
      std::vector<void *> samplers;
      std::vector<pipe_sampler_view *> resources;
      std::vector<pipe_resource *> g_buffers;
      std::vector<size_t> g_handles;
      size_t mem_local;

   private:
      void *st;
      struct pipe_compute_state cs;
   };

public:
   class argument {
   public:
      virtual void set(size_t size, const void *value) = 0;
      virtual void bind(exec_context &ctx) = 0;
      virtual void unbind(exec_context &ctx) = 0;
   };

   _cl_kernel(clover::program &prog,
               const std::string &name);

   void launch(clover::command_queue &q,
               const std::vector<size_t> &grid_offset,
               const std::vector<size_t> &grid_size,
               const std::vector<size_t> &block_size);

   size_t mem_local() const;
   size_t mem_private() const;
   size_t max_block_size() const;

   const std::string &name() const;
   std::vector<size_t> block_size() const;

   clover::program &prog;
   std::vector<std::unique_ptr<argument>> args;

private:
   class scalar_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);

   private:
      uint32_t v;
   };

   class global_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);

   private:
      clover::buffer *obj;
   };

   class local_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);
      virtual size_t size() const;

   private:
      size_t __size;
   };

   class constant_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);

   private:
      clover::buffer *obj;
      pipe_sampler_view *st;
   };

   class image_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);

   private:
      clover::image *obj;
      pipe_sampler_view *st;
   };

   class sampler_argument : public argument {
   public:
      virtual void set(size_t size, const void *value);
      virtual void bind(exec_context &ctx);
      virtual void unbind(exec_context &ctx);

   private:
      clover::sampler *obj;
      void *st;
   };

   std::string __name;
   unsigned pc;
   exec_context exec;
};

#endif
