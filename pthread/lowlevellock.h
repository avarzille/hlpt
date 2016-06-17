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

#ifndef __LOWLEVELLOCK_H__
#define __LOWLEVELLOCK_H__   1

struct timespec;

#define GSYNC_SHARED      0x01
#define GSYNC_QUAD        0x02
#define GSYNC_TIMED       0x04
#define GSYNC_BROADCAST   0x08
#define GSYNC_MUTATE      0x10

/* Additional kernel return values. */
#define KERN_TIMEDOUT      27
#define KERN_INTERRUPTED   28

/* Flags for robust locks. */
#define LLL_WAITERS      (1U << 31)
#define LLL_DEAD_OWNER   (1U << 30)

#define LLL_OWNER_MASK   ~(LLL_WAITERS | LLL_DEAD_OWNER)

/* Convenience wrappers around the 'gsync' RPC's. */

extern int lll_wait (void *__ptr, int __val, int __flags);

extern int lll_xwait (void *__ptr, int __lo,
  int __hi, int __flags);

extern int lll_timed_wait (void *__ptr, int __val,
  int __mlsec, int __flags);

extern int lll_timed_xwait (void *__ptr, int __lo,
  int __hi, int __mlsec, int __flags);

extern int __lll_abstimed_wait (void *__ptr, int __val,
  const struct timespec *__tsp, int __flags, int __clk);

extern int __lll_abstimed_xwait (void *__ptr, int __lo, int __hi,
  const struct timespec *__tsp, int __flags, int __clk);

extern int lll_lock (void *__ptr, int __flags);

extern int __lll_abstimed_lock (void *__ptr,
  const struct timespec *__tsp, int __flags, int __clk);

extern int lll_trylock (void *__ptr);

extern int lll_robust_lock (void *__ptr, int __flags);

extern int __lll_robust_abstimed_lock (void *__ptr,
  const struct timespec *__tsp, int __flags, int __clk);

extern int lll_robust_trylock (void *__ptr);

extern void lll_wake (void *__ptr, int __flags);

extern void lll_set_wake (void *__ptr, int __val, int __flags);

extern void lll_unlock (void *__ptr, int __flags);

extern void lll_robust_unlock (void *__ptr, int __flags);

extern void lll_requeue (void *__src, void *__dst,
  int __wake_one, int __flags);

/* The following are hacks that allow us to simulate optional
 * parameters in C, to avoid having to pass the clock id for
 * every one of these calls. */

#define lll_abstimed_wait(ptr, val, tsp, flags, ...)   \
  ({   \
     const clockid_t __clk[] = { CLOCK_REALTIME, ##__VA_ARGS__ };   \
     __lll_abstimed_wait ((ptr), (val), (tsp), (flags),   \
       __clk[sizeof (__clk) / sizeof (__clk[0]) - 1]);   \
   })

#define lll_abstimed_xwait(ptr, lo, hi, tsp, flags, ...)   \
  ({   \
     const clockid_t __clk[] = { CLOCK_REALTIME, ##__VA_ARGS__ };   \
     __lll_abstimed_xwait ((ptr), (lo), (hi), (tsp), (flags),   \
       __clk[sizeof (__clk) / sizeof (__clk[0]) - 1]);   \
   })

#define lll_abstimed_lock(ptr, tsp, flags, ...)   \
  ({   \
     const clockid_t __clk[] = { CLOCK_REALTIME, ##__VA_ARGS__ };   \
     __lll_abstimed_lock ((ptr), (tsp), (flags),   \
       __clk[sizeof (__clk) / sizeof (__clk[0]) - 1]);   \
   })

#define lll_robust_abstimed_lock(ptr, tsp, flags, ...)   \
  ({   \
     const clockid_t __clk[] = { CLOCK_REALTIME, ##__VA_ARGS__ };   \
     __lll_robust_abstimed_lock ((ptr), (tsp), (flags),   \
       __clk[sizeof (__clk) / sizeof (__clk[0]) - 1]);   \
   })

#endif
