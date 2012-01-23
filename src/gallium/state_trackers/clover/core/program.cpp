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

#include <algorithm>

#include "core/program.hpp"
#include "pipe/p_shader_tokens.h"
#include "tgsi/tgsi_text.h"
#include "util/u_memory.h"

using namespace clover;

_cl_program::_cl_program(clover::context &ctx,
                         const std::string &source) :
   ctx(ctx), __source(source) {
}

_cl_program::_cl_program(clover::context &ctx,
                         const std::vector<clover::device *> &devs,
                         const std::vector<std::string> &binaries) :
   ctx(ctx) {
   for_each([&](clover::device *dev, const std::string &bin) {
         __binaries.insert({ dev, bin });
      },
      devs.begin(), devs.end(), binaries.begin());
}

void
_cl_program::build(const std::vector<clover::device *> &devs) {
   tgsi_token prog[1024];
   tgsi_header *header = (tgsi_header *)&prog[0];

   __binaries.clear();

   if (!tgsi_text_translate(__source.c_str(), prog, Elements(prog)))
      throw error(CL_BUILD_PROGRAM_FAILURE);

   for (auto dev : devs)
      __binaries.insert({ dev,
            { (char *)prog, (char *)&prog[header->BodySize] } });
}

const std::string &
_cl_program::source() const {
   return __source;
}

const std::map<clover::device *, std::string> &
_cl_program::binaries() const {
   return __binaries;
}

cl_build_status
_cl_program::build_status(clover::device *dev) const {
   return __binaries.count(dev) ? CL_BUILD_SUCCESS : CL_BUILD_NONE;
}

std::string
_cl_program::build_opts(clover::device *dev) const {
   return {};
}

std::string
_cl_program::build_log(clover::device *dev) const {
   return {};
}
