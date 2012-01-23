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

#include "core/event.hpp"

using namespace clover;

_cl_event::_cl_event(clover::context &ctx,
                     std::vector<clover::event *> deps,
                     action action_ok, action action_fail) :
   ctx(ctx), wait_count(1), action_ok(action_ok),
   action_fail(action_fail), __status(CL_QUEUED) {
   for (auto ev : deps)
      ev->chain(this);
}

void
_cl_event::chain(clover::event *ev) {
   if (wait_count) {
      ev->wait_count++;
      __chain.push_back(ev);
   }
}

void
_cl_event::trigger() {
   if (!--wait_count) {
      __status = CL_COMPLETE;
      action_ok(*this);

      for (auto ev : __chain)
         ev->trigger();
   }
}

void
_cl_event::abort(cl_int status) {
   __status = status;
   action_fail(*this);

   for (auto ev : __chain)
      ev->abort(status);
}

cl_int
_cl_event::status() const {
   return __status;
}

hard_event::hard_event(clover::command_queue &q, cl_command_type command,
                       std::vector<clover::event *> deps, action action) :
   _cl_event(q.ctx, deps, action, [](event &ev){}),
   __queue(q), __command(command) {
   q.sequence(this);
   trigger();
}

cl_command_queue
hard_event::queue() const {
   return &__queue;
}

cl_command_type
hard_event::command() const {
   return __command;
}

void
hard_event::wait() const {
   assert(status() == CL_COMPLETE);
}

soft_event::soft_event(clover::context &ctx,
                       std::vector<clover::event *> deps, action action) :
   _cl_event(ctx, deps, action, action) {
}

cl_command_queue
soft_event::queue() const {
   return NULL;
}

cl_command_type
soft_event::command() const {
   return CL_COMMAND_USER;
}

void
soft_event::wait() const {
   assert(status() == CL_COMPLETE);
}
