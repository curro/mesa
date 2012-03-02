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

#ifndef __CORE_PROGRAM_HPP__
#define __CORE_PROGRAM_HPP__

#include <map>

#include <llvm/Module.h>
#include <llvm/Function.h>

#include "core/base.hpp"
#include "core/context.hpp"
#include "llvm/tgsi_object.h"

namespace clover {
   typedef struct _cl_program program;

   struct module {
      module(llvm::Module * mod);
   
      struct section : public tgsi_section {
         const char *ptr;
      };
   
      struct symbol : public tgsi_symbol {
         std::vector<tgsi_argument> args;
         std::string name;
      };
   
      llvm::Module * llvm_module;

      std::string binary;
      std::map<uint32_t, section> secs;
      std::map<std::string, symbol> syms;
   };
}

struct _cl_program : public clover::ref_counter {
public:
   _cl_program(clover::context &ctx,
               const std::string &source);
   _cl_program(clover::context &ctx,
               const std::vector<clover::device *> &devs,
               const std::vector<std::string> &binaries);

   void build(const std::vector<clover::device *> &devs);

   const std::string &source() const;
#ifdef TGSI_SOURCE
   const std::map<clover::device *, std::string> &binaries() const;
#else
   const std::map<clover::device *, clover::module> &binaries() const;
#endif

#ifndef TGSI_SOURCE
	 std::map<std::string, llvm::Function*> kernel_functions(const clover::module& m) const;
#endif

   std::vector<std::string> kernel_functions_names() const;
   
   cl_build_status build_status(clover::device *dev) const;
   std::string build_opts(clover::device *dev) const;
   std::string build_log(clover::device *dev) const;

   clover::context &ctx;

private:
#ifdef TGSI_SOURCE
   std::map<clover::device *, std::string> __binaries;
#else
   std::map<clover::device *, clover::module> __modules;
#endif
   std::string __source;
};

#endif
