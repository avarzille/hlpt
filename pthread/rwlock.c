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
#include <errno.h>
#include <time.h>

static const pthread_rwlockattr_t dfl_attr;

int pthread_rwlockattr_init (pthread_rwlockattr_t *attrp)
{
  *attrp = dfl_attr;
  return (0);
}

int pthread_rwlockattr_setpshared (pthread_rwlockattr_t *attrp, int pshared)
{
  if (pshared != PTHREAD_PROCESS_SHARED &&
      pshared != PTHREAD_PROCESS_PRIVATE)
    return (EINVAL);

  attrp->__flags = (attrp->__flags & ~GSYNC_SHARED) |
    (pshared == PTHREAD_PROCESS_SHARED ? GSYNC_SHARED : 0);
  return (0);
}

int pthread_rwlockattr_getpshared (const pthread_rwlockattr_t *attrp, 
  int *outp)
{
  *outp = (attrp->__flags & GSYNC_SHARED) ?
    PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
  return (0);
}

int pthread_rwlockattr_destroy (pthread_rwlockattr_t *attrp)
{
  (void)attrp;
  return (0);
}

/* Read-write locks have the following memory layout:
 * 0         4            8            12           16
 * |__shpid__|__nwriters__|__owner_id__|__nreaders__|
 *
 * Whenever a thread wants to acquire the lock, it first
 * checks the OID field. It may be unowned, owned by readers, or
 * owned by a particular writer. For reader ownership, we use a
 * special OID that no thread can ever have.
 *
 * When it comes to waiting for the lock to change ownership, we
 * need different wait queues for readers and writers. However, 
 * both readers and writers have to monitor the OID field for changes.
 * This is where 64-bit gsync comes into play: Readers will wait on 
 * the address of the OID, while writers will wait on the 64-bit address
 * that starts at NWRITERS and extends to OID as well.
 *
 * This approach can cause some extra work on the writer side,
 * but it's more efficient by virtue of being lockless. As long
 * as we have 64-bit atomics, we can safely implement the POSIX
 * read-write lock API without internal locks. */

#define __rwl_atp(ptr, idx)   (((unsigned int *)(ptr))[idx])

/* Access to the 3 fields described above. */
#define rwl_qwr(val)   __rwl_atp (&(val), -1)
#define rwl_oid(val)   __rwl_atp (&(val), +0)
#define rwl_nrd(val)   __rwl_atp (&(val), +1)

/* Special ID's to represent unowned and readers-owned locks. */
#define RWLOCK_UNOWNED   (0)
#define RWLOCK_RO        (1U << 31)

/* Access the owner's PID for task-shared rwlocks. */
#define rwl_spid(rwl)   *(unsigned int *)&(rwl)->__shpid_qwr

int pthread_rwlock_init (pthread_rwlock_t *rwp,
  const pthread_rwlockattr_t *attrp)
{
  if (attrp == NULL)
    attrp = &dfl_attr;

  rwp->__shpid_qwr.qv = rwp->__oid_nrd.qv = 0;
  rwp->__flags = attrp->__flags;
  return (0);
}

/* We need a function if we're using a macro that expands
 * to a list of arguments (ugh). */
static inline int
catomic_casx_bool (unsigned long long *ptr, unsigned int elo,
  unsigned int ehi, unsigned int nlo, unsigned int nhi)
{
  return (atomic_casx_bool (ptr, elo, ehi, nlo, nhi));
}

/* Test that a read-write lock is owned by a particular thread. */
#define rwl_owned_p(rwp, tid, flags)   \
  (rwl_oid ((rwp)->__oid_nrd) == (tid) &&   \
    (((flags) & GSYNC_SHARED) == 0 ||   \
      rwl_spid (rwp) == (unsigned int)getpid ()))

#define rwl_setown(rwp, flags)   \
  do   \
    {   \
      if (((flags) & GSYNC_SHARED) != 0)   \
        rwl_spid(rwp) = getpid ();   \
    }   \
  while (0)

int pthread_rwlock_rdlock (pthread_rwlock_t *rwp)
{
  int flags = rwp->__flags & GSYNC_SHARED;

  /* Test that we don't own the lock already. */
  if (rwl_owned_p (rwp, PTHREAD_SELF->id, flags))
    return (EDEADLK);

  while (1)
    {
      union hurd_xint tmp = { atomic_loadx (&rwp->__oid_nrd.qv) };
      if ((rwl_oid (tmp) & PTHREAD_ID_MASK) == 0)
        {
          /* The lock is either unowned, or the readers hold it. */
          if (catomic_casx_bool (&rwp->__oid_nrd.qv,
              hurd_xint_pair (tmp.lo, tmp.hi),
              hurd_xint_pair (RWLOCK_RO, rwl_nrd (tmp) + 1)))
            {
              /* If we grabbed an unowned lock and there were readers
               * queued, notify our fellows so they stop blocking. */
              if (rwl_oid (tmp) == RWLOCK_UNOWNED && rwl_nrd (tmp) > 0)
                lll_wake (&rwl_oid(rwp->__oid_nrd), flags | GSYNC_BROADCAST);

              return (0);
            }
        }
      else
        {
          /* A writer holds the lock. Sleep. */
          atomic_add (&rwl_nrd(rwp->__oid_nrd), +1);
          lll_wait (&rwl_oid(rwp->__oid_nrd), rwl_oid (tmp), flags);
          atomic_add (&rwl_nrd(rwp->__oid_nrd), -1);
        }
    }
}

int pthread_rwlock_tryrdlock (pthread_rwlock_t *rwp)
{
  if (rwl_owned_p (rwp, PTHREAD_SELF->id, rwp->__flags))
    return (EDEADLK);

  while (1)
    {
      union hurd_xint tmp = { atomic_loadx (&rwp->__oid_nrd.qv) };
      if ((rwl_oid (tmp) & PTHREAD_ID_MASK) != 0)
        return (EBUSY);
      else if (catomic_casx_bool (&rwp->__oid_nrd.qv,
          hurd_xint_pair (tmp.lo, tmp.hi),
          hurd_xint_pair (RWLOCK_RO, rwl_nrd (tmp) + 1)))
        {
          if (rwl_oid (tmp) == RWLOCK_UNOWNED && rwl_nrd (tmp) > 0)
            lll_wake (&rwl_oid(rwp->__oid_nrd), GSYNC_BROADCAST |
              (rwp->__flags & GSYNC_SHARED));

          return (0);
        }
    }
}

int pthread_rwlock_timedrdlock (pthread_rwlock_t *rwp,
  const struct timespec *abstime)
{
  int flags = rwp->__flags & GSYNC_SHARED;

  if (rwl_owned_p (rwp, PTHREAD_SELF->id, flags))
    return (EDEADLK);

  while (1)
    {
      union hurd_xint tmp = { atomic_loadx (&rwp->__oid_nrd.qv) };
      if ((rwl_oid (tmp) & PTHREAD_ID_MASK) == 0)
        {
          if (catomic_casx_bool (&rwp->__oid_nrd.qv,
              hurd_xint_pair (tmp.lo, tmp.hi),
              hurd_xint_pair (RWLOCK_RO, rwl_nrd (tmp) + 1)))
            {
              if (rwl_oid (tmp) == RWLOCK_UNOWNED && rwl_nrd (tmp) > 0)
                lll_wake (&rwl_oid(rwp->__oid_nrd), flags | GSYNC_BROADCAST);

              return (0);
            }
        }
      else
        {
          /* The timeout parameter has to be checked on every iteration,
           * because its value must not be examined if the lock can be
           * taken without blocking. */

          if (__glibc_unlikely (abstime->tv_nsec < 0 ||
              abstime->tv_nsec >= 1000000000))
            return (EINVAL);

          atomic_add (&rwl_nrd(rwp->__oid_nrd), +1);
          int ret = lll_abstimed_wait (&rwl_oid(rwp->__oid_nrd),
            rwl_oid (tmp), abstime, flags);
          atomic_add (&rwl_nrd(rwp->__oid_nrd), -1);

          if (ret == KERN_TIMEDOUT)
            return (ETIMEDOUT);
        }
    }
}

int pthread_rwlock_wrlock (pthread_rwlock_t *rwp)
{
  int flags = rwp->__flags & GSYNC_SHARED;
  unsigned int self_id = PTHREAD_SELF->id;

  if (rwl_owned_p (rwp, self_id, flags))
    return (EDEADLK);

  while (1)
    {
      unsigned int owner = atomic_load (&rwl_oid(rwp->__oid_nrd));
      if (owner == RWLOCK_UNOWNED)
        {
          /* The lock is unowned. Try to take ownership. */
          if (atomic_cas_bool (&rwl_oid(rwp->__oid_nrd), owner, self_id))
            {
              rwl_setown (rwp, flags);
              return (0);
            }
        }
      else
        {
          /* Wait on the address. We are only interested in the
           * value of the OID field, but we need a different queue
           * for writers. As such, we use 64-bit values, with the
           * high limb being the owner id. */
          unsigned int *ptr = &rwl_qwr(rwp->__oid_nrd);
          unsigned int nw = atomic_add (ptr, +1);
          lll_xwait (ptr, nw + 1, owner, flags);
          atomic_add (ptr, -1);
        }
    }
}

int pthread_rwlock_trywrlock (pthread_rwlock_t *rwp)
{
  unsigned int self_id = PTHREAD_SELF->id;
  unsigned int owner = atomic_load (&rwl_oid(rwp->__oid_nrd));

  if (rwl_owned_p (rwp, self_id, rwp->__flags))
    return (EDEADLK);
  else if (owner == RWLOCK_UNOWNED &&
      atomic_cas_bool (&rwl_oid(rwp->__oid_nrd), owner, self_id))
    {
      rwl_setown (rwp, rwp->__flags);
      return (0);
    }
  else
    return (EBUSY);
}

int pthread_rwlock_timedwrlock (pthread_rwlock_t *rwp,
  const struct timespec *abstime)
{
  unsigned int self_id = PTHREAD_SELF->id;
  int flags = rwp->__flags & GSYNC_SHARED;

  if (rwl_owned_p (rwp, self_id, flags))
    return (EDEADLK);

  while (1)
    {
      unsigned int owner = atomic_load (&rwl_oid(rwp->__oid_nrd));
      if (owner == RWLOCK_UNOWNED)
        {
          if (atomic_cas_bool (&rwl_oid(rwp->__oid_nrd), owner, self_id))
            {
              rwl_setown (rwp, flags);
              return (0);
            }
        }
      else
        {
          if (__glibc_unlikely (abstime->tv_nsec < 0 ||
              abstime->tv_nsec >= 1000000000))
            return (EINVAL);

          unsigned int *ptr = &rwl_qwr(rwp->__oid_nrd);
          unsigned int nw = atomic_add (ptr, +1);

          int ret = lll_abstimed_xwait (ptr,
            nw + 1, owner, abstime, flags);
          nw = atomic_add (ptr, -1);

          if (ret == KERN_TIMEDOUT)
            {
              /* If we timed out, there are no writers pending, the
               * lock is unowned and there are readers blocked, it's
               * possible that a wakeup was destined for us, but the
               * timeout came first. In such (unlikely) case, we wake
               * all readers in order to avoid a potential deadlock. */

              union hurd_xint tmp = { atomic_loadx (&rwp->__oid_nrd.qv) };
              if (__glibc_unlikely (nw == 1 && rwl_nrd (tmp) > 0 &&
                  rwl_oid (tmp) == RWLOCK_UNOWNED))
                lll_wake (&rwl_oid(rwp->__oid_nrd), flags | GSYNC_BROADCAST);

              /* We still return with an error. */
              return (ETIMEDOUT);
            }
        }
    }
}

int pthread_rwlock_unlock (pthread_rwlock_t *rwp)
{
  unsigned int owner = atomic_load (&rwl_oid(rwp->__oid_nrd));
  int flags = rwp->__flags & GSYNC_SHARED;

  if ((owner & PTHREAD_ID_MASK) != 0)
    {
      /* A writer holds the lock. */
      if (!rwl_owned_p (rwp, PTHREAD_SELF->id, flags))
        /* ... But it isn't us. */
        return (EPERM);

      rwl_spid(rwp) = 0;
      atomic_store (&rwl_oid(rwp->__oid_nrd), RWLOCK_UNOWNED);

      /* The exclusive lock is no longer being held. Now decide
       * whether to wake a queued writer (preferred), or all
       * the queued readers. */
      if (rwl_qwr (rwp->__oid_nrd) > 0)
        lll_wake (&rwl_qwr(rwp->__oid_nrd), flags);
      else if (rwl_nrd (rwp->__oid_nrd) > 0)
        lll_wake (&rwl_oid(rwp->__oid_nrd), flags | GSYNC_BROADCAST);
    }
  else if (rwl_nrd (rwp->__oid_nrd) == 0)
    return (EPERM);
  else
    {
      union hurd_xint tmp;
      while (1)
        {
          tmp.qv = atomic_loadx (&rwp->__oid_nrd.qv);
          if (catomic_casx_bool (&rwp->__oid_nrd.qv,
              hurd_xint_pair (tmp.lo, tmp.hi),
              hurd_xint_pair (rwl_nrd (tmp) == 1 ?
                RWLOCK_UNOWNED : rwl_oid (tmp), rwl_nrd (tmp) - 1)))
            break;
        }

      /* As a reader, we only need to do a wakeup if:
       * - We were the last one.
       * - There's at least a writer queued. */
      if (rwl_nrd (tmp) == 1 && rwl_qwr (rwp->__oid_nrd) > 0)
        lll_wake (&rwl_qwr(rwp->__oid_nrd), flags);
    }

  return (0);
}

int pthread_rwlock_destroy (pthread_rwlock_t *rwp)
{
  /* XXX: Maybe we could do some sanity checks. */
  (void)rwp;
  return (0);
}

