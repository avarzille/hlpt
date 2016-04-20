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
#include <errno.h>
#include <sched.h>
#include <mach_init.h>

int pthread_attr_init (pthread_attr_t *attrp)
{
  *attrp = __pthread_dfl_attr;
  return (0);
}

int pthread_attr_setstackaddr (pthread_attr_t *attrp, void *stack)
{
  attrp->__stack = stack;
  return (0);
}

int pthread_attr_getstackaddr (const pthread_attr_t *attrp, void **outp)
{
  *outp = attrp->__stack;
  return (0);
}

int pthread_attr_setstacksize (pthread_attr_t *attrp, size_t size)
{
  if (size < PTHREAD_STACK_MIN)
    return (EINVAL);

  attrp->__stacksize = size;
  return (0);
}

int pthread_attr_getstacksize (const pthread_attr_t *attrp, size_t *outp)
{
  /* If the user did not provide a stack size, we return
   * the system default. */
  *outp = attrp->__stacksize ?: __pthread_dfl_attr.__stacksize;
  return (0);
}

int pthread_attr_setstack (pthread_attr_t *attrp, void *stack, size_t size)
{
  if (size < PTHREAD_STACK_MIN)
    return (EINVAL);

  attrp->__stack = (char *)stack +
    (attrp->__stacksize = size);
  return (0);
}

int pthread_attr_getstack (const pthread_attr_t *attrp, 
  void **outp, size_t *outsz)
{
  *outp = (char *)attrp->__stack -
    (*outsz = attrp->__stacksize);
  return (0);
}

int pthread_attr_setdetachstate (pthread_attr_t *attrp, int state)
{
  if (state != PTHREAD_CREATE_JOINABLE &&
      state != PTHREAD_CREATE_DETACHED)
    return (EINVAL);

  attrp->__flags = state | (attrp->__flags &
    ~(PTHREAD_CREATE_DETACHED | PTHREAD_CREATE_JOINABLE));
  return (0);
}

int pthread_attr_getdetachstate (const pthread_attr_t *attrp, int *outp)
{
  *outp = attrp->__flags &
    (PTHREAD_CREATE_DETACHED | PTHREAD_CREATE_JOINABLE);
  return (0);
}

int pthread_attr_setguardsize (pthread_attr_t *attrp, size_t size)
{
  if (size < vm_page_size)
    return (EINVAL);

  attrp->__guardsize = size;
  return (0);
}

int pthread_attr_getguardsize (const pthread_attr_t *attrp, size_t *outp)
{
  *outp = attrp->__guardsize;
  return (0);
}

int pthread_attr_setscope (pthread_attr_t *attrp, int scope)
{
  (void)attrp;
  return (scope == PTHREAD_SCOPE_PROCESS ? ENOTSUP :
    scope != PTHREAD_SCOPE_SYSTEM ? EINVAL : 0);
}

int pthread_attr_getscope (const pthread_attr_t *attrp, int *outp)
{
  *outp = attrp->__flags &
    (PTHREAD_SCOPE_PROCESS | PTHREAD_SCOPE_SYSTEM);
  return (0);
}

int pthread_attr_setinheridsched (pthread_attr_t *attrp, int sched)
{
  (void)attrp;
  return (sched == PTHREAD_EXPLICIT_SCHED ? ENOTSUP :
    sched != PTHREAD_INHERIT_SCHED ? EINVAL : 0);
}

int pthread_attr_getinheritsched (const pthread_attr_t *attrp, int *outp)
{
  *outp = attrp->__flags &
    (PTHREAD_INHERIT_SCHED | PTHREAD_EXPLICIT_SCHED);
  return (0);
}

int pthread_attr_setschedparam (pthread_attr_t *attrp,
  const struct sched_param *parmp)
{
  attrp->__sched.__data = parmp->sched_priority;
  return (0);
}

int pthread_attr_getschedparam (const pthread_attr_t *attrp,
  struct sched_param *paramp)
{
  paramp->sched_priority = attrp->__sched.__data;
  return (0);
}

int pthread_attr_setschedpolicy (pthread_attr_t *attrp, int policy)
{
  (void)attrp;
  return (policy == SCHED_FIFO || policy == SCHED_RR ?
    ENOTSUP : policy != SCHED_OTHER ? EINVAL : 0);
}

int pthread_attr_getschedpolicy (const pthread_attr_t *attrp, int *outp)
{
  *outp = attrp->__sched.__policy;
  return (0);
}

int pthread_getattr_np (pthread_t th, pthread_attr_t *attrp)
{
  struct pthread *pt = (struct pthread *)th;
  if (INVALID_P (pt))
    return (ESRCH);

  attrp->__stack = pt->stack + 
    (attrp->__stacksize = pt->stacksize);
  attrp->__guardsize = pt->guardsize;
  attrp->__flags = DETACHED_P (pt) ?
    PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;

  /* We only support the following flags. */
  attrp->__flags |= PTHREAD_SCOPE_SYSTEM | PTHREAD_INHERIT_SCHED;
  attrp->__sched = __pthread_dfl_attr.__sched;

  return (0);
}

int pthread_attr_destroy (pthread_attr_t *attrp)
{
  (void)attrp;
  return (0);
}

