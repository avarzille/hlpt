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

#include "lowlevellock.h"
#include "gsync.h"
#include "../sysdeps/atomic.h"
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <mach.h>

int lll_wait (void *ptr, int val, int flags)
{
  return (gsync_wait (mach_task_self (),
    (vm_offset_t)ptr, val, 0, 0, flags));
}

int lll_xwait (void *ptr, int lo, int hi, int flags)
{
  return (gsync_wait (mach_task_self (),
    (vm_offset_t)ptr, lo, hi, 0, flags | GSYNC_QUAD));
}

int lll_timed_wait (void *ptr, int val, int mlsec, int flags)
{
  return (gsync_wait (mach_task_self (),
    (vm_offset_t)ptr, val, 0, mlsec, flags | GSYNC_TIMED));
}

int lll_timed_xwait (void *ptr, int lo,
  int hi, int mlsec, int flags)
{
  return (gsync_wait (mach_task_self (), (vm_offset_t)ptr,
    lo, hi, mlsec, flags | GSYNC_TIMED | GSYNC_QUAD));
}

/* Convert an absolute timeout (in nanoseconds) to a relative
 * timeout in milliseconds. */
static inline int __attribute__ ((gnu_inline))
compute_reltime (const struct timespec *abstime, clockid_t clk)
{
  struct timespec ts;
  clock_gettime (clk, &ts);

  ts.tv_sec = abstime->tv_sec - ts.tv_sec;
  ts.tv_nsec = abstime->tv_nsec - ts.tv_nsec;

  if (ts.tv_nsec < 0)
    {
      --ts.tv_sec;
      ts.tv_nsec += 1000000000;
    }

  return (ts.tv_sec < 0 ? -1 :
    (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
}

int __lll_abstimed_wait (void *ptr, int val,
  const struct timespec *tsp, int flags, int clk)
{
  int mlsec = compute_reltime (tsp, clk);
  return (mlsec < 0 ? KERN_TIMEDOUT :
    lll_timed_wait (ptr, val, mlsec, flags));
}

int __lll_abstimed_xwait (void *ptr, int lo, int hi,
  const struct timespec *tsp, int flags, int clk)
{
  int mlsec = compute_reltime (tsp, clk);
  return (mlsec < 0 ? KERN_TIMEDOUT :
    lll_timed_xwait (ptr, lo, hi, mlsec, flags));
}

int lll_lock (void *ptr, int flags)
{
  int *iptr = (int *)ptr;
  if (*iptr == 0 && atomic_cas_bool (iptr, 0, 1))
    return (0);

  while (1)
    {
      if (atomic_swap (iptr, 2) == 0)
        return (0);

      lll_wait (iptr, 2, flags);
    }
}

int __lll_abstimed_lock (void *ptr,
  const struct timespec *tsp, int flags, int clk)
{
  int *iptr = (int *)ptr;
  if (*iptr == 0 && atomic_cas_bool (iptr, 0, 1))
    return (0);

  while (1)
    {
      if (atomic_swap (iptr, 2) == 0)
        return (0);
      else if (tsp->tv_nsec < 0 || tsp->tv_nsec >= 1000000000)
        return (EINVAL);

      int mlsec = compute_reltime (tsp, clk);
      if (mlsec < 0 || lll_timed_wait (iptr,
          2, mlsec, flags) == KERN_TIMEDOUT)
        return (ETIMEDOUT);
    }   
}

int lll_trylock (void *ptr)
{
  int *iptr = (int *)ptr;
  return (*iptr == 0 && 
    atomic_cas_bool (iptr, 0, 1) ? 0 : EBUSY);
}

void lll_wake (void *ptr, int flags)
{
  gsync_wake (mach_task_self (), (vm_offset_t)ptr, 0, flags);
}

void lll_set_wake (void *ptr, int val, int flags)
{
  gsync_wake (mach_task_self (), (vm_offset_t)ptr,
    val, flags | GSYNC_MUTATE);
}

void lll_unlock (void *ptr, int flags)
{
  if (atomic_swap ((int *)ptr, 0) != 1)
    lll_wake (ptr, flags);
}

void lll_requeue (void *src, void *dst, int wake_one, int flags)
{
  gsync_requeue (mach_task_self (), (vm_offset_t)src,
    (vm_offset_t)dst, (boolean_t)wake_one, flags);
}

/* Robust locks. */

extern int getpid (void) __attribute__ ((const));
extern mach_port_t pid2task (int);

/* Test if a given process id is still valid. */
static inline int valid_pid (int pid)
{
  mach_port_t task = pid2task (pid);
  if (task == MACH_PORT_NULL)
    return (0);

  mach_port_deallocate (mach_task_self (), task);
  return (1);
}

/* Robust locks have currently no support from the kernel; they
 * are simply implemented with periodic polling. When sleeping, the
 * maximum block time is determined by this constant. */
#define MAX_WAIT_TIME   1500

/* Define the additional errno code for robust locks. */
#ifndef EOWNERDEAD
  #define EOWNERDEAD        1073741945
#endif

int lll_robust_lock (void *ptr, int flags)
{
  int *iptr = (int *)ptr;
  int id = getpid ();
  int wait_time = 25;
  unsigned int val;

  while (1)
    {
      val = *iptr;
      if (val == 0 && atomic_cas_bool (iptr, 0, id))
        return (0);
      else if (val != 0 && atomic_cas_bool (iptr, val, val | LLL_WAITERS))
        break;
    }

  for (id |= LLL_WAITERS ; ; )
    {
      val = *iptr;
      if (val == 0 && atomic_cas_bool (iptr, 0, id))
        return (0);
      else if (val && !valid_pid (val & LLL_OWNER_MASK))
        {
          if (atomic_cas_bool (iptr, val, id))
            return (EOWNERDEAD);
        }
      else
        {
          lll_timed_wait (iptr, val, wait_time, flags);
          if (wait_time < MAX_WAIT_TIME)
            wait_time <<= 1;
        }
    }
}

int __lll_robust_abstimed_lock (void *ptr,
  const struct timespec *tsp, int flags, int clk)
{
  int *iptr = (int *)ptr;
  int id = getpid ();
  int wait_time = 25;
  unsigned int val;

  while (1)
    {
      val = *iptr;
      if (val == 0 && atomic_cas_bool (iptr, 0, id))
        return (0);
      else if (val != 0 && atomic_cas_bool (iptr, val, val | LLL_WAITERS))
        break;
    }

  for (id |= LLL_WAITERS ; ; )
    {
      val = *iptr;
      if (val == 0 && atomic_cas_bool (iptr, val, id))
        return (0);
      else if (val != 0 && !valid_pid (val & LLL_OWNER_MASK))
        {
          if (atomic_cas_bool (iptr, val, id))
            return (EOWNERDEAD);
        }
      else
        {
          int mlsec = compute_reltime (tsp, clk);
          if (mlsec < 0)
            return (ETIMEDOUT);
          else if (mlsec > wait_time)
            mlsec = wait_time;

          int res = lll_timed_wait (iptr, val, mlsec, flags);
          if (res == KERN_TIMEDOUT)
            return (ETIMEDOUT);
          else if (wait_time < MAX_WAIT_TIME)
            wait_time <<= 1;
        }
    }
}

int lll_robust_trylock (void *ptr)
{
  int *iptr = (int *)ptr;
  int id = getpid ();
  unsigned int val = *iptr;

  if (val == 0)
    {
      if (atomic_cas_bool (iptr, val, id))
        return (0);
    }
  else if (!valid_pid (val & LLL_OWNER_MASK) &&
      atomic_cas_bool (iptr, val, id))
    return (EOWNERDEAD);

  return (EBUSY);
}

void lll_robust_unlock (void *ptr, int flags)
{
  while (1)
    {
      unsigned int val = *(unsigned int *)ptr;
      if (val & LLL_WAITERS)
        {
          lll_set_wake (ptr, 0, flags);
          break;
        }
      else if (atomic_cas_bool ((int *)ptr, val, 0))
        break;
    }
}

