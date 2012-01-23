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

#ifndef __CORE_BASE_HPP__
#define __CORE_BASE_HPP__

#include <stdexcept>
#include <atomic>
#include <cassert>
#include <tuple>
#include <vector>

#include "CL/cl.h"

namespace clover {
   class error : public std::runtime_error {
   public:
      error(cl_int code, std::string what = "") :
         std::runtime_error(what), code(code) {
      }

      cl_int get() const {
         return code;
      }

   protected:
      cl_int code;
   };

   class ref_counter {
   public:
      ref_counter() : __ref_count(1) {}

      unsigned ref_count() {
         return __ref_count;
      }

      void retain() {
         __ref_count++;
      }

      bool release() {
         return (--__ref_count) == 0;
      }

   private:
      std::atomic<unsigned> __ref_count;
   };

   template<typename T>
   class ref_ptr {
   public:
      ref_ptr(T *q = NULL) : p(NULL) {
         reset(q);
      }

      template<typename S>
      ref_ptr(const ref_ptr<S> &ref) : p(NULL) {
         reset(ref.p);
      }

      ~ref_ptr() {
         reset(NULL);
      }

      void reset(T *q = NULL) {
         if (q)
            q->retain();
         if (p && p->release())
            delete p;
         p = q;
      }

      ref_ptr &operator=(const ref_ptr &ref) {
         reset(ref.p);
         return *this;
      }

      T *operator*() const {
         return p;
      }

      T *operator->() const {
         return p;
      }

      operator bool() const {
         return p;
      }

   private:
      T *p;
   };

   template<typename T>
   static inline ref_ptr<T>
   transfer(T *p) {
      ref_ptr<T> ref { p };
      p->release();
      return ref;
   }

   template<typename T, typename S, int N>
   struct __iter_helper {
      template<typename F, typename Its, typename... Args>
      static T
      step(F op, S state, Its its, Args... args) {
         return __iter_helper<T, S, N - 1>::step(
            op, state, its, *(std::get<N>(its)++), args...);
      }
   };

   template<typename T, typename S>
   struct __iter_helper<T, S, 0> {
      template<typename F, typename Its, typename... Args>
      static T
      step(F op, S state, Its its, Args... args) {
         return op(state, *(std::get<0>(its)++), args...);
      }
   };

   struct __empty {};

   template<typename T>
   struct __iter_helper<T, __empty, 0> {
      template<typename F, typename Its, typename... Args>
      static T
      step(F op, __empty state, Its its, Args... args) {
         return op(*(std::get<0>(its)++), args...);
      }
   };

   template<typename F, typename... Its>
   struct __result_helper {
      typedef typename std::remove_const<
         typename std::result_of<
            F (typename std::iterator_traits<Its>::value_type...)
            >::type
         >::type type;
   };


   template<typename F, typename It0, typename... Its>
   F
   for_each(F op, It0 it0, It0 end0, Its... its) {
      while (it0 != end0)
         __iter_helper<void, __empty, sizeof...(Its)>::step(
            op, {}, std::tie(it0, its...));

      return op;
   }

   template<typename F, typename It0, typename... Its,
            typename C = std::vector<
               typename __result_helper<F, It0, Its...>::type>>
   C
   map(F op, It0 it0, It0 end0, Its... its) {
      C c;

      while (it0 != end0)
         c.push_back(
            __iter_helper<typename C::value_type, __empty, sizeof...(Its)>
            ::step(op, {}, std::tie(it0, its...)));

      return c;
   }

   template<typename F, typename T, typename It0, typename... Its>
   T
   fold(F op, T a, It0 it0, It0 end0, Its... its) {
      while (it0 != end0)
         a = __iter_helper<T, T, sizeof...(Its)>::step(
            op, a, std::tie(it0, its...));

      return a;
   }

   template<typename T, typename S>
   T
   keys(const std::pair<T, S> &ent) {
      return ent.first;
   }

   template<typename T, typename S>
   S
   values(const std::pair<T, S> &ent) {
      return ent.second;
   }
}

#endif
