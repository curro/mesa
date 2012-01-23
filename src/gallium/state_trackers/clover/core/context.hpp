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

#ifndef __CORE_CONTEXT_HPP__
#define __CORE_CONTEXT_HPP__

#include "core/base.hpp"
#include "core/device.hpp"
#include "pipe/p_context.h"

namespace clover {
   typedef struct _cl_context context;
   typedef struct _cl_command_queue command_queue;
   typedef struct _cl_kernel kernel;
   class resource;
}

struct _cl_context : public clover::ref_counter {
public:
   _cl_context(const std::vector<cl_context_properties> &props,
               const std::vector<clover::device *> &devs);
   _cl_context(const _cl_context &ctx) = delete;

   const std::vector<cl_context_properties> &props() const {
      return __props;
   }

   const std::vector<clover::device *> devs;

private:
   std::vector<cl_context_properties> __props;
};

struct _cl_command_queue : public clover::ref_counter {
public:
   _cl_command_queue(clover::context &ctx, clover::device &dev,
                     cl_command_queue_properties props);
   _cl_command_queue(const _cl_command_queue &q) = delete;
   ~_cl_command_queue();

   void sequence(_cl_event *ev);

   cl_command_queue_properties props() const {
      return __props;
   }

   clover::context &ctx;
   clover::device &dev;

   friend class clover::resource;
   friend struct _cl_sampler;
   friend struct _cl_kernel;

private:
   cl_command_queue_properties __props;
   pipe_context *pipe;
   clover::ref_ptr<_cl_event> last_event;
};

#endif
