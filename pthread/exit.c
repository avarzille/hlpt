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

/* Definitions from libgcc's unwind API. */
#define END_OF_STACK   0x10

extern void* _Unwind_GetCFA (void *);
extern void _Unwind_ForcedUnwind (void *, int (*) (int, int,
  unsigned long long, void *, void *, void *), void *);

#define FRAME_LEFT(cfa, buf)   \
  ((unsigned long)(cfa) >= (unsigned long)(buf))

static int
unwind_cleanup (int version, int actions,
  unsigned long long cls, void *obj, void *ctx, void *argp)
{
  struct pthread *self = (struct pthread *)argp;
  void *cfa = _Unwind_GetCFA (ctx);
  int last = (actions & END_OF_STACK) ||
    !FRAME_LEFT (obj, cfa);

  /* Only execute the cleanup handlers for the current frame,
   * in order to properly support stack unwinding. */
  while (self->cleanup != NULL && 
      (FRAME_LEFT (cfa, self->cleanup) || last))
    {
      self->cleanup->__fct (self->cleanup->__argp);
      self->cleanup = self->cleanup->__next;
    }

  if (last != 0)
    /* If we are at the end of stack, cleanup and exit. */
    __pthread_cleanup (self);

  (void)version; (void)cls;
  return (0);
}

/* Failing to rethrow an exception inside a 'catch-all' block
 * is a fatal error. If that happens, we simply abort. */
extern void __libc_fatal (const char *) __attribute__ ((noreturn));

static void
unwind_abort (int reason, void *excp)
{
  (void)reason; (void)excp;
  __libc_fatal ("FATAL: Exception must be rethrown\n");
}

void pthread_exit (void *retval)
{
  struct pthread *self = PTHREAD_SELF;

  self->retval = retval;
  atomic_or (&self->flags, PTHREAD_FLG_EXITING);

  /* The exception we use for forced stack unwinding can be considered
   * as 'special', so the class remains zero. On the other hand, we do
   * set the cleanup callback for when things turn out badly. */
  self->exc.cleanup = unwind_abort;
  _Unwind_ForcedUnwind (&self->exc, unwind_cleanup, self);
  /* We better not get here. */
  __builtin_unreachable ();
}

