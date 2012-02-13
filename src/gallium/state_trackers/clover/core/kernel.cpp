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

#include <sstream>

#include "core/kernel.hpp"
#include "pipe/p_context.h"

#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
#include <llvm/Metadata.h>
#include <llvm/Type.h>

using namespace clover;

_cl_kernel::_cl_kernel(clover::program &prog,
                       const std::string &name) :
   prog(prog), __name(name) {
#ifdef TGSI_SOURCE
   std::istringstream s(name);
   std::string tok;
   unsigned pc;

   if (!(s >> pc))
      throw error(CL_INVALID_KERNEL_NAME);

   if (s >> tok && tok != ":")
      throw error(CL_INVALID_KERNEL_NAME);

   while (s >> tok) {
      if (tok == "scalar")
         args.emplace_back(new scalar_argument);
      else if (tok == "global")
         args.emplace_back(new global_argument);
      else if (tok == "local")
         args.emplace_back(new local_argument);
      else if (tok == "constant")
         args.emplace_back(new constant_argument);
      else if (tok == "image")
         args.emplace_back(new image_argument);
      else if (tok == "sampler")
         args.emplace_back(new sampler_argument);
      else
         throw error(CL_INVALID_KERNEL_NAME);
   }
#else
   const module &m = prog.binaries().begin()->second;
/*
   const module::symbol &sym = m.syms.find(name)->second;
 
   for (auto arg : sym.args) {
      if (arg.kind == TGSI_ARGUMENT_INLINE)
          args.emplace_back(new scalar_argument);
      else if (arg.kind == TGSI_ARGUMENT_CONSTANT)
         args.emplace_back(new constant_argument);
      else if (arg.kind == TGSI_ARGUMENT_GLOBAL)
         args.emplace_back(new global_argument);
      else if (arg.kind == TGSI_ARGUMENT_LOCAL)
         args.emplace_back(new local_argument);
      else if (arg.kind == TGSI_ARGUMENT_RDIMAGE2D ||
               arg.kind == TGSI_ARGUMENT_WRIMAGE2D ||
               arg.kind == TGSI_ARGUMENT_RDIMAGE3D ||
               arg.kind == TGSI_ARGUMENT_WRIMAGE3D)
          args.emplace_back(new image_argument);
      else if (arg.kind == TGSI_ARGUMENT_SAMPLER)
          args.emplace_back(new sampler_argument);
       else
         assert(0);
    }

   pc = sym.offset;
*/
   const llvm::NamedMDNode * kernels =
                        m.llvm_module->getNamedMetadata("opencl.kernels");
   /* XXX: Can we have more than one kernel ? */
   assert(kernels->getNumOperands() == 1);
   const llvm::MDNode * kernel_md = kernels->getOperand(0);
   llvm::Function * kernel_func =
                     llvm::dyn_cast<llvm::Function>(kernel_md->getOperand(0));
   if (!kernel_func) {
      assert(!"Error parsing kernel metadata. XXX: Throw a CL error here.");
   } else {
      for (llvm::Function::arg_iterator I = kernel_func->arg_begin(),
                               E = kernel_func->arg_end(); I != E; ++I) {
         llvm::Argument & arg = *I;
         llvm::Type * arg_type = arg.getType();
         if (arg_type->isPointerTy()) {
            /* XXX: Figure out LLVM->OpenCL address space mappings. */
            unsigned address_space =
                     llvm::cast<llvm::PointerType>(arg_type)->getAddressSpace();
            switch (address_space) {
            default: assert(!"XXX: Unhandled address space\n");
            /* fall through */
            case 0:
               args.emplace_back(new global_argument);
               break;
            }
         } else {
               args.emplace_back(new scalar_argument);
         }
      }
  }
#endif
}

template<typename T, typename V>
static inline std::vector<T>
pad_vector(clover::command_queue &q, const V &v, T x) {
   std::vector<T> w { v.begin(), v.end() };
   w.resize(q.dev.max_block_size().size(), x);
   return w;
}

void
_cl_kernel::launch(clover::command_queue &q,
                   const std::vector<size_t> &grid_offset,
                   const std::vector<size_t> &grid_size,
                   const std::vector<size_t> &block_size) {
   void *st = exec.bind(this, &q);
   auto g_handles = map([&](size_t h) { return (uint32_t *)&exec.input[h]; },
                        exec.g_handles.begin(), exec.g_handles.end());

   q.pipe->bind_compute_state(q.pipe, st);
   q.pipe->bind_compute_sampler_states(q.pipe, exec.samplers.size(),
                                       exec.samplers.data());
   q.pipe->set_compute_sampler_views(q.pipe, exec.resources.size(),
                                     exec.resources.data());
   q.pipe->set_global_binding(q.pipe, exec.g_buffers.size(),
                              exec.g_buffers.data(), g_handles.data());

   q.pipe->launch_grid(q.pipe,
                       pad_vector<uint>(q, block_size, 1).data(),
                       pad_vector<uint>(q, grid_size, 1).data(),
#ifdef TGSI_SOURCE
                       std::stoul(__name),
                       exec.input.data());
#else
                       pc, exec.input.data());
#endif

   q.pipe->set_global_binding(q.pipe, 0, NULL, NULL);
   q.pipe->set_compute_sampler_views(q.pipe, 0, NULL);
   q.pipe->bind_compute_sampler_states(q.pipe, 0, NULL);
   exec.unbind();
}

size_t
_cl_kernel::mem_local() const {
   size_t sz;

   for (auto &arg : args) {
      if (auto larg = dynamic_cast<local_argument *>(arg.get()))
         sz += larg->size();
   }

   return sz;
}

size_t
_cl_kernel::mem_private() const {
   return 0;
}

size_t
_cl_kernel::max_block_size() const {
   return SIZE_MAX;
}

const std::string &
_cl_kernel::name() const {
   return __name;
}

std::vector<size_t>
_cl_kernel::block_size() const {
   return { 0, 0, 0 };
}

_cl_kernel::exec_context::exec_context() :
   kern(kern), q(q), mem_local(0), st(NULL) {
}

_cl_kernel::exec_context::~exec_context() {
   if (st)
      q->pipe->delete_compute_state(q->pipe, st);
}

void *
_cl_kernel::exec_context::bind(clover::kernel *kern1,
                               clover::command_queue *q1) {
   std::swap(kern, kern1);
   std::swap(q, q1);

#ifndef TGSI_SOURCE
   const clover::module & m = kern->prog.binaries().find(&q->dev)->second;
   const module::section &sec = m.secs.find(TGSI_SECTION_TEXT)->second;
#endif

   for (auto &arg : kern->args)
      arg->bind(*this);

   if (!st || q != q1 ||
       cs.req_local_mem != mem_local ||
       cs.req_input_mem != input.size()) {
      if (st)
         q1->pipe->delete_compute_state(q1->pipe, st);
#ifdef TGSI_SOURCE
      cs.tokens = (const struct tgsi_token *)kern->prog.binaries()
         .find(&q->dev)->second.data();
#else
      /*XXX: Modify this so we can pass LLVM. */
//      cs.tokens = (tgsi_token *)sec.ptr;
	cs.shader.ir = m.llvm_module;
#endif
      cs.req_local_mem = mem_local;
      cs.req_input_mem = input.size();
      st = q->pipe->create_compute_state(q->pipe, &cs);
   }

   return st;
}

void
_cl_kernel::exec_context::unbind() {
   for (auto &arg : kern->args)
      arg->unbind(*this);

   input.clear();
   samplers.clear();
   resources.clear();
   g_buffers.clear();
   g_handles.clear();
   mem_local = 0;
}

void
_cl_kernel::scalar_argument::set(size_t size, const void *value) {
   if (size != sizeof(uint32_t))
      throw error(CL_INVALID_ARG_SIZE);

   v = *(uint32_t *)value;
}

void
_cl_kernel::scalar_argument::bind(exec_context &ctx) {
   size_t offset = ctx.input.size();

   ctx.input.resize(offset + sizeof(uint32_t));
   *(uint32_t *)&ctx.input[offset] = v;
}

void
_cl_kernel::scalar_argument::unbind(exec_context &ctx) {
}

void
_cl_kernel::global_argument::set(size_t size, const void *value) {
   if (size != sizeof(cl_mem))
      throw error(CL_INVALID_ARG_SIZE);

   obj = dynamic_cast<clover::buffer *>(*(cl_mem *)value);
}

void
_cl_kernel::global_argument::bind(exec_context &ctx) {
   size_t offset = ctx.input.size();
   size_t idx = ctx.g_buffers.size();

   ctx.input.resize(offset + sizeof(uint32_t));

   ctx.g_buffers.resize(idx + 1);
   ctx.g_buffers[idx] = obj->resource(ctx.q).pipe;

   ctx.g_handles.resize(idx + 1);
   ctx.g_handles[idx] = offset;
}

void
_cl_kernel::global_argument::unbind(exec_context &ctx) {
}

void
_cl_kernel::local_argument::set(size_t size, const void *value) {
   if (value)
      throw error(CL_INVALID_ARG_VALUE);

   __size = size;
}

void
_cl_kernel::local_argument::bind(exec_context &ctx) {
   size_t offset = ctx.input.size();
   size_t ptr = ctx.mem_local;

   ctx.input.resize(offset + sizeof(uint32_t));
   *(uint32_t *)&ctx.input[offset] = ptr;

   ctx.mem_local += __size;
}

void
_cl_kernel::local_argument::unbind(exec_context &ctx) {
}

size_t
_cl_kernel::local_argument::size() const {
   return __size;
}

void
_cl_kernel::constant_argument::set(size_t size, const void *value) {
   if (size != sizeof(cl_mem))
      throw error(CL_INVALID_ARG_SIZE);

   obj = dynamic_cast<clover::buffer *>(*(cl_mem *)value);
}

void
_cl_kernel::constant_argument::bind(exec_context &ctx) {
   size_t offset = ctx.input.size();
   size_t idx = ctx.resources.size();

   ctx.input.resize(offset + sizeof(uint32_t));
   *(uint32_t *)&ctx.input[offset] = idx << 24;

   ctx.resources.resize(idx + 1);
   ctx.resources[idx] = st = obj->resource(ctx.q).bind(*ctx.q, false);
}

void
_cl_kernel::constant_argument::unbind(exec_context &ctx) {
   obj->resource(ctx.q).unbind(*ctx.q, st);
}

void
_cl_kernel::image_argument::set(size_t size, const void *value) {
   if (size != sizeof(cl_mem))
      throw error(CL_INVALID_ARG_SIZE);

   obj = dynamic_cast<clover::image *>(*(cl_mem *)value);
}

void
_cl_kernel::image_argument::bind(exec_context &ctx) {
   size_t offset = ctx.input.size();
   size_t idx = ctx.resources.size();

   ctx.input.resize(offset + sizeof(uint32_t));
   *(uint32_t *)&ctx.input[offset] = idx;

   ctx.resources.resize(idx + 1);
   ctx.resources[idx] = st = obj->resource(ctx.q).bind(*ctx.q, false);
}

void
_cl_kernel::image_argument::unbind(exec_context &ctx) {
   obj->resource(ctx.q).unbind(*ctx.q, st);
}

void
_cl_kernel::sampler_argument::set(size_t size, const void *value) {
   if (size != sizeof(cl_sampler))
      throw error(CL_INVALID_ARG_SIZE);

   obj = *(cl_sampler *)value;
}

void
_cl_kernel::sampler_argument::bind(exec_context &ctx) {
   size_t idx = ctx.samplers.size();

   ctx.samplers.resize(idx + 1);
   ctx.samplers[idx] = st = obj->bind(*ctx.q);
}

void
_cl_kernel::sampler_argument::unbind(exec_context &ctx) {
   obj->unbind(*ctx.q, st);
}
