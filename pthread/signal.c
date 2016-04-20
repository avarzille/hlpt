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
#include <hurd/signal.h>

int pthread_kill (pthread_t th, int sig)
{
  struct pthread *pt = (struct pthread *)th;

  if (INVALID_P (pt))
    return (ESRCH);

  struct hurd_signal_detail detail =
    { .exc = 0, .code = sig, .error = 0 };

  __spin_lock (&__pthread_sigstate(pt)->lock);
  _hurd_raise_signal (__pthread_sigstate (pt), sig, &detail);
  return (0);
}

int pthread_sigmask (int how, const sigset_t *setp, sigset_t *prevp)
{
  struct hurd_sigstate *stp = __pthread_sigstate (PTHREAD_SELF);
  _hurd_sigstate_lock (stp);

  if (prevp != NULL)
    *prevp = stp->blocked;
  if (setp == NULL)
    {
      _hurd_sigstate_unlock (stp);
      return (0);
    }

  switch (how)
    {
      case SIG_BLOCK:
        sigorset (&stp->blocked, &stp->blocked, setp);
        break;

      case SIG_SETMASK:
        stp->blocked = *setp;
        break;

      case SIG_UNBLOCK:
        stp->blocked &= ~*setp;
        break;

      default:
        _hurd_sigstate_unlock (stp);
        return (EINVAL);
    }

  /* Make sure we don't block some signals. */
  stp->blocked &= ~_SIG_CANT_MASK;

  /* Get the new set of pending signals. */
  sigset_t pending = _hurd_sigstate_pending (stp) & ~stp->blocked;

  /* Done with the state lock. */
  _hurd_sigstate_unlock (stp);

  /* If we unblocked signals, and there are pending ones,
   * notify the signal thread now. */
  if (how != SIG_BLOCK && !sigemptyset (&pending))
    __msg_sig_post (_hurd_msgport, 0, 0, mach_task_self ());

  return (0);
}

