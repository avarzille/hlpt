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
#include <hurd/signal.h>
#include "lowlevellock.h"
#include "../sysdeps/atomic.h"
#include "sysdep.h"
#include <errno.h>
#include <time.h>
#include <stddef.h>

/* We use the high 16 bits of the 'flags' integer to store the clock id,
 * and the rest for any synchronization-related flags. */
#define COND_CLK_SHIFT   16
#define COND_CLK_MASK    ((1 << COND_CLK_SHIFT) - 1)

static const pthread_condattr_t dfl_attr =
{
  .__flags = CLOCK_REALTIME << 16
};

int pthread_condattr_init (pthread_condattr_t *attrp)
{
  *attrp = dfl_attr;
  return (0);
}

int pthread_condattr_setpshared (pthread_condattr_t *attrp, int pshared)
{
  if (pshared != PTHREAD_PROCESS_SHARED &&
      pshared != PTHREAD_PROCESS_PRIVATE)
    return (EINVAL);

  attrp->__flags = (attrp->__flags & ~GSYNC_SHARED) |
    (pshared == PTHREAD_PROCESS_SHARED ? GSYNC_SHARED : 0);
  return (0);
}

int pthread_condattr_getpshared (const pthread_condattr_t *attrp, int *outp)
{
  *outp = (attrp->__flags & GSYNC_SHARED) ?
    PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
  return (0);
}

int pthread_condattr_setclock (pthread_condattr_t *attrp, clockid_t clk)
{
  if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
    return (EINVAL);

  attrp->__flags = (clk << COND_CLK_SHIFT) |
    (attrp->__flags & COND_CLK_MASK);
  return (0);
}

int pthread_condattr_getclock (const pthread_condattr_t *attrp, 
  clockid_t *outp)
{
  *outp = attrp->__flags >> COND_CLK_SHIFT;
  return (0);
}

int pthread_condattr_destroy (pthread_condattr_t *attrp)
{
  (void)attrp;
  return (0);
}

int pthread_cond_init (pthread_cond_t *condp,
  const pthread_condattr_t *attrp)
{
  if (attrp == NULL)
    attrp = &dfl_attr;

  condp->__seq_nw.qv = 0;
  condp->__mutex = NULL;
  condp->__flags = attrp->__flags;
  return (0);
}

/* Wait and signal routines. */

/* For mutexes that are being used together with condvars, we want
 * to make sure at some points that a thread is unconditionally
 * woken during the release. As such, we have to set some internal
 * members to trick the mutex into thinking it's contended. */
static inline int
__pthread_mutex_cond_lock (pthread_mutex_t *mtxp)
{
  int ret = pthread_mutex_lock (mtxp);
  if (ret == 0)
    {
      if (!(mtxp->__flags & PTHREAD_MUTEX_ROBUST))
        atomic_store (&mtxp->__lock, 2);
      else if (!(mtxp->__lock & LLL_WAITERS))
        /* This should be faster than an 'atomic_or'. */
        atomic_store (&mtxp->__lock, mtxp->__lock | LLL_WAITERS);
    }

  return (ret);
}

struct cv_cleanup
{
  union hurd_xint sw;
  pthread_cond_t *condp;
  pthread_mutex_t *mtxp;
};

static void
cleanup (void *argp)
{
  struct cv_cleanup *ccp = (struct cv_cleanup *)argp;
  /* Decrement the waiters count, and fetch the wakeup sequence. */
  union hurd_xint tmp = { atomic_addx_hi (&ccp->condp->__seq_nw.qv, -1) };

  if (ccp->sw.lo != tmp.lo)
    /* The wakeup sequence has changed. There's no way
     * to know if we actually consumed a notify call, so
     * we do a broadcast to make sure no signal gets lost. */
    lll_wake (&ccp->condp->__seq_nw.lo,
      ccp->condp->__flags | GSYNC_BROADCAST);

  /* Reaquire the mutex. */
  __pthread_mutex_cond_lock (ccp->mtxp);
}

int pthread_cond_wait (pthread_cond_t *condp, pthread_mutex_t *mtxp)
{
  struct cv_cleanup cc = { .condp = condp, .mtxp = mtxp };
  int flags = condp->__flags & GSYNC_SHARED;

  /* Remember the mutex that is coupled with the condvar, but only
   * if it's a task-local object. */
  if (!(flags & GSYNC_SHARED) && condp->__mutex == NULL)
    atomic_store (&condp->__mutex, mtxp);

  int ret = pthread_mutex_unlock (mtxp);
  if (ret != 0)
    return (ret);

  /* Add ourselves as waiters and register the cancellation handler. */
  cc.sw.qv = atomic_addx_hi (&condp->__seq_nw.qv, 1);
  pthread_cleanup_push (cleanup, &cc);

  do
    {
      /* Switch to async mode before blocking. */
      int prev = __pthread_cancelpoint_begin ();
      ret = lll_wait (&condp->__seq_nw.lo, cc.sw.lo, flags);
      __pthread_cancelpoint_end (prev);

      if (ret == KERN_INTERRUPTED)
        continue;

      atomic_add (&condp->__seq_nw.hi, -1);
    }
  while (0);

  pthread_cleanup_pop (0);
  return (__pthread_mutex_cond_lock (mtxp));
}

int pthread_cond_timedwait (pthread_cond_t *condp,
  pthread_mutex_t *mtxp, const struct timespec *tsp)
{
  struct cv_cleanup cc = { .condp = condp, .mtxp = mtxp };
  int pshared = condp->__flags & GSYNC_SHARED;
  clockid_t clk = condp->__flags >> COND_CLK_SHIFT;
  
  /* Validate the timeout parameter. */
  if (tsp->tv_nsec < 0 || tsp->tv_nsec >= 1000000000)
    return (EINVAL);

  if (pshared == 0 && condp->__mutex == NULL)
    atomic_store (&condp->__mutex, mtxp);

  int ret = pthread_mutex_unlock (mtxp);
  if (ret != 0)
    return (ret);

  cc.sw.qv = atomic_addx_hi (&condp->__seq_nw.qv, 1);
  pthread_cleanup_push (cleanup, &cc);

  do
    {
      int prev = __pthread_cancelpoint_begin ();
      ret = lll_abstimed_wait (&condp->__seq_nw.lo,
        cc.sw.lo, tsp, pshared, clk);
      __pthread_cancelpoint_end (prev);

      if (ret == KERN_INTERRUPTED)
        continue;
      else if (ret == KERN_TIMEDOUT)
        ret = ETIMEDOUT;

      atomic_add (&condp->__seq_nw.hi, -1);
    }
  while (0);

  pthread_cleanup_pop (0);

  int r2 = __pthread_mutex_cond_lock (mtxp);
  if (ret == 0)
    /* The wait on the condition may have been successful, but
     * acquiring the mutex may not (e.g: robust locks). In such
     * cases, it's worth reporting the error. */
    ret = r2;

  return (ret);
}

int pthread_cond_signal (pthread_cond_t *condp)
{
  union hurd_xint tmp = { atomic_addx_lo (&condp->__seq_nw.qv, 1) };
  if (tmp.hi > 0)
    /* There are waiters; wake one of them. */
    lll_wake (&condp->__seq_nw.lo, condp->__flags & GSYNC_SHARED);

  return (0);
}

int pthread_cond_broadcast (pthread_cond_t *condp)
{
  union hurd_xint tmp = { atomic_addx_lo (&condp->__seq_nw.qv, 1) };

  if (tmp.hi > 0)
    {
      int flags = (condp->__flags & GSYNC_SHARED) | GSYNC_BROADCAST;

      if (condp->__mutex != NULL)
        /* If we saved a mutex, rearrange the waiting queues and
         * have a single waiter be awakened, and the rest moved into
         * waiting for the mutex instead. This avoids the infamous
         * 'thundering herd' issue. */
        lll_requeue (&condp->__seq_nw.lo, &condp->__mutex->__lock, 1, flags);
      else
        /* No mutex. Wake every waiter. */
        lll_wake (&condp->__seq_nw.lo, flags);
    }

  return (0);
}

int pthread_cond_destroy (pthread_cond_t *condp)
{
  return (atomic_load (&condp->__seq_nw.hi) != 0 ? EBUSY : 0);
}

static int
pt_hurd_cond_wait (pthread_cond_t *condp, pthread_mutex_t *mtxp,
  const struct timespec *tsp)
{
  struct hurd_sigstate *stp = _hurd_self_sigstate ();
  if (tsp != NULL && (tsp->tv_nsec < 0 || tsp->tv_nsec >= 1000000000))
    return (EINVAL);

  __spin_lock (&stp->lock);
  /* If we were cancelled previous to this call, don't do anything. */
  if (stp->cancel)
    {
      stp->cancel = 0;
      __spin_unlock (&stp->lock);
      return (EINTR);
    }

  int flags = condp->__flags & GSYNC_SHARED;

  void cancel_self (void)
    {
      /* We have to wake the interrupted thread. However, there's
       * no way to specify which thread is to be awakened with
       * 'gsync_wake', so we just do a broadcast. */
      atomic_add (&condp->__seq_nw.lo, 1);
      lll_wake (&condp->__seq_nw.lo, flags | GSYNC_BROADCAST);
    }

  if (!(flags & GSYNC_SHARED) && condp->__mutex == NULL)
    atomic_store (&condp->__mutex, mtxp);

  int ret = pthread_mutex_unlock (mtxp);
  if (ret != 0)
    {
      __spin_unlock (&stp->lock);
      return (ret);
    }

  /* Add ourselves as a waiter and register the cancellation callback. */
  union hurd_xint tmp =  { atomic_addx_hi (&condp->__seq_nw.qv, 1) };
  stp->cancel_hook = cancel_self;
  __spin_unlock (&stp->lock);

  do
    {
      ret = tsp == NULL ? lll_wait (&condp->__seq_nw.lo, tmp.lo, flags) :
        lll_abstimed_wait (&condp->__seq_nw.lo, tmp.lo, tsp, flags);

      if (ret == KERN_INTERRUPTED)
        continue;
      else if (ret == KERN_TIMEDOUT)
        ret = ETIMEDOUT;

      atomic_add (&condp->__seq_nw.hi, -1);
    }
  while (0);

  /* Clear the cancellation hook and fetch the flag. */
  __spin_lock (&stp->lock);
  stp->cancel_hook = 0;

  if (stp->cancel != 0)
    {
      ret = EINTR;
      stp->cancel = 0;
    }

  __spin_unlock (&stp->lock);

  if (ret != EINTR)
    /* Only reacquire the mutex if we weren't cancelled. */
    ret = __pthread_mutex_cond_lock (mtxp);

  return (ret);
}

int pthread_hurd_cond_wait_np (pthread_cond_t *condp, pthread_mutex_t *mtxp)
{
  return (pt_hurd_cond_wait (condp, mtxp, NULL));
}

int pthread_hurd_cond_timedwait_np (pthread_cond_t *condp,
  pthread_mutex_t *mtxp, const struct timespec *tsp)
{
  return (pt_hurd_cond_wait (condp, mtxp, tsp));
}

