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
#include <errno.h>

int pthread_detach (pthread_t th)
{
  struct pthread *pt = (struct pthread *)th;
  int ret = 0;

  if (INVALID_P (pt))
    ret = ESRCH;
  else if (DETACHED_P (pt))
    ret = EINVAL;
  else if (atomic_cas_bool (&pt->joinpt, NULL, pt) &&
      (pt->flags & PT_FLG_EXITING) != 0)
    {
      /* The thread is performing cleanup thinking it's
       * joinable. Wait for it to finish and then deallocate
       * its stack ourselves. */
      unsigned int id;
      while ((id = pt->id) != 0)
        lll_wait (&pt->id, id, 0);

      __pthread_deallocate (pt);
    }

  return (ret);
}

