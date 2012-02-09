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
#include "core/program.hpp"

using namespace clover;

PUBLIC cl_program
clCreateProgramWithSource(cl_context ctx, cl_uint count,
                          const char **strings, const size_t *lengths,
                          cl_int *errcode_ret) {
   std::string source;

   for (unsigned i = 0; i < count; ++i)
         source += (lengths && lengths[i] ?
                    std::string(strings[i], strings[i] + lengths[i]) :
                    std::string(strings[i]));

   set_error(errcode_ret, CL_SUCCESS);
   return new program(*ctx, source);
}

PUBLIC cl_program
clCreateProgramWithBinary(cl_context ctx, cl_uint count,
                          const cl_device_id *devs, const size_t *lengths,
                          const unsigned char **binaries, cl_int *statuses,
                          cl_int *errcode_ret) {
   set_error(errcode_ret, CL_SUCCESS);
   std::fill_n(statuses, count, CL_SUCCESS);

   return new program(*ctx,
                      std::vector<cl_device_id>(devs, devs + count),
                      map([](const unsigned char *p, size_t l) {
                            return std::string(p, p + l);
                         },
                         binaries, binaries + count, lengths));
}

PUBLIC cl_int
clRetainProgram(cl_program prog) {
   prog->retain();
   return CL_SUCCESS;
}

PUBLIC cl_int
clReleaseProgram(cl_program prog) {
   if (prog->release())
      delete prog;
   return CL_SUCCESS;
}

PUBLIC cl_int
clBuildProgram(cl_program prog, cl_uint count, const cl_device_id *devs,
               const char *opts, void (*pfn_notify)(cl_program, void *),
               void *user_data) {
   prog->build(std::vector<cl_device_id>(devs, devs + count));
   return CL_SUCCESS;
}

PUBLIC cl_int
clUnloadCompiler() {
   return CL_SUCCESS;
}

PUBLIC cl_int
clGetProgramInfo(cl_program prog, cl_program_info param,
                 size_t size, void *buf, size_t *size_ret) {
   switch (param) {
   case CL_PROGRAM_REFERENCE_COUNT:
      return scalar_property<cl_uint>(buf, size, size_ret,
                                      prog->ref_count());

   case CL_PROGRAM_CONTEXT:
      return scalar_property<cl_context>(buf, size, size_ret,
                                         &prog->ctx);

   case CL_PROGRAM_NUM_DEVICES:
      return scalar_property<cl_uint>(buf, size, size_ret,
                                      prog->binaries().size());

   case CL_PROGRAM_DEVICES:
      return vector_property<cl_device_id>(
         buf, size, size_ret,
#ifdef TGSI_SOURCE
         map(keys<device *, std::string>,
#else
         map(keys<device *, clover::module>,
#endif
             prog->binaries().begin(), prog->binaries().end()));

   case CL_PROGRAM_SOURCE:
      return string_property(buf, size, size_ret, prog->source());

   case CL_PROGRAM_BINARY_SIZES:
      return vector_property<size_t>(
         buf, size, size_ret,
#ifdef TGSI_SOURCE
         map([](const std::pair<device *, std::string> &ent) {
               return ent.second.size();
#else
         map([](const std::pair<device *, clover::module> &ent) {
               return ent.second.binary.size();
#endif
            },
            prog->binaries().begin(), prog->binaries().end()));

   case CL_PROGRAM_BINARIES:
      return matrix_property<unsigned char>(
         buf, size, size_ret,
#ifdef TGSI_SOURCE
         map(values<device *, std::string>,
#else
         map([](const std::pair<device *, clover::module> &ent) {
               return ent.second.binary;
            },
#endif
            prog->binaries().begin(), prog->binaries().end()));

   default:
      return CL_INVALID_VALUE;
   }
}

PUBLIC cl_int
clGetProgramBuildInfo(cl_program prog, cl_device_id dev,
                      cl_program_build_info param,
                      size_t size, void *buf, size_t *size_ret) {
   switch (param) {
   case CL_PROGRAM_BUILD_STATUS:
      return scalar_property<cl_build_status>(buf, size, size_ret,
                                              prog->build_status(dev));

   case CL_PROGRAM_BUILD_OPTIONS:
      return string_property(buf, size, size_ret, prog->build_opts(dev));

   case CL_PROGRAM_BUILD_LOG:
      return string_property(buf, size, size_ret, prog->build_log(dev));

   default:
      return CL_INVALID_VALUE;
   }
}
