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
#include "../sysdeps/atomic.h"
#include "sysdep.h"
#include <errno.h>

static const pthread_mutexattr_t dfl_attr =
{
  .__type = PTHREAD_MUTEX_NORMAL
};

int pthread_mutexattr_init (pthread_mutexattr_t *attrp)
{
  *attrp = dfl_attr;
  return (0);
}

int pthread_mutexattr_destroy (pthread_mutexattr_t *attrp)
{
  (void)attrp;
  return (0);
}

int pthread_mutexattr_settype (pthread_mutexattr_t *attrp, int type)
{
  if (type < 0 || type >= PTHREAD_MUTEX_TYPE_MAX)
    return (EINVAL);

  attrp->__type = type;
  return (0);
}

int pthread_mutexattr_gettype (const pthread_mutexattr_t *attrp, int *outp)
{
  *outp = attrp->__type;
  return (0);
}

int pthread_mutexattr_setpshared (pthread_mutexattr_t *attrp, int pshared)
{
  if (pshared != PTHREAD_PROCESS_PRIVATE &&
      pshared != PTHREAD_PROCESS_SHARED)
    return (EINVAL);

  attrp->__flags = (attrp->__flags & ~GSYNC_SHARED) |
    (pshared == PTHREAD_PROCESS_SHARED ? GSYNC_SHARED : 0);
  return (0);
}

int pthread_mutexattr_getpshared (const pthread_mutexattr_t *attrp, int *outp)
{
  *outp = (attrp->__flags & GSYNC_SHARED) ?
    PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
  return (0);
}

int pthread_mutexattr_setrobust (pthread_mutexattr_t *attrp, int robust)
{
  if (robust != PTHREAD_MUTEX_ROBUST &&
      robust != PTHREAD_MUTEX_STALLED)
    return (EINVAL);

  attrp->__flags |= robust;
  return (0);
}

int pthread_mutexattr_getrobust (const pthread_mutexattr_t *attrp, int *outp)
{
  *outp = (attrp->__flags & PTHREAD_MUTEX_ROBUST) ?
    PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED;
  return (0);
}

int pthread_mutexattr_setprioceiling (pthread_mutexattr_t *ap, int cl)
{
  (void)ap; (void)cl;
  return (ENOSYS);
}

int pthread_mutexattr_getprioceiling (const pthread_mutexattr_t *ap, int *clp)
{
  (void)ap; (void)clp;
  return (ENOSYS);
}

int pthread_mutexattr_setprotocol (pthread_mutexattr_t *attrp, int proto)
{
  (void)attrp;
  return (proto == PTHREAD_PRIO_NONE ? 0 :
    proto != PTHREAD_PRIO_INHERIT &&
    proto != PTHREAD_PRIO_PROTECT ? EINVAL : ENOSYS);
}

int pthread_mutexattr_getprotocol (const pthread_mutexattr_t *attrp, int *ptp)
{
  (void)attrp;
  *ptp = PTHREAD_PRIO_NONE;
  return (0);
}

int pthread_mutex_init (pthread_mutex_t *mtxp,
  const pthread_mutexattr_t *attrp)
{
  if (attrp == NULL)
    attrp = &dfl_attr;

  mtxp->__flags = attrp->__flags;
  mtxp->__type = attrp->__type;
  mtxp->__owner_id = 0;
  mtxp->__shpid = 0;
  mtxp->__cnt = 0;
  mtxp->__lock = 0;

  if ((mtxp->__flags & (PTHREAD_MUTEX_ROBUST |
      GSYNC_SHARED)) == PTHREAD_MUTEX_ROBUST)
    /* Silently disable robustness for task-local mutexes. */
    mtxp->__flags &= ~PTHREAD_MUTEX_ROBUST;

  return (0);
}

/* Lock routines. */

/* Special ID used to signal an irecoverable robust mutex. */
#define NOTRECOVERABLE_ID   (1U << 31)

/* Common path for robust mutexes. Assumes the variable 'ret'
 * is bound in the function this is called from. */
#define ROBUST_LOCK(self, mtxp, cb, ...)   \
  if (mtxp->__owner_id == NOTRECOVERABLE_ID)   \
    return (ENOTRECOVERABLE);   \
  else if (mtxp->__owner_id == self->id &&   \
      getpid () == (int)(mtxp->__lock & LLL_OWNER_MASK))   \
    {   \
      if (mtxp->__type == PTHREAD_MUTEX_RECURSIVE)   \
        {   \
          if (__glibc_unlikely (mtxp->__cnt + 1 == 0))   \
            return (EAGAIN);   \
          \
          ++mtxp->__cnt;   \
          return (0);   \
        }   \
      else if (mtxp->__type == PTHREAD_MUTEX_ERRORCHECK)   \
        return (EDEADLK);   \
    }   \
  \
  ret = cb (&mtxp->__lock, ##__VA_ARGS__);   \
  if (ret == 0 || ret == EOWNERDEAD)   \
    {   \
      if (mtxp->__owner_id == NOTRECOVERABLE_ID)   \
        ret = ENOTRECOVERABLE;   \
      else   \
        {   \
          mtxp->__owner_id = self->id;   \
          mtxp->__cnt = 1;   \
          if (ret == EOWNERDEAD)   \
            atomic_store (&mtxp->__lock,   \
              mtxp->__lock | LLL_DEAD_OWNER);   \
        }   \
    }   \
  (void)0

/* Check that a thread owns the mutex. For non-robust, task-shared
 * objects, we have to check the thread and process id. */
#define mtx_owned_p(mtx, pt, flags)   \
  ((mtx)->__owner_id == (pt)->id &&   \
    (((flags) & GSYNC_SHARED) == 0 ||   \
      (mtx)->__shpid == getpid ()))

/* Record a thread as the owner of the mutex. */
#define mtx_set_owner(mtx, pt, flags)   \
  (void)   \
    ({   \
       (mtx)->__owner_id = (pt)->id;   \
       if ((flags) & GSYNC_SHARED)   \
         (mtx)->__shpid = getpid ();   \
     })

/* Mutex type, including robustness. */
#define MTX_TYPE(mtxp)   \
  ((mtxp)->__type | ((mtxp)->__flags & PTHREAD_MUTEX_ROBUST))

int pthread_mutex_lock (pthread_mutex_t *mtxp)
{
  struct pthread *self = PTHREAD_SELF;
  int flags = mtxp->__flags & GSYNC_SHARED;
  int ret = 0;

  switch (MTX_TYPE (mtxp))
    {
      case PTHREAD_MUTEX_NORMAL:
        lll_lock (&mtxp->__lock, flags);
        break;

      case PTHREAD_MUTEX_RECURSIVE:
        if (mtx_owned_p (mtxp, self, flags))
          {
            if (__glibc_unlikely (mtxp->__cnt + 1 == 0))
              return (EAGAIN);

            ++mtxp->__cnt;
            return (ret);
          }

        lll_lock (&mtxp->__lock, flags);
        mtx_set_owner (mtxp, self, flags);
        mtxp->__cnt = 1;
        break;

      case PTHREAD_MUTEX_ERRORCHECK:
        if (mtx_owned_p (mtxp, self, flags))
          return (EDEADLK);

        lll_lock (&mtxp->__lock, flags);
        mtx_set_owner (mtxp, self, flags);
        break;

      case PTHREAD_MUTEX_NORMAL     | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_RECURSIVE  | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_ROBUST:
        ROBUST_LOCK (self, mtxp, lll_robust_lock, flags);
        break;

      default:
        ret = EINVAL;
        break;
    }

  return (ret);
}

int pthread_mutex_trylock (pthread_mutex_t *mtxp)
{
  struct pthread *self = PTHREAD_SELF;
  int ret;

  switch (MTX_TYPE (mtxp))
    {
      case PTHREAD_MUTEX_NORMAL:
        ret = lll_trylock (&mtxp->__lock);
        break;

      case PTHREAD_MUTEX_RECURSIVE:
        if (mtx_owned_p (mtxp, self, mtxp->__flags))
          {
            if (__glibc_unlikely (mtxp->__cnt + 1 == 0))
              return (EAGAIN);

            ++mtxp->__cnt;
            ret = 0;
          }
        else if ((ret = lll_trylock (&mtxp->__lock)) == 0)
          {
            mtx_set_owner (mtxp, self, mtxp->__flags);
            mtxp->__cnt = 1;
          }

        break;

      case PTHREAD_MUTEX_ERRORCHECK:
        if (mtx_owned_p (mtxp, self, mtxp->__flags))
          ret = EDEADLK;
        else if ((ret = lll_trylock (&mtxp->__lock)) == 0)
          mtx_set_owner (mtxp, self, mtxp->__flags);

        break;

      case PTHREAD_MUTEX_NORMAL     | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_RECURSIVE  | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_ROBUST:
        ROBUST_LOCK (self, mtxp, lll_robust_trylock);
        break;

      default:
        ret = EINVAL;
        break;
    }

  return (ret);
}

int pthread_mutex_timedlock (pthread_mutex_t *mtxp, 
  const struct timespec *tsp)
{
  struct pthread *self = PTHREAD_SELF;
  int ret, flags = mtxp->__flags & GSYNC_SHARED;

  switch (MTX_TYPE (mtxp))
    {
      case PTHREAD_MUTEX_NORMAL:
        ret = lll_abstimed_lock (&mtxp->__lock, tsp, flags);
        break;

      case PTHREAD_MUTEX_RECURSIVE:
        if (mtx_owned_p (mtxp, self, flags))
          {
            if (__glibc_unlikely (mtxp->__cnt + 1 == 0))
              return (EAGAIN);

            ++mtxp->__cnt;
            ret = 0;
          }
        else if ((ret = lll_abstimed_lock (&mtxp->__lock,
            tsp, flags)) == 0)
          {
            mtx_set_owner (mtxp, self, flags);
            mtxp->__cnt = 1;
          }

        break;

      case PTHREAD_MUTEX_ERRORCHECK:
        if (mtxp->__owner_id == self->id)
          return (EDEADLK);
        else if ((ret = lll_abstimed_lock (&mtxp->__lock,
            tsp, flags)) == 0)
          mtx_set_owner (mtxp, self, flags);

        break;

      case PTHREAD_MUTEX_NORMAL     | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_RECURSIVE  | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_ROBUST:
        ROBUST_LOCK (self, mtxp, lll_robust_abstimed_lock, tsp, flags);
        break;

      default:
        ret = EINVAL;
        break;
    }

  return (ret);
}

int pthread_mutex_unlock (pthread_mutex_t *mtxp)
{
  struct pthread *self = PTHREAD_SELF;
  int ret = 0, flags = mtxp->__flags & GSYNC_SHARED;

  switch (MTX_TYPE (mtxp))
    {
      case PTHREAD_MUTEX_NORMAL:
        lll_unlock (&mtxp->__lock, flags);
        break;

      case PTHREAD_MUTEX_RECURSIVE:
        if (!mtx_owned_p (mtxp, self, flags))
          ret = EPERM;
        else if (--mtxp->__cnt == 0)
          {
            mtxp->__owner_id = mtxp->__shpid = 0;
            lll_unlock (&mtxp->__lock, flags);
          }

        break;

      case PTHREAD_MUTEX_ERRORCHECK:
        if (!mtx_owned_p (mtxp, self, flags))
          ret = EPERM;
        else
          {
            mtxp->__owner_id = mtxp->__shpid = 0;
            lll_unlock (&mtxp->__lock, flags);
          }

        break;

      case PTHREAD_MUTEX_NORMAL     | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_RECURSIVE  | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_ROBUST:
        if (mtxp->__owner_id == NOTRECOVERABLE_ID)
          ;   /* Nothing to do. */
        else if (mtxp->__owner_id != self->id ||
            (int)(mtxp->__lock & LLL_OWNER_MASK) != getpid ())
          ret = EPERM;
        else if (--mtxp->__cnt == 0)
          {
            /* Release the lock. If it's in an inconsistent
             * state, mark it as not recoverable. */
            mtxp->__owner_id = (mtxp->__lock & LLL_DEAD_OWNER) ?
              NOTRECOVERABLE_ID : 0;
            lll_robust_unlock (&mtxp->__lock, flags);
          }

        break;

      default:
        ret = EINVAL;
        break;
    }

  return (ret);
}

int pthread_mutex_consistent (pthread_mutex_t *mtxp)
{
  int ret = EINVAL;
  unsigned int val = mtxp->__lock;

  if ((mtxp->__flags & PTHREAD_MUTEX_ROBUST) != 0 &&
      (val & LLL_DEAD_OWNER) != 0 &&
      atomic_cas_bool (&mtxp->__lock, val, getpid () | LLL_WAITERS))
    {
      /* The mutex is now ours, and it's consistent. */
      mtxp->__owner_id = PTHREAD_SELF->id;
      mtxp->__cnt = 1;
      ret = 0;
    }

  return (ret);
}

int pthread_mutex_transfer_np (pthread_mutex_t *mtxp, pthread_t th)
{
  struct pthread *self = PTHREAD_SELF;
  struct pthread *pt = (struct pthread *)th;

  if (INVALID_P (pt))
    return (ESRCH);
  else if (pt == self)
    return (0);

  int ret = 0;
  int flags = mtxp->__flags & GSYNC_SHARED;

  switch (MTX_TYPE (mtxp))
    {
      case PTHREAD_MUTEX_NORMAL:
        break;

      case PTHREAD_MUTEX_RECURSIVE:
      case PTHREAD_MUTEX_ERRORCHECK:
        if (!mtx_owned_p (mtxp, self, flags))
          ret = EPERM;
        else
          mtx_set_owner (mtxp, pt, flags);

        break;

      case PTHREAD_MUTEX_NORMAL     | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_RECURSIVE  | PTHREAD_MUTEX_ROBUST:
      case PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_ROBUST:
        /* Note that this can be used to transfer an inconsistent
         * mutex as well. The new owner will still have the same
         * flags as the original. */
        if (mtxp->__owner_id != self->id ||
            (int)(mtxp->__lock & LLL_OWNER_MASK) != getpid ())
          ret = EPERM;
        else
          mtxp->__owner_id = pt->id;
        break;

      default:
        ret = EINVAL;
    }

  return (ret);
}

int pthread_mutex_setprioceiling (pthread_mutex_t *mtxp, int cl, int *prp)
{
  (void)mtxp; (void)cl; (void)prp;
  return (ENOSYS);
}

int pthread_mutex_getprioceiling (const pthread_mutex_t *mtxp, int *clp)
{
  (void)mtxp; (void)clp;
  return (ENOSYS);
}

int pthread_mutex_destroy (pthread_mutex_t *mtxp)
{
  if (atomic_load (&mtxp->__lock) != 0)
    return (EBUSY);

  mtxp->__type = PTHREAD_MUTEX_TYPE_MAX;
  return (0);
}

