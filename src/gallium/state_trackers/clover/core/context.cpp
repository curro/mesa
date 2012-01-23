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

#include "core/context.hpp"
#include "core/event.hpp"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"

using namespace clover;

_cl_context::_cl_context(const std::vector<cl_context_properties> &props,
                         const std::vector<device *> &devs) :
   devs(devs), __props(props) {
}

_cl_command_queue::_cl_command_queue(context &ctx, device &dev,
                                     cl_command_queue_properties props) :
   ctx(ctx), dev(dev), __props(props) {
   pipe = dev.pipe->context_create(dev.pipe, NULL);
   if (!pipe)
      throw error(CL_INVALID_DEVICE);
}

_cl_command_queue::~_cl_command_queue() {
   pipe->destroy(pipe);
}

void
_cl_command_queue::sequence(_cl_event *ev) {
   if (last_event)
      last_event->chain(ev);
   last_event = ev;
}
