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

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__   1

#undef _SEMAPHORE_H
#define _SEMAPHORE_H   1

#include <hurd/qval.h>
#include <time.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* Maximum value that a semaphore may have. */
#ifndef SEM_VALUE_MAX
#  define SEM_VALUE_MAX   0x7FFFFFFF
#endif

/* POSIX semaphore type. */
typedef struct
{
  union hurd_qval __vw;
  int __flags;
} sem_t;

/* Special constant returned on failure. */
#define SEM_FAILED   ((sem_t *)0)

/* Initialize semaphore SEMP with count VAL. If PSHARED is non-zero,
 * it may be used to synchronize threads across processes. */
extern int sem_init (sem_t *__semp, int __pshared, unsigned int __val)
  __THROW __nonnull ((1));

/* Open a semaphore with name NAME and open flags FLAG. Depending on
 * the value of FLAG, additional parameters may be passed, indicating
 * the open mode and initial count value. */
extern sem_t* sem_open (const char *__name, int __flag, ...)
  __THROW __nonnull ((1));

/* Wait on semaphore SEMP. This function is a cancellation point. */
extern int sem_wait (sem_t *__semp) __nonnull ((1));

/* Try to acquire semaphore SEMP, without blocking. */
extern int sem_trywait (sem_t *__semp) __THROWNL __nonnull ((1));

/* Wait on semaphore until TSP elapses. 
 * This function is a cancellation point. */
extern int sem_timedwait (sem_t *__semp, const struct timespec *__tsp)
  __nonnull ((1, 2));

/* Increment the count of semaphore SEMP. If there are threads waiting
 * on, it, wake one of them. */
extern int sem_post (sem_t *__semp) __THROWNL __nonnull ((1));

/* Store the semaphore count of SEMP in *OUTP. */
extern int sem_getvalue (sem_t *__semp, int *__outp)
  __THROW __nonnull ((1, 2));

/* Destroy semaphore SEMP. */
extern int sem_destroy (sem_t *__semp) __THROW __nonnull ((1));

/* Close named semaphore SEMP. */
extern int sem_close (sem_t *__semp) __THROW __nonnull ((1));

/* Remove the semaphore with name NAME. */
extern int sem_unlink (const char *__name) __THROW __nonnull ((1));

__END_DECLS

#endif
