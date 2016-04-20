/* Copyright (C) 2016 Free Software Foundation, Inc.
   Contributed by Agustina Arzille <avarzille@riseup.net>, 2016.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either
   version 3 of the license, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, see
   <http://www.gnu.org/licenses/>.
*/

#ifndef __SYSDEPS_ATOMIC_H__
#define __SYSDEPS_ATOMIC_H__   1

/* Wrappers for gcc atomic builtins. */

#ifndef atomic_cas_bool
  /* By default, we should use a strong CAS, since most
   * platforms implement it in terms of LL+SC. */
#  define atomic_cas_bool(ptr, exp, nval)   \
     ({   \
        typeof (exp) __e = (exp);   \
        __atomic_compare_exchange_n ((ptr), &__e, (nval), 0,   \
          __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);   \
      })
#endif

#define atomic_swap(ptr, val)   \
  __atomic_exchange_n ((ptr), (val), __ATOMIC_ACQ_REL)

#define atomic_add(ptr, val)   \
  __atomic_fetch_add ((ptr), (val), __ATOMIC_ACQ_REL)

#define atomic_load(ptr)   \
  __atomic_load_n ((ptr), __ATOMIC_ACQUIRE)

#define atomic_store(ptr, val)   \
  __atomic_store_n ((ptr), (val), __ATOMIC_RELEASE)

#define atomic_or(ptr, val)   \
  __atomic_fetch_or ((ptr), (val), __ATOMIC_ACQ_REL)

#define atomic_and(ptr, val)   \
  __atomic_fetch_and ((ptr), (val), __ATOMIC_ACQ_REL)

#define atomic_mfence_acq()   \
  __atomic_thread_fence (__ATOMIC_ACQUIRE)

#define atomic_mfence_rel()   \
  __atomic_thread_fence (__ATOMIC_RELEASE)

#define atomic_mfence()   \
  __atomic_thread_fence (__ATOMIC_SEQ_CST)

#endif
