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
#include "../sysdeps/atomic.h"
#include "lowlevellock.h"
#include <hurd/signal.h>
#include "sysdep.h"
#include <errno.h>

int pthread_setcancelstate (int state, int *prevp)
{
  int dummy;
  struct pthread *self = PTHREAD_SELF;

  if (state != PTHREAD_CANCEL_ENABLE &&
      state != PTHREAD_CANCEL_DISABLE)
    return (EINVAL);
  else if (prevp == NULL)
    prevp = &dummy;

  while (1)
    {
      int oldval = self->flags;
      int newval = state == PTHREAD_CANCEL_DISABLE ?
        (oldval | PT_FLG_CANCEL_DISABLE) :
        (oldval & ~PT_FLG_CANCEL_DISABLE);

      *prevp = (oldval & PT_FLG_CANCEL_DISABLE) ?
        PTHREAD_CANCEL_DISABLE : PTHREAD_CANCEL_ENABLE;

      /* Try to avoid expensive atomic operations if there's
       * nothing more to do. */
      if (oldval == newval)
        break;
      else if (atomic_cas_bool (&self->flags, oldval, newval))
        {
          /* If this change triggered a cancellation, exit now. */
          if (CANCELLED_ENABLED_ASYNC_P (newval))
            pthread_exit (PTHREAD_CANCELED);
          break;
        }
    }

  return (0);
}

int pthread_setcanceltype (int type, int *prevp)
{
  int dummy;
  struct pthread *self = PTHREAD_SELF;

  if (type != PTHREAD_CANCEL_DEFERRED &&
      type != PTHREAD_CANCEL_ASYNCHRONOUS)
    return (EINVAL);
  else if (prevp == NULL)
    prevp = &dummy;

  while (1)
    {
      int oldval = self->flags;
      int newval = type == PTHREAD_CANCEL_ASYNCHRONOUS ?
        (oldval | PT_FLG_CANCEL_ASYNC) :
        (oldval & ~PT_FLG_CANCEL_ASYNC);

      *prevp = (oldval & PT_FLG_CANCEL_ASYNC) ?
        PTHREAD_CANCEL_ASYNCHRONOUS : PTHREAD_CANCEL_DEFERRED;

      if (oldval == newval)
        break;
      else if (atomic_cas_bool (&self->flags, oldval, newval))
        {
          if (CANCELLED_ENABLED_ASYNC_P (newval))
            pthread_exit (PTHREAD_CANCELED);
          break;
        }
    }

  return (0);
}

static void
exit_thread (void)
{
  atomic_or (&PTHREAD_SELF->flags, PT_FLG_CANCELLED);
  pthread_exit (PTHREAD_CANCELED);
}

extern int interrupt_operation (mach_port_t, mach_msg_timeout_t);
extern mach_msg_return_t _hurd_intr_rpc_mach_msg (mach_msg_header_t *,
  mach_msg_option_t, mach_msg_size_t, mach_msg_size_t,
  mach_port_t, mach_msg_timeout_t, mach_port_t);

static inline int
in_cancelpoint_p (machine_state_t *statep, struct pthread *pt)
{
  struct hurd_sigstate *stp = __pthread_sigstate (pt);

  /* The interrupt port is set for the signal state just as it's about
   * to enter the trap, inside '_hurd_intr_rpc_mach_msg'. As such, we
   * can deduce that a thread is in a cancellation point (i.e: inside
   * the trap itself) when its interrupt port is non-null, but the
   * thread PC is *not* '_hurd_intr_rpc_mach_msg'. */
  if (stp->intr_port != MACH_PORT_NULL &&
      machine_state_pc(*statep) != (unsigned long)_hurd_intr_rpc_mach_msg)
    {
      /* If the server supports it, notify it that the thread is
       * leaving and should abort any RPC on its behalf. */
      if (stp->intr_port != MACH_PORT_DEAD)
        interrupt_operation (stp->intr_port, 100);
      return (1);
    }

  return (0);
}

int pthread_cancel (pthread_t th)
{
  struct pthread *pt = (struct pthread *)th;

  if (INVALID_P (pt))
    return (ESRCH);

  int ret = 0;

  while (1)
    {
      int oldval = pt->flags;
      int newval = oldval | PT_FLG_CANCELLING | PT_FLG_CANCELLED;

      if (oldval == newval)
        break;
      else if (CANCELLED_ENABLED_ASYNC_P (newval))
        {
          /* If we succeed here, we would cause an async cancellation. */
          if (!atomic_cas_bool (&pt->flags,
              oldval, oldval | PT_FLG_CANCELLING))
            continue;
          else if (pt == PTHREAD_SELF)
            pthread_exit (PTHREAD_CANCELED);
          else
            {
              mach_port_t ktid = __pthread_kport (pt);

              if (thread_suspend (ktid) != 0)
                ret = ESRCH;
              else
                {
                  machine_state_t state;

                  ret |= thread_abort (ktid);
                  ret |= machine_state_get (ktid, &state);

                  /* Mutate the thread state and have it execute a routine
                   * that calls 'pthread_exit'. For transitioning threads,
                   * make sure that it's actually inside a cancellation
                   * point, and not just after one. */
                  if ((oldval & PT_FLG_CANCEL_TRANS) == 0 ||
                     in_cancelpoint_p (&state, pt))
                    {
                      machine_state_pc(state) = (unsigned long)exit_thread;
                      ret |= machine_state_set (ktid, &state);
                    }

                  ret |= thread_resume (ktid);
                  if (ret != 0)
                    ret = ESRCH;
                }

              break;
            }
        }
      else if (atomic_cas_bool (&pt->flags, oldval, newval))
        break;
    }

  /* A single threaded process that has issued a cancellation
   * request also has to do the additional checks when executing
   * a cancellation point. */
  if (ret == 0)
    atomic_store (&__pthread_mtflag, 1);

  return (ret);
}

void pthread_testcancel (void)
{
  if (CANCELLED_ENABLED_P (PTHREAD_SELF->flags))
    pthread_exit (PTHREAD_CANCELED);
}

int __pthread_cancelpoint_begin (void)
{
  struct pthread *self = PTHREAD_SELF;
  int oldval = self->flags;

  if (oldval & (PT_FLG_CANCEL_ASYNC | PT_FLG_CANCEL_DISABLE))
    /* For cancellation points, we're only interested in the transition
     * from deferred to asynchronous mode. As such, if cancellation
     * was disabled, or already async, we don't do anything. */
    return (0);

  __pthread_sigstate(self)->intr_port = MACH_PORT_DEAD;

  while (1)
    {
      oldval = self->flags;
      /* Tell other threads that we're about to transition into
       * executing a cancellation point. */
      int newval = oldval | PT_FLG_CANCEL_ASYNC | PT_FLG_CANCEL_TRANS;

      if (oldval == newval)
        break;
      else if (atomic_cas_bool (&self->flags, oldval, newval))
        {
          if (CANCELLED_ENABLED_ASYNC_P (newval))
            pthread_exit (PTHREAD_CANCELED);

          break;
        }
    }

  return (1);
}

void __pthread_cancelpoint_end (int prev)
{
  struct pthread *self = PTHREAD_SELF;

  if (prev == 0)
    /* Same as above: Don't bother doing anything in this case. */
    return;

  /* Note that we don't care if we were cancelled. If the cancelling
   * thread succeeded, it would have forced us to execute the exiting
   * routine, and if we get here, then the cancellation type was deferred,
   * in which case the request will be handled in the next chance. */
  atomic_and (&self->flags, ~(PT_FLG_CANCEL_ASYNC | PT_FLG_CANCEL_TRANS));
}

