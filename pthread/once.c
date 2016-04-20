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

/* A once-control variable may be in one of 3 states:
 *
 * - ONCE_INIT: The first state. Threads that enter 'pthread_once' on
 *   the same address will compete to see who gets to execute the
 *   initialization routine. 
 *
 * - ONCE_WIP | PID: One thread is executing the routine, and the
 *   others are waiting for it to finish. See below as to why we
 *   use the PID for this flag.
 *
 * - ONCE_DONE: The routine has been successfully executed.
 */

#define ONCE_INIT   0
#define ONCE_WIP    1
#define ONCE_DONE   2

#define ONCE_SHIFT   2

/* Sanity check. */
#if PTHREAD_ONCE_INIT != ONCE_INIT
#  error "PTHREAD_ONCE_INIT must be zero"
#endif

static void
cleanup (void *argp)
{
  /* During cleanup, wake everyone and reset the value
   * to its initial state. */
  lll_set_wake (argp, ONCE_INIT, GSYNC_BROADCAST);
}

int pthread_once (pthread_once_t *ctp, void (*fct) (void))
{
  int val = atomic_load (ctp);
  if (val == ONCE_DONE)
    return (0);

  /* In order to handle 'fork' correctly, we need to keep in mind
   * that a child might inherit a once-control variable that is
   * in progress. To counter that, we use the PID as a unique token
   * that allows us to know if that's precisely the case. */

  int id = getpid () << ONCE_SHIFT;
  while (1)
    {
      int nv = id | ONCE_WIP;
      val = atomic_load (ctp);

      if (val == ONCE_DONE)
        break;
      else if (val == nv)
        /* Someone beat us to it. Wait on the address. */
        lll_wait (ctp, val, 0);
      else if (atomic_cas_bool (ctp, val, nv))
        {
          /* We're alone. Install the cleanup handler, execute the
           * routine and pop the handler. */
          pthread_cleanup_push (cleanup, ctp);
          fct ();
          pthread_cleanup_pop (0);

          /* Now, set the value to its final state, and let everyone
           * know that we finished the job. */
          lll_set_wake (ctp, ONCE_DONE, GSYNC_BROADCAST);
        }
    }

  return (0);
}

