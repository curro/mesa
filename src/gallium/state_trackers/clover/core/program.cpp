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
#ifdef TGSI_SOURCE
#include "pipe/p_shader_tokens.h"
#include "tgsi/tgsi_text.h"
#include "util/u_memory.h"
#else
#include "llvm/invocation.hpp"
#endif

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
#ifdef TGSI_SOURCE
         __binaries.insert({ dev, bin });
#else
         __modules.insert({ dev, { bin } });
#endif
      },
      devs.begin(), devs.end(), binaries.begin());
}

void
_cl_program::build(const std::vector<clover::device *> &devs) {

#ifdef TGSI_SOURCE
   tgsi_token prog[1024];
   tgsi_header *header = (tgsi_header *)&prog[0];

   __binaries.clear();

   if (!tgsi_text_translate(__source.c_str(), prog, Elements(prog)))
      throw error(CL_BUILD_PROGRAM_FAILURE);

   for (auto dev : devs)
      __binaries.insert({ dev,
            { (char *)prog, (char *)&prog[header->BodySize] } });
#else
   size_t prog_sz;
   char * prog;
   compile_program(__source.c_str(), &prog, &prog_sz);

   for (auto dev : devs)
      __modules.insert( { dev, { { prog, prog + prog_sz } } });

   delete prog;
#endif
}

const std::string &
_cl_program::source() const {
   return __source;
}

#ifdef TGSI_SOURCE
const std::map<clover::device *, std::string> &
#else
const std::map<clover::device *, clover::module> &
#endif
_cl_program::binaries() const {
#ifdef TGSI_SOURCE
   return __binaries;
#else
   return __modules;
#endif
}

cl_build_status
_cl_program::build_status(clover::device *dev) const {
#ifdef TGSI_SOURCE
   return __binaries.count(dev) ? CL_BUILD_SUCCESS : CL_BUILD_NONE;
#else
   return __modules.count(dev) ? CL_BUILD_SUCCESS : CL_BUILD_NONE;
#endif
}

std::string
_cl_program::build_opts(clover::device *dev) const {
   return {};
}

std::string
_cl_program::build_log(clover::device *dev) const {
   return {};
}

module::module(const std::string &bin) :
   binary(bin) {
   for (auto it = binary.begin(); it < binary.end();) {
      section sec;

      std::copy_n(it, sizeof(tgsi_section),
                   (char *)static_cast<tgsi_section *>(&sec));
      it += sizeof(tgsi_section);
      sec.ptr = &*it;

      if (sec.kind == TGSI_SECTION_SYMTAB) {
         for (auto end = it + sec.phys_sz; it < end;) {
            symbol sym;

            std::copy_n(it, sizeof(tgsi_symbol),
                        (char *)static_cast<tgsi_symbol *>(&sym));
            it += sizeof(tgsi_symbol);

            for (auto end = it + sym.args_sz; it < end;) {
               tgsi_argument arg;

               std::copy_n(it, sizeof(tgsi_argument), (char *)&arg);
               it += sizeof(tgsi_argument);
               sym.args.push_back(arg);
            }

            sym.name = { it, it + sym.name_sz };
            it += sym.name_sz;

            syms.insert({ sym.name, sym });
         }
      } else {
         it += sec.phys_sz;
      }

      secs.insert({ sec.kind, sec });
   }
}

