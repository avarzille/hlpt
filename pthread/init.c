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

#include "pt-internal.h"
#include "sysdep.h"
#include <hurd/signal.h>
#include <mach/mig_support.h>
#include <sys/resource.h>
#include <sched.h>

/* We have these global variables zero-initialized and then
 * manually construct them in the function below. This makes things
 * easier for the dynamic linker. */
struct hurd_list __running_threads;
unsigned int __running_threads_lock;
unsigned int __pthread_id_counter;
unsigned int __pthread_total;
struct pthread_key_data __pthread_keys[PTHREAD_KEYS_MAX];
pthread_attr_t __pthread_dfl_attr;
int __pthread_concurrency;
int __pthread_mtflag;

/* Descriptor for the main thread. */
static struct pthread __main_thread;

void  __attribute__ ((constructor)) __pthread_initialize (void)
{
  struct pthread *pt = &__main_thread;
  hurd_list_init (&__running_threads);
  hurd_list_add_head (&__running_threads, &pt->link);

  /* The main thread has a fixed ID of one. */
  pt->id = __pthread_id_counter = 1;

  /* We have one running thread now. */
  __pthread_total = 1;

  /* This reference is needed for the main thread in case it
   * exits. Otherwise, the simpleroutine 'terminate_release2'
   * fails with an invalid destination error code. XXX */
  mach_port_t self = mach_thread_self ();

  /* The TCB for the main thread is already set up, 
   * so link the thread descriptor to it. */
  pt->tcb = GET_TCB ();
  SETUP_TCB (pt->tcb, pt, self);
  __pthread_kport(pt) = self;

  /* The main thread's stack may not be deallocated by hand,
   * and is also special w.r.t its TLS area. */
  pt->flags |= PTHREAD_FLG_USR_STACK | PTHREAD_FLG_MAIN_THREAD;

  /* Initialize the pthread default attributes. */
  struct rlimit lim;
  if (getrlimit (RLIMIT_STACK, &lim) < 0 || lim.rlim_cur == RLIM_INFINITY)
    /* Either an error, or the stack size is unbounded. Use a suitably
     * large value as the default. */
    lim.rlim_cur = 2 * 1024 * 1024;   /* 2MB should be fine. */
  else if (lim.rlim_cur < PTHREAD_STACK_MIN)
    /* Make sure the default stack size is at least the minimum required. */
    lim.rlim_cur = PTHREAD_STACK_MIN;

  __pthread_dfl_attr.__stacksize = lim.rlim_cur;
  __pthread_dfl_attr.__guardsize = 0;
  __pthread_dfl_attr.__flags = 
    PTHREAD_CREATE_JOINABLE | PTHREAD_INHERIT_SCHED;
  __pthread_dfl_attr.__sched.__policy = SCHED_OTHER;

  /* XXX: Computing the stack size for the main thread is pointless. It can
   * be extended indefinitely during runtime. So we'll just settle for
   * the default value for now. Getting a somewhat usable stack pointer
   * is more important. */
  extern void *__libc_stack_end;
  pt->stack = (char *)__libc_stack_end - (pt->stacksize = lim.rlim_cur);

  /* Initialize MIG with thread support. */
  mig_init ((void *)1);
}

