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

#ifndef __PT_INTERNAL_H__
#define __PT_INTERNAL_H__   1

/* XXX: Gross hack to avoid including the standard files. */
#define _PTHREAD_H        1
#define _PTHREADTYPES_H   1

#include "../hurd/list.h"
#include "./pthread.h"
#include <stddef.h>
#include <stdbool.h>
#include <mach/kern_return.h>
#include <resolv.h>

/* Special exception used by libgcc to force stack unwinding. 
 * It's mostly opaque, but the ABI is *very* unlikely to change. */
struct __pthread_unwind_exc
{
  unsigned long long cls;
  void (*cleanup) (int, void *);
  unsigned long priv[2];
} __attribute__ ((__aligned__));

/* Structure that describes an entry into a global array. The 
 * pthread descriptors store pointers to data, while the lib itself
 * stores destructors. */
struct pthread_key_data
{
  unsigned long seq;
  union
    {
      void *data;
      void (*destr) (void *);
    };
};

/* The minimum size of a pthread stack. */
#define PTHREAD_STACK_MIN   16384

/* When executing a key's destructor, it's possible that the function
 * ends up allocating more thread-specific data. In order to prevent
 * infinite loops, the implementation only retries this many times. */
#define PTHREAD_DESTRUCTION_ITERATIONS   4

/* A rather arbitrary limit. */
#define PTHREAD_KEYS_MAX   512

/* When dynamically allocating thread-specific data via pthread keys,
 * we do so in chunks of the following constant. The first of the 
 * blocks is actually statically allocated within the pthread descriptor,
 * so this value shouldn't be too large. */
#define PTHREAD_KEY_L2_SIZE   16

/* Given the above 2 constants, the pointer array used for TSD should
 * have no more than the following entries in order to address the
 * maximum number of keys we allow. */
#define PTHREAD_KEY_L1_SIZE   \
  ((PTHREAD_KEYS_MAX + PTHREAD_KEY_L2_SIZE - 1) / PTHREAD_KEY_L2_SIZE)

/* Thread descriptor type. */
struct pthread
{
  /* Thread control block, keeps a list of thread-local data,
   * and maintains a pointer to this very descriptor, to make
   * 'pthread_self' a very fast operation. */
  void *tcb;

  /* Link in the running threads list. */
  struct hurd_list link;

  /* Chain of cleanup callbacks and their arguments. */
  struct __pthread_cleanup_buf *cleanup;

  /* Unique id used to identify mutex/rwlock ownership. */
  unsigned int id;

  /* Descriptor flags. Includes cancellation state. */
  int flags;

  /* Thread-specific data blocks. */
  struct pthread_key_data specific_blk1[PTHREAD_KEY_L2_SIZE];
  struct pthread_key_data *specific[PTHREAD_KEY_L1_SIZE];

  /* During thread-exit time, this flag is checked in order
   * to know if we have to deallocate TSD. Since most threads don't
   * use TSD at all, this can be considered an optimization. */
  int specific_used;

  /* Joiner pthread. For detached pthreads, this points to itself,
   * since it's otherwise impossible to do so. */
  struct pthread *joinpt;

  /* Thread routine, its argument and return value. */
  void* (*start_fct) (void *);
  void *argp;
  void *retval;

  /* Thread stack and guard. */
  void *stack;
  size_t stacksize;
  size_t guardsize;

  /* Resolver state. */
  struct __res_state res_state;

  /* Special exception used for forced stack unwinding. */
  struct __pthread_unwind_exc exc;
};

/* The kernel port is already stored in the signal state,
 * so fetching it is simple enough. */
#define __pthread_kport(pt)      __pthread_sigstate(pt)->thread

/* When generating thread ids, we reserve a few bits to have a few
 * 'special' values. This still allows us to mantain millions of
 * thread id's before wrapping happens. */
#define PTHREAD_ID_RESERVED_BITS   3
#define PTHREAD_ID_MASK   ((1U << (32 - PTHREAD_ID_RESERVED_BITS)) - 1)

/* One of the few impossible-to-have thread id's. */
#define PTHREAD_INVALID_ID   (~0U)

/* Thread flags. */
#define PT_FLG_USR_STACK        (1U << 0)
#define PT_FLG_EXITING          (1U << 1)
#define PT_FLG_CANCELLED        (1U << 2)
#define PT_FLG_CANCELLING       (1U << 3)
#define PT_FLG_TERMINATED       (1U << 4)
#define PT_FLG_CANCEL_ASYNC     (1U << 5)
#define PT_FLG_CANCEL_DISABLE   (1U << 6)
#define PT_FLG_CANCEL_TRANS     (1U << 7)
#define PT_FLG_MAIN_THREAD      (1U << 8)

/* Test if FLG denotes any cancellation. */
#define CANCELLED_ENABLED_P(flg)   \
  (((flg) & (PT_FLG_CANCEL_DISABLE | PT_FLG_CANCELLED |   \
             PT_FLG_EXITING | PT_FLG_TERMINATED)) ==   \
     PT_FLG_CANCELLED)

/* Test if FLG denotes an asynchronous cancellation. */
#define CANCELLED_ENABLED_ASYNC_P(flg)   \
  (((flg) & (PT_FLG_CANCEL_DISABLE | PT_FLG_CANCEL_ASYNC |   \
             PT_FLG_CANCELLED | PT_FLG_EXITING |   \
             PT_FLG_TERMINATED)) ==   \
     (PT_FLG_CANCEL_ASYNC | PT_FLG_CANCELLED))

/* A pthread is detached if the joiner pthread is itself. */
#define DETACHED_P(pt)   ((pt)->joinpt == (pt))

/* A pthread is invalid if the kernel has signaled its
 * death by clearing the ID field. */
#define INVALID_P(pt)    (!(pt) || (pt)->id == 0)

/* Global variables. */
extern struct hurd_list __running_threads;
extern unsigned int __running_threads_lock;
extern unsigned int __pthread_id_counter;
extern unsigned int __pthread_total;
extern pthread_attr_t __pthread_dfl_attr;
extern struct pthread_key_data __pthread_keys[];
extern int __pthread_concurrency;
extern int __pthread_mtflag;

/* Internal functions. */

/* Deallocate all thread-specific data held by the thread descriptor. */
extern void __pthread_dealloc_tsd (struct pthread *);

/* Deallocate a thread descriptor's stack, if it hasn't already. */
extern void __pthread_deallocate (struct pthread *);

/* Cleanup all that's left of the thread, and ask the kernel
 * to terminate it. */
extern void __pthread_cleanup (struct pthread *);

/* Deallocate every thread stack, except for the calling thread's. */
extern void __pthread_free_stacks (struct pthread *);

/* Switch cancellation type from deferred to asynchronous; start
 * a cancellation point. */
extern int __pthread_cancelpoint_begin (void);

/* Undo the effects done by the previous function. */
extern void __pthread_cancelpoint_end (int);

/* Useful forward declarations. */
extern int getpid (void) __attribute__ ((const));

/* Add to a 64-bit value carefully, so that wrapping at the sub-limb
 * level isn't an issue. */
#define atomic_addx_mem(ptr, lv, hv)   \
  ({   \
     union hurd_xint __tmp;   \
     while (1)   \
       {   \
         __tmp.qv = atomic_loadx (ptr);   \
         if (atomic_casx_bool ((ptr), __tmp.lo, __tmp.hi,   \
             __tmp.lo + (lv), __tmp.hi + (hv)))   \
           break;   \
       }   \
     \
     __tmp.qv;   \
   })

#define atomic_addx_lo(ptr, val)   atomic_addx_mem (ptr, val, 0)
#define atomic_addx_hi(ptr, val)   atomic_addx_mem (ptr, 0, val)

#endif
