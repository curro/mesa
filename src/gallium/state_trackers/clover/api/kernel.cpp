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
#include "core/kernel.hpp"
#include "core/event.hpp"

using namespace clover;

PUBLIC cl_kernel
clCreateKernel(cl_program prog, const char *name,
               cl_int *errcode_ret) {
   set_error(errcode_ret, CL_SUCCESS);
   return new kernel(*prog, name);
}

PUBLIC cl_int
clCreateKernelsInProgram(cl_program prog, cl_uint count,
                         cl_kernel *kerns, cl_uint *count_ret) {
   if (count_ret)
      *count_ret = 0;
   return CL_SUCCESS;
}

PUBLIC cl_int
clRetainKernel(cl_kernel kern) {
   kern->retain();
   return CL_SUCCESS;
}

PUBLIC cl_int
clReleaseKernel(cl_kernel kern) {
   if (kern->release())
      delete kern;
   return CL_SUCCESS;
}

PUBLIC cl_int
clSetKernelArg(cl_kernel kern, cl_uint idx, size_t size,
               const void *value) {
   kern->args[idx]->set(size, value);
   return CL_SUCCESS;
}

PUBLIC cl_int
clGetKernelInfo(cl_kernel kern, cl_kernel_info param,
                size_t size, void *buf, size_t *size_ret) {
   switch (param) {
   case CL_KERNEL_FUNCTION_NAME:
      return string_property(buf, size, size_ret, kern->name());

   case CL_KERNEL_NUM_ARGS:
      return scalar_property<cl_uint>(buf, size, size_ret,
                                      kern->args.size());

   case CL_KERNEL_REFERENCE_COUNT:
      return scalar_property<cl_uint>(buf, size, size_ret,
                                      kern->ref_count());

   case CL_KERNEL_CONTEXT:
      return scalar_property<cl_context>(buf, size, size_ret,
                                         &kern->prog.ctx);

   case CL_KERNEL_PROGRAM:
      return scalar_property<cl_program>(buf, size, size_ret,
                                         &kern->prog);

   default:
      return CL_INVALID_VALUE;
   }
}

PUBLIC cl_int
clGetKernelWorkGroupInfo(cl_kernel kern, cl_device_id dev,
                         cl_kernel_work_group_info param,
                         size_t size, void *buf, size_t *size_ret) {
   switch (param) {
   case CL_KERNEL_WORK_GROUP_SIZE:
      return scalar_property<size_t>(buf, size, size_ret,
                                     kern->max_block_size());

   case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
      return vector_property<size_t>(buf, size, size_ret,
                                     kern->block_size());

   case CL_KERNEL_LOCAL_MEM_SIZE:
      return scalar_property<cl_ulong>(buf, size, size_ret,
                                       kern->mem_local());

   case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
      return scalar_property<size_t>(buf, size, size_ret, 1);

   case CL_KERNEL_PRIVATE_MEM_SIZE:
      return scalar_property<cl_ulong>(buf, size, size_ret,
                                       kern->mem_private());

   default:
      return CL_INVALID_VALUE;
   }
}

namespace {
   std::function<void (event &)>
   kernel_op(cl_command_queue q, cl_kernel kern,
             const std::vector<size_t> &grid_offset,
             const std::vector<size_t> &grid_size,
             const std::vector<size_t> &block_size) {
      return [=](event &) {
         kern->launch(*q, grid_offset, grid_size, block_size);
      };
   }
}

PUBLIC cl_int
clEnqueueNDRangeKernel(cl_command_queue q, cl_kernel kern,
                       cl_uint dims, const size_t *grid_offset,
                       const size_t *grid_size, const size_t *block_size,
                       cl_uint num_deps, const cl_event *deps,
                       cl_event *ev) {
   hard_event *hev = new hard_event(
      *q, CL_COMMAND_NDRANGE_KERNEL, { deps, deps + num_deps },
      kernel_op(q, kern, { grid_offset, grid_offset + dims },
                map(std::divides<size_t>(), grid_size, grid_size + dims,
                    block_size),
                { block_size, block_size + dims }));

   set_object(ev, hev);
   return CL_SUCCESS;
}

PUBLIC cl_int
clEnqueueTask(cl_command_queue q, cl_kernel kern,
              cl_uint num_deps, const cl_event *deps,
              cl_event *ev) {
   hard_event *hev = new hard_event(
      *q, CL_COMMAND_TASK, { deps, deps + num_deps },
      kernel_op(q, kern, { 0 }, { 1 }, { 1 }));

   set_object(ev, hev);
   return CL_SUCCESS;
}

PUBLIC cl_int
clEnqueueNativeKernel(cl_command_queue q, void (*func)(void *),
                      void *args, size_t args_size,
                      cl_uint obj_count, const cl_mem *obj_list,
                      const void **obj_args, cl_uint num_deps,
                      const cl_event *deps, cl_event *ev) {
   return CL_INVALID_OPERATION;
}
