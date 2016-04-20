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
#include "lowlevellock.h"
#include "sysdep.h"
#include <hurd/signal.h>

mach_port_t pthread_kport_np (pthread_t th)
{
  struct pthread *pt = (struct pthread *)th;
  return (INVALID_P (pt) ? MACH_PORT_NULL : __pthread_kport (pt));
}

struct hurd_sigstate* pthread_sigstate_np (pthread_t th)
{
  struct pthread *pt = (struct pthread *)th;
  return (INVALID_P (pt) ? NULL : __pthread_sigstate (pt));
}

/* 'pthread_foreach_np' is not itself a cancellation point, but because
 * it executes a user-provided callback, it may turn into one. As such,
 * we need a cleanup handler to release the running threads lock in case
 * it's cancelled. */

static void
cleanup (void *argp)
{
  lll_unlock (argp, 0);
}

int pthread_foreach_np (int (*fct) (pthread_t, void *), void *argp)
{
  struct hurd_list *runp;
  int ret = 0;

  lll_lock (&__running_threads_lock, 0);
  pthread_cleanup_push (cleanup, &__running_threads_lock);
  int prev = __pthread_cancelpoint_begin ();

  hurd_list_each (&__running_threads, runp)
    {
      struct pthread *pt = hurd_list_entry (runp, struct pthread, link);
      ret = fct ((pthread_t)pt, argp);
      if (ret < 0)
        break;
    }

  __pthread_cancelpoint_end (prev);
  pthread_cleanup_pop (1);
  return (ret);
}

int pthread_gettid_np (pthread_t th, unsigned int *outp)
{
  struct pthread *pt = (struct pthread *)th;
  if (INVALID_P (pt))
    return (ESRCH);

  *outp = pt->id;
  return (0);
}

int pthread_suspend_np (pthread_t th)
{
  mach_port_t kport = pthread_kport_np (th);
  if (kport == MACH_PORT_NULL)
    return (ESRCH);
  else if (kport == __pthread_kport (PTHREAD_SELF) ||
      thread_suspend (kport) != 0)
    return (EINVAL);
  else if (thread_abort (kport) != 0)
    {
      /* Don't leave the thread suspended in this case. */
      thread_resume (kport);
      return (EINVAL);
    }

  return (0);
}

int pthread_resume_np (pthread_t th)
{
  mach_port_t kport = pthread_kport_np (th);
  if (kport == MACH_PORT_NULL ||
      thread_resume (kport) != 0)
    return (ESRCH);

  return (0);
}

