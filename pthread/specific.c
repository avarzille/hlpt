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
#include "sysdep.h"
#include "../sysdeps/atomic.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Unused keys have even sequence numbers. This is useful for static
 * initialization as __pthread_keys will be zero-filled. */
static inline int __attribute__ ((always_inline))
unused_p (unsigned long seq)
{
  return (seq % 2 == 0);
}

int pthread_key_create (pthread_key_t *keyp, void (*destr) (void *))
{
  for (int i = 0; i < PTHREAD_KEYS_MAX; ++i)
    {
      unsigned long seq = __pthread_keys[i].seq;
      if (unused_p (seq) &&
          atomic_cas_bool (&__pthread_keys[i].seq, seq, seq + 1))
        {
          __pthread_keys[i].destr = destr;
          *keyp = i;
          return (0);
        }
    }

  return (EAGAIN);
}

int pthread_key_delete (pthread_key_t key)
{
  if (key >= PTHREAD_KEYS_MAX || unused_p (__pthread_keys[key].seq))
    return (EINVAL);

  atomic_add (&__pthread_keys[key].seq, 1);
  return (0);
}

void* pthread_getspecific (pthread_key_t key)
{
  if (key >= PTHREAD_KEYS_MAX)
    return (NULL);

  struct pthread *self = PTHREAD_SELF;
  unsigned int idx = key / PTHREAD_KEY_L2_SIZE;
  struct pthread_key_data *lp = self->specific[idx];

  if (lp == NULL)
    return (NULL);

  lp += key % PTHREAD_KEY_L2_SIZE;
  void *retp = lp->data;

  /* Test that the key sequence matches with the global array. */
  if (__glibc_unlikely (retp != NULL &&
      lp->seq != __pthread_keys[key].seq))
    retp = lp->data = NULL;

  return (retp);
}

int pthread_setspecific (pthread_key_t key, const void *valp)
{
  unsigned long seq;

  if (key >= PTHREAD_KEYS_MAX ||
      unused_p ((seq = __pthread_keys[key].seq)))
    return (EINVAL);

  struct pthread *self = PTHREAD_SELF;
  unsigned int idx = key / PTHREAD_KEY_L2_SIZE;
  struct pthread_key_data *lp = self->specific[idx];

  if (lp == NULL)
    {
      if (valp == NULL)
        /* Don't bother with memory allocations if we were
         * going to set a null pointer anyways. */
        return (0);

      lp = (struct pthread_key_data *)
        calloc (PTHREAD_KEY_L2_SIZE, sizeof (*lp));

      if (lp == NULL)
        return (ENOMEM);

      self->specific[idx] = lp;
    }

  lp += key % PTHREAD_KEY_L2_SIZE;
  /* Store the sequence number for future validations. */
  lp->seq = seq;
  lp->data = (void *)valp;

  /* Only set the specific flag if the value needs destruction
   * during thread-exit time. */
  if (__glibc_likely (valp != NULL))
    self->specific_used = 1;

  return (0);
}

void __pthread_dealloc_tsd (struct pthread *pt)
{
  /* Avoid doing any work if no thread-specific data was allocated. */
  if (!pt->specific_used)
    return;

  int cnt, iters = 0;

  do
    {
      int q;

      pt->specific_used = 0;
      for (q = cnt = 0; cnt < PTHREAD_KEY_L1_SIZE; ++cnt)
        {
          struct pthread_key_data *l2 = pt->specific[cnt];
          if (l2 == NULL)
            {
              q += PTHREAD_KEY_L1_SIZE;
              continue;
            }

          for (int inner = 0; inner < PTHREAD_KEY_L2_SIZE; ++inner, ++q)
            {
              void *dp = l2[inner].data;
              if (dp == NULL)
                continue;

              /* Clear the data before (potentially) running
               * the destructor callback. */
              l2[inner].data = NULL;

              /* Only run the destructor if the key is valid and if
               * a callback was actually registered for this key. */
              if (l2[inner].seq == __pthread_keys[q].seq &&
                  __pthread_keys[q].destr != NULL)
                __pthread_keys[q].destr (dp);
            }
        }

      /* If no more thread-specific data was allocated, we're done. */
      if (!pt->specific_used)
        break;
    }
  while (++iters < PTHREAD_DESTRUCTION_ITERATIONS);

  memset (pt->specific_blk1, 0, sizeof (pt->specific_blk1));

  for (cnt = 1; cnt < PTHREAD_KEY_L1_SIZE; ++cnt)
    {
      free (pt->specific[cnt]);
      pt->specific[cnt] = NULL;
    }

  pt->specific_used = 0;
}

