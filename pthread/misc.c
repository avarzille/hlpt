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
#include <errno.h>

pthread_t pthread_self (void)
{
  return (PTHREAD_SELF);
}

int pthread_equal (pthread_t t1, pthread_t t2)
{
  return (t1 == t2);
}

struct __pthread_cleanup_buf**
__pthread_cleanup_list (void)
{
  return (&PTHREAD_SELF->cleanup);
}

int pthread_setconcurrency (int lvl)
{
  if (lvl < 0)
    return (EINVAL);

  __pthread_concurrency = lvl;
  return (0);
}

int pthread_getconcurrency (void)
{
  return (__pthread_concurrency);
}

int pthread_setschedparam (pthread_t th,
  const struct sched_param *parmp)
{
  (void)th; (void)parmp;
  return (ENOSYS);
}

int pthread_getschedparam (pthread_t th,
  struct sched_param *parmp)
{
  (void)th; (void)parmp;
  return (ENOSYS);
}

int pthread_setschedprio (pthread_t th, int prio)
{
  (void)th; (void)prio;
  return (ENOSYS);
}

void pthread_yield (void)
{
  sched_yield ();
}

