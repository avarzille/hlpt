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
#include <errno.h>

#define SPIN_UNLOCKED   PTHREAD_SPINLOCK_INITIALIZER
#define SPIN_LOCKED     (SPIN_UNLOCKED ^ 1)

int pthread_spin_init (pthread_spinlock_t *lockp, int pshared)
{
  if (pshared != PTHREAD_PROCESS_SHARED &&
      pshared != PTHREAD_PROCESS_PRIVATE)
    return (EINVAL);

  *lockp = SPIN_UNLOCKED;
  return (0);
}

/* XXX: This constant needs some tuning. */
#define NSPINS   1000

int pthread_spin_lock (pthread_spinlock_t *lockp)
{
  while (1)
    {
      if (atomic_swap (lockp, SPIN_LOCKED) == SPIN_UNLOCKED)
        break;

      int nspins = NSPINS;
      while (*lockp == SPIN_LOCKED && --nspins != 0)
#if defined (i386) || defined (__i386__)
        __asm__ ("pause");
#else
        atomic_mfence_acq ();
#endif
    }

  return (0);
}

int pthread_spin_trylock (pthread_spinlock_t *lockp)
{
  return (*lockp != SPIN_UNLOCKED ||
    atomic_swap (lockp, SPIN_LOCKED) != SPIN_UNLOCKED ? EBUSY : 0);
}

int pthread_spin_unlock (pthread_spinlock_t *lockp)
{
  atomic_store (lockp, SPIN_UNLOCKED);
  return (0);
}

int pthread_spin_destroy (pthread_spinlock_t *lockp)
{
  (void)lockp;
  return (0);
}

