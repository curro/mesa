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

#include "api/util.hpp"
#include "core/event.hpp"

using namespace clover;

PUBLIC cl_event
clCreateUserEvent(cl_context ctx, cl_int *errcode_ret) {
   set_error(errcode_ret, CL_SUCCESS);
   return new soft_event(*ctx, {});
}

PUBLIC cl_int
clSetUserEventStatus(cl_event ev, cl_int status) {
   if (status)
      ev->abort(status);
   else
      ev->trigger();

   return CL_SUCCESS;
}

PUBLIC cl_int
clWaitForEvents(cl_uint num_evs, const cl_event *evs) {
   if (!num_evs || !evs)
      return CL_INVALID_VALUE;

   ref_ptr<soft_event> sev = transfer(
      new soft_event(evs[0]->ctx, { evs, evs + num_evs }));
   sev->wait();

   return CL_SUCCESS;
}

PUBLIC cl_int
clGetEventInfo(cl_event ev, cl_event_info param,
               size_t size, void *buf, size_t *size_ret) {
   switch (param) {
   case CL_EVENT_COMMAND_QUEUE:
      return scalar_property<cl_command_queue>(buf, size, size_ret, ev->queue());

   case CL_EVENT_CONTEXT:
      return scalar_property<cl_context>(buf, size, size_ret, &ev->ctx);

   case CL_EVENT_COMMAND_TYPE:
      return scalar_property<cl_command_type>(buf, size, size_ret, ev->command());

   case CL_EVENT_COMMAND_EXECUTION_STATUS:
      return scalar_property<cl_int>(buf, size, size_ret, ev->status());

   case CL_EVENT_REFERENCE_COUNT:
      return scalar_property<cl_uint>(buf, size, size_ret, ev->ref_count());

   default:
      return CL_INVALID_VALUE;
   }
}

PUBLIC cl_int
clSetEventCallback(cl_event ev, cl_int type,
                   void (CL_CALLBACK *pfn_event_notify)(cl_event, cl_int,
                                                        void *),
                   void *user_data) {
   ref_ptr<soft_event> sev = transfer(
      new soft_event(ev->ctx, { ev }, [=](event &) {
            pfn_event_notify(ev, ev->status(), user_data);
         }));
   return CL_SUCCESS;
}

PUBLIC cl_int
clRetainEvent(cl_event ev) {
   ev->retain();
   return CL_SUCCESS;
}

PUBLIC cl_int
clReleaseEvent(cl_event ev) {
   if (ev->release())
      delete ev;
   return CL_SUCCESS;
}

PUBLIC cl_int
clEnqueueMarker(cl_command_queue q, cl_event *ev) {
   if (!ev)
      return CL_INVALID_VALUE;

   *ev = new hard_event(*q, CL_COMMAND_MARKER, {});

   return CL_SUCCESS;
}

PUBLIC cl_int
clEnqueueBarrier(cl_command_queue q) {
   return CL_SUCCESS;
}

PUBLIC cl_int
clEnqueueWaitForEvents(cl_command_queue q, cl_uint num_evs,
                       const cl_event *evs) {
   ref_ptr<hard_event> hev = transfer(
      new hard_event(*q, 0, { evs, evs + num_evs }));

   return CL_SUCCESS;
}

PUBLIC cl_int
clGetEventProfilingInfo(cl_event ev, cl_profiling_info param,
                        size_t size, void *buf, size_t *size_ret) {
   return CL_PROFILING_INFO_NOT_AVAILABLE;
}

PUBLIC cl_int
clFinish(cl_command_queue q) {
   ref_ptr<hard_event> hev = transfer(new hard_event(*q, 0, { }));
   hev->wait();

   return CL_SUCCESS;
}
