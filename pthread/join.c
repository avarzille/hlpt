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
#include "../sysdeps/atomic.h"
#include "sysdep.h"
#include "lowlevellock.h"
#include <mach/kern_return.h>
#include <errno.h>

static void
cleanup (void *argp)
{
  /* Only swap the joiner thread if we managed to set ourselves. */
  atomic_cas_bool ((void **)argp, PTHREAD_SELF, NULL);
}

int pthread_join (pthread_t th, void **retpp)
{
  struct pthread *pt = (struct pthread *)th;

  /* Avoid invalid and detached threads. */
  if (!pt)
    return (ESRCH);
  else if (DETACHED_P (pt))
    return (EINVAL);

  struct pthread *self = PTHREAD_SELF;
  int ret = 0;
  
  /* Set things up for cancellation handling. */
  pthread_cleanup_push (cleanup, &pt->joinpt);

  if (pt == self || self->joinpt == pt)
    /* A basic test got a deadlock. Report it. */
    ret = EDEADLK;
  else if (pt->joinpt != NULL || !atomic_cas_bool (&pt->joinpt, NULL, self))
    /* Another thread is already waiting for this one to finish. */
    ret = EINVAL;
  else
    {
      /* The kernel will notify this thread's termination by setting
       * its ID field to 0 and doing a 'gsync_wake' on that address. */
      unsigned int id;
      int prev = __pthread_cancelpoint_begin ();

      while ((id = pt->id) != 0)
        lll_wait (&pt->id, id, 0);

      __pthread_cancelpoint_end (prev);
    }

  pthread_cleanup_pop (0);

  if (__glibc_likely (ret == 0))
    {
      /* The thread terminated. Collect the return value and
       * deallocate its resources. */
      if (retpp != NULL)
        *retpp = pt->retval;

      __pthread_deallocate (pt);
    }

  return (ret);
}

int pthread_timedjoin_np (pthread_t th, void **retpp,
  const struct timespec *tsp)
{
  struct pthread *pt = (struct pthread *)th;

  if (!pt)
    return (ESRCH);
  else if (DETACHED_P (pt))
    return (EINVAL);

  struct pthread *self = PTHREAD_SELF;
  int ret = 0;
  
  pthread_cleanup_push (cleanup, &pt->joinpt);

  if (pt == self || self->joinpt == pt)
    ret = EDEADLK;
  else if (pt->joinpt != NULL || !atomic_cas_bool (&pt->joinpt, NULL, self))
    ret = EINVAL;
  else
    {
      unsigned int id;
      int prev = __pthread_cancelpoint_begin ();

      while ((id = pt->id) != 0)
        {
          ret = lll_abstimed_wait (&pt->id, id, tsp, 0);
          if (ret != KERN_INTERRUPTED)
            break;
        }

      __pthread_cancelpoint_end (prev);
      if (ret == KERN_TIMEDOUT)
        ret = ETIMEDOUT;
      else
        ret = 0;

    }

  /* Restore the joiner thread only if we previously modified it
   * before timeout'ing. */
  pthread_cleanup_pop (ret == ETIMEDOUT);

  if (__glibc_likely (ret == 0))
    {
      if (retpp != NULL)
        *retpp = pt->retval;

      __pthread_deallocate (pt);
    }

  return (ret);
}

int pthread_tryjoin_np (pthread_t th, void **retpp)
{
  struct pthread *pt = (struct pthread *)th;

  if (!pt)
    return (ESRCH);
  else if (DETACHED_P (pt))
    return (EINVAL);

  struct pthread *self = PTHREAD_SELF;
  if (pt == self || self->joinpt == pt)
    return (EDEADLK);
  else if (pt->id != 0)
    return (EBUSY);
  else if (pt->joinpt != NULL || !atomic_cas_bool (&pt->joinpt, NULL, self))
    return (EINVAL);
  else if (retpp != NULL)
    *retpp = pt->retval;

  /* At this point, we know the thread has terminated and only
   * we are joining with it. So it's safe to deallocate its resources. */
  __pthread_deallocate (pt);
  return (0);
}

