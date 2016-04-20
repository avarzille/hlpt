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

static const pthread_barrierattr_t dfl_attr;

int pthread_barrierattr_init (pthread_barrierattr_t *attrp)
{
  *attrp = dfl_attr;
  return (0);
}

int pthread_barrierattr_setpshared (pthread_barrierattr_t *attrp, int pshared)
{
  if (pshared != PTHREAD_PROCESS_SHARED &&
      pshared != PTHREAD_PROCESS_PRIVATE)
    return (EINVAL);

  attrp->__flags = (attrp->__flags & ~GSYNC_SHARED) |
    (pshared == PTHREAD_PROCESS_SHARED ? GSYNC_SHARED : 0);
  return (0);
}

int pthread_barrierattr_getpshared (const pthread_barrierattr_t *attrp, 
  int *outp)
{
  *outp = (attrp->__flags & GSYNC_SHARED) ?
    PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
  return (0);
}

int pthread_barrierattr_destroy (pthread_barrierattr_t *attrp)
{
  (void)attrp;
  return (0);
}

int pthread_barrier_init (pthread_barrier_t *barp,
  const pthread_barrierattr_t *attrp, unsigned int cnt)
{
  if (__glibc_unlikely (cnt == 0))
    return (EINVAL);
  else if (attrp == NULL)
    attrp = &dfl_attr;

  barp->__seq_cnt.qv = 0;
  barp->__nrefs = 1;
  barp->__total = cnt - 1;
  barp->__flags = attrp->__flags;

  return (0);
}

int pthread_barrier_wait (pthread_barrier_t *barp)
{
  int ret = 0, pshared = barp->__flags & GSYNC_SHARED;
  atomic_add (&barp->__nrefs, 1);

  do
    {
      unsigned int seq = atomic_load (&barp->__seq_cnt.lo);
      unsigned int cnt = atomic_add (&barp->__seq_cnt.hi, 1);

      if (cnt < barp->__total)
        {
          /* Sleep on the sequence address as long as we
           * cannot proceed. */
          while (barp->__seq_cnt.lo == seq)
            lll_wait (&barp->__seq_cnt.lo, seq, pshared);
        }
      else if (cnt == barp->__total)
        {
          /* Clear count and bump sequence number. */
          atomic_storeq (&barp->__seq_cnt.qv, barp->__seq_cnt.lo + 1);

          /* Tell everyone that we're done. */
          lll_wake (&barp->__seq_cnt.lo, pshared | GSYNC_BROADCAST);
          ret = PTHREAD_BARRIER_SERIAL_THREAD;
        }
      else
        {
          /* Wait for the barrier to be released. */
          lll_wait (&barp->__seq_cnt.lo, seq, pshared);
          continue;
        }
    }
  while (0);

  /* If we are the last to wake up, notify the destroying thread. */
  if (atomic_add (&barp->__nrefs, -1) == 1)
    lll_wake (&barp->__nrefs, pshared);

  return (ret);
}

int pthread_barrier_destroy (pthread_barrier_t *barp)
{
  int pshared = barp->__flags & GSYNC_SHARED;
  unsigned int refc = atomic_add (&barp->__nrefs, -1) - 1;

  /* Loop until every reference to this barrier is gone. */
  while (refc != 0)
    {
      lll_wait (&barp->__nrefs, refc, pshared);
      refc = atomic_load (&barp->__nrefs);
    }

  return (0);
}

