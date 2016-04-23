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

#define __need_pthread_machine
#include "pt-internal.h"
#include <hurd/signal.h>
#include "sysdep.h"
#include "lowlevellock.h"
#include "ttr.h"
#include "../sysdeps/atomic.h"
#include <mach.h>
#include <mach/mig_support.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>

#define PTHREAD_MPROT   (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)

/* XXX: This really needs to use the internal definitions
 * for the dynamic linker. */

#ifndef internal_function
#  define internal_function   __attribute__ ((regparm (3), stdcall))
#endif

extern void* _dl_allocate_tls (void *) internal_function;
extern void _dl_deallocate_tls (void *, bool) internal_function;

static int
alloc_tls (struct pthread *pt, mach_port_t ktid)
{
  if (!(pt->tcb = _dl_allocate_tls (0)))
    return (-1);

  /* Link the TCB and thread descriptor together. */
  SETUP_TCB (pt->tcb, pt, ktid);
  return (0);
}

static void
dealloc_tls (struct pthread *pt)
{
  _dl_deallocate_tls (pt->tcb,
    !(pt->flags & PTHREAD_FLG_MAIN_THREAD));
  pt->tcb = NULL;
}

#undef internal_function

static inline size_t
roundup_page (size_t size)
{
  return ((size + vm_page_size - 1) & ~(vm_page_size - 1));
}

static inline void
free_stack (struct pthread *pt)
{
  if (pt->flags & PTHREAD_FLG_USR_STACK)
    return;

  size_t total = pt->stacksize + pt->guardsize;
  vm_offset_t addr = (vm_offset_t)pt->stack - pt->guardsize;
  vm_deallocate (mach_task_self (), addr, total);
}

/* XXX: This function assumes the stack always grows down. */
static struct pthread*
pt_allocate (const pthread_attr_t *attrp)
{
  void *stack = attrp->__stack;
  size_t size = attrp->__stacksize ?: __pthread_dfl_attr.__stacksize;
  size_t guardsize = attrp->__guardsize;

  if (__glibc_likely (stack == NULL))
    {
      vm_address_t *outp = (vm_address_t *)&stack;

      /* Round stack size and guard size to a page. */
      size = roundup_page (size);
      if (guardsize != 0)
        guardsize = roundup_page (guardsize);

      size_t total = size + guardsize;

      if (vm_map (mach_task_self (), outp, total, 0,
          1, MEMORY_OBJECT_NULL, 0, 0, VM_PROT_DEFAULT,
          PTHREAD_MPROT, VM_INHERIT_DEFAULT) != 0)
        return (NULL);

      if (guardsize != 0 && vm_protect (mach_task_self (),
          *outp, guardsize, 1, 0) != 0)
        {
          vm_deallocate (mach_task_self (), *outp, total);
          return (NULL);
        }

      /* Set the pointer at the end. */
      *outp += total;
    }

  /* Place the thread descriptor at the top of the stack. */
  struct pthread *pt = (struct pthread *)
    (((unsigned long)stack - sizeof (*pt)) &
      ~(__alignof__ (struct pthread) - 1));

  if (attrp->__stack != 0)
    {
      memset (pt, 0, sizeof (*pt));
      pt->flags |= PTHREAD_FLG_USR_STACK;
    }

  /* Initialize pthread descriptor. */
  pt->stack = (char *)stack - (pt->stacksize = size);
  pt->guardsize = guardsize;

  /* Temporarily set the ID to an invalid value. The thread itself
   * will create a valid one once it begins executing. */
  pt->id = PTHREAD_INVALID_ID;

  pt->specific[0] = pt->specific_blk1;

  if (attrp->__flags & PTHREAD_CREATE_DETACHED)
    pt->joinpt = pt;

  mach_port_t ktid;
  struct hurd_sigstate *stp;

  if (thread_create (mach_task_self (), &ktid) != 0)
    goto fail_kthread;
  else if (!(stp = _hurd_thread_sigstate (ktid)))
    goto fail_sigstate;
  else if (alloc_tls (pt, ktid) < 0)
    goto fail_tls;

  __pthread_sigstate (pt) = stp;
  return (pt);

fail_tls:
  _hurd_sigstate_delete (ktid);

fail_sigstate:
  thread_terminate (ktid);

fail_kthread:
  free_stack (pt);

  return (NULL);
}

static void
thread_entry (struct pthread *pt)
{
  /* Add ourselves to the list of running threads, and
   * create a (sort of) unique id. */
  lll_lock (&__running_threads_lock, 0);
  hurd_list_add_tail (&__running_threads, &pt->link);
  pt->id = ++__pthread_id_counter & PTHREAD_ID_MASK;
  lll_unlock (&__running_threads_lock, 0);

  /* Initialize internal ctype structures. */
  extern void __ctype_init (void);
  __ctype_init ();

  /* Set up thread-local resolver state. */
  extern __thread struct __res_state* __resp
    __attribute__ ((tls_model ("initial-exec")));
  __resp = &pt->res_state;

  /* New threads should use the global locale. */
  uselocale (LC_GLOBAL_LOCALE);

  /* Mark the thread as a possible receiver of global signals. */
  _hurd_sigstate_set_global_rcv (__pthread_sigstate (pt));

  pt->retval = pt->start_fct (pt->argp);
  __pthread_cleanup (pt);
}

void __pthread_cleanup (struct pthread *pt)
{
  int detached_p = DETACHED_P (pt);
  /* Make sure the local variable above is set before
   * marking the thread as in the 'exiting' state. */
  atomic_mfence ();
  atomic_or (&pt->flags, PTHREAD_FLG_EXITING);

  lll_lock (&__running_threads_lock, 0);
  hurd_list_del (&pt->link);
  lll_unlock (&__running_threads_lock, 0);

  /* Call destructors for thread-local variables. */
  extern void __call_tls_dtors (void);
  __call_tls_dtors ();

  /* Finalize any libc state in thread-local variables. */
  extern void __libc_thread_freeres (void);
  __libc_thread_freeres ();

  /* Free thread-specific data. */
  __pthread_dealloc_tsd (pt);

  /* If this is the last thread, we can just end the task
   * abruptly. The kernel will handle the cleanup. */
  if (atomic_add (&__pthread_total, -1) == 1)
    exit (0);

  /* We unconditionally destroy the kernel thread and reply port,
   * but only free the stack if the pthread is detached and if
   * the stack wasn't supplied by the user. */
  mach_port_t ktid = __pthread_kport (pt);
  mach_port_t rport = __mig_get_reply_port ();
  vm_address_t stack = 0;
  size_t size = 0;
  vm_address_t death_ev = 0;

  _hurd_sigstate_delete (ktid);
  __pthread_sigstate(pt) = NULL;

  if (!detached_p)
    /* If the thread is joinable, ask the kernel to
     * signal us once it's terminated. */
    death_ev = (vm_address_t)&pt->id;
  else if (!(pt->flags & PTHREAD_FLG_USR_STACK))
    {
      size = pt->stacksize + pt->guardsize;
      stack = (vm_address_t)pt->stack - pt->guardsize;
    }

  dealloc_tls (pt);
  thread_terminate_release2 (ktid, mach_task_self (),
    ktid, rport, stack, size, death_ev);
}

int pthread_create (pthread_t *ptp, const pthread_attr_t *attrp,
  void* (*start_fct) (void *), void *argp)
{
  if (!attrp)
    attrp = &__pthread_dfl_attr;

  struct pthread *pt = pt_allocate (attrp);

  if (!pt)
    return (EAGAIN);

  pt->start_fct = start_fct;
  pt->argp = argp;

  /* Copy the signal mask from the parent thread, as per POSIX. */
  struct pthread *parent = PTHREAD_SELF;
  if (parent != NULL && __pthread_sigstate (parent) != NULL)
    {
      __spin_lock (&__pthread_sigstate(parent)->lock);
      __pthread_sigstate(pt)->blocked =
        __pthread_sigstate(parent)->blocked;
      __spin_unlock (&__pthread_sigstate(parent)->lock);
    }
  else
    sigemptyset (&__pthread_sigstate(pt)->blocked);

  /* At this point, all that's left is to initialize the machine state
   * for the new thread so that it may execute its entry point. We
   * increment the number of running threads here because the stillborn
   * thread cannot do it on its own (Otherwise, we risk a race condition
   * triggering a premature 'exit'). If we cannot kickstart the new thread,
   * that simply means we lied for a bit, and it's no problem. */

  atomic_add (&__pthread_total, 1);
  if (__pthread_set_machine_state (pt, thread_entry) != 0)
    {
      mach_port_t ktid = __pthread_kport (pt);
      _hurd_sigstate_delete (ktid);
      dealloc_tls (pt);
      thread_terminate (ktid);

      /* The stack is the last thing to clean up. */
      free_stack (pt);
      atomic_add (&__pthread_total, -1);
      return (EAGAIN);
    }

  *ptp = pt;
  /* At this point, we know there's more than one thread. */
  atomic_store (&__pthread_mtflag, 1);

  /* This should never fail. */
  thread_resume (__pthread_kport (pt));
  return (0);
}

void __pthread_deallocate (struct pthread *pt)
{
  if ((atomic_or (&pt->flags, PTHREAD_FLG_TERMINATED) &
      PTHREAD_FLG_TERMINATED) == 0)
    free_stack (pt);
}

void __pthread_free_stacks (struct pthread *self)
{
  struct hurd_list *runp;

  hurd_list_each (&__running_threads, runp)
    {
      struct pthread *pt = hurd_list_entry (runp, struct pthread, link);
      if (pt == self)
        continue;

      dealloc_tls (pt);
      free_stack (pt);
    }
}

