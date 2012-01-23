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

#ifndef __CORE_EVENT_HPP__
#define __CORE_EVENT_HPP__

#include <functional>
#include <atomic>

#include "core/base.hpp"
#include "core/context.hpp"

namespace clover {
   typedef struct _cl_event event;
}

struct _cl_event : public clover::ref_counter {
public:
   typedef std::function<void (clover::event &)> action;

   _cl_event(clover::context &ctx, std::vector<clover::event *> deps,
             action action_ok, action action_fail);

   void chain(clover::event *ev);

   void trigger();
   void abort(cl_int status);

   cl_int status() const;
   virtual cl_command_queue queue() const = 0;
   virtual cl_command_type command() const = 0;
   virtual void wait() const = 0;

   clover::context &ctx;

private:
   unsigned wait_count;
   action action_ok;
   action action_fail;
   cl_int __status;
   std::vector<clover::ref_ptr<clover::event>> __chain;
};

namespace clover {
   class hard_event : public event {
   public:
      hard_event(clover::command_queue &q, cl_command_type command,
                 std::vector<clover::event *> deps,
                 action action = [](event &){});

      virtual cl_command_queue queue() const;
      virtual cl_command_type command() const;
      virtual void wait() const;

   private:
      clover::command_queue &__queue;
      cl_command_type &__command;
   };

   class soft_event : public event {
   public:
      soft_event(clover::context &ctx, std::vector<clover::event *> deps,
                 action action = [](event &){});

      virtual cl_command_queue queue() const;
      virtual cl_command_type command() const;
      virtual void wait() const;
   };
}

#endif
