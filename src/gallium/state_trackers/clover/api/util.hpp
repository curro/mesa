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

#ifndef __CL_UTIL_HPP__
#define __CL_UTIL_HPP__

#include <cstdint>
#include <cstring>
#include <algorithm>

#include "core/base.hpp"
#include "pipe/p_compiler.h"

namespace clover {
   template<typename T, typename V>
   static inline cl_int
   matrix_property(void *buf, size_t size, size_t *size_ret, const V& v) {
      if (buf && size < sizeof(T *) * v.size())
         return CL_INVALID_VALUE;

      if (size_ret)
         *size_ret = sizeof(T *) * v.size();

      if (buf)
         for_each([](typename V::value_type src, T *dst) {
               if (dst)
                  std::copy(src.begin(), src.end(), dst);
            },
            v.begin(), v.end(), (T **)buf);

      return CL_SUCCESS;
   }

   template<typename T, typename V>
   static inline cl_int
   vector_property(void *buf, size_t size, size_t *size_ret, const V& v) {
      if (buf && size < sizeof(T) * v.size())
         return CL_INVALID_VALUE;

      if (size_ret)
         *size_ret = sizeof(T) * v.size();
      if (buf)
         std::copy(v.begin(), v.end(), (T *)buf);

      return CL_SUCCESS;
   }

   template<typename T>
   static inline cl_int
   scalar_property(void *buf, size_t size, size_t *size_ret, T v) {
      return vector_property<T>(buf, size, size_ret, std::vector<T>(1, v));
   }

   static inline cl_int
   string_property(void *buf, size_t size, size_t *size_ret,
                   const std::string &v) {
      if (buf && size < v.size() + 1)
         return CL_INVALID_VALUE;

      if (size_ret)
         *size_ret = v.size() + 1;
      if (buf)
         std::strcpy((char *)buf, v.c_str());

      return CL_SUCCESS;
   }

   template<typename T>
   class delim_vector {
   public:
      delim_vector(T *p, T delim = T()) : __begin(p) {
         for (__end = p; __end && *__end != delim; ++__end);
      }

      T *begin() const {
         return __begin;
      }

      T *end() const {
         return __end;
      }

      template<typename sequence_type>
      operator sequence_type() const {
         return sequence_type(begin(), end());
      }

   private:
      T *__begin;
      T *__end;
   };

   static inline void
   set_error(cl_int *p, const clover::error &e) {
      if (p)
         *p = e.get();
   }

   template<typename T, typename S>
   static inline void
   set_object(T p, S v) {
      if (p)
         *p = v;
      else
         v->release();
   }
}

#endif
