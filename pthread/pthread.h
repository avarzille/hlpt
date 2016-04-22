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

#ifndef __PTHREAD_H__
#define __PTHREAD_H__   1

#undef _PTHREAD_H
#define _PTHREAD_H   1

#include <hurd/qval.h>
#include <time.h>
#include <sched.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* Thread descriptor type. */
typedef volatile void* pthread_t;

/* Thread attributes. */
typedef struct
{
  void *__stack;
  unsigned long __stacksize;
  unsigned long __guardsize;
  int __flags;
  struct
    {
      int __policy;
      int __data;
    } __sched;
} pthread_attr_t;

/* Various flags that may be set in pthread attributes. */
enum
{
  PTHREAD_CREATE_JOINABLE = 0x1,
#define PTHREAD_CREATE_JOINABLE   PTHREAD_CREATE_JOINABLE
  PTHREAD_CREATE_DETACHED = 0x2,
#define PTHREAD_CREATE_DETACHED   PTHREAD_CREATE_DETACHED
  PTHREAD_INHERIT_SCHED   = 0x4,
#define PTHREAD_INHERIT_SCHED     PTHREAD_INHERIT_SCHED
  PTHREAD_EXPLICIT_SCHED  = 0x8,
#define PTHREAD_EXPLICIT_SCHED    PTHREAD_EXPLICIT_SCHED
  PTHREAD_SCOPE_SYSTEM    = 0x10,
#define PTHREAD_SCOPE_SYSTEM      PTHREAD_SCOPE_SYSTEM
  PTHREAD_SCOPE_PROCESS   = 0x20
#define PTHREAD_SCOPE_PROCESS     PTHREAD_SCOPE_PROCESS
};

/* Initialize thread attributes ATTRP. */
extern int pthread_attr_init (pthread_attr_t *__attrp) 
  __THROW __nonnull ((1));

/* Set the stack address in ATTRP to STACKP. */
extern int pthread_attr_setstackaddr (pthread_attr_t *__attrp,
  void *__stackp) __THROW __nonnull ((1));

/* Get the stack address for ATTRP in *OUTP. */
extern int pthread_attr_getstackaddr (const pthread_attr_t *__attrp,
  void **__outp) __THROW __nonnull ((1, 2));

/* Set the stack size in ATTRP to SIZE bytes. */
extern int pthread_attr_setstacksize (pthread_attr_t *__attrp,
  size_t __size) __THROW __nonnull ((1));

/* Get the stack size in bytes for ATTRP in *OUTP. */
extern int pthread_attr_getstacksize (const pthread_attr_t *__attrp,
  size_t *__outp) __THROW __nonnull ((1, 2));

/* Set the stack and stack size in ATTRP to <STACKP, SIZE>. */
extern int pthread_attr_setstack (pthread_attr_t *__attrp,
  void *__stackp, size_t __size) __THROW __nonnull ((1));

/* Get the stack for ATTRP in *OUTSTKP and the stack size
 * in bytes in *OUTSZP. */
extern int pthread_attr_getstack (const pthread_attr_t *__attrp,
  void **__outstkp, size_t *__outszp) __THROW __nonnull ((1, 2, 3));

/* Set the detach state in ATTRP to STATE. */
extern int pthread_attr_setdetachstate (pthread_attr_t *__attrp,
  int __state) __THROW __nonnull ((1));

/* Get the detach state for ATTRP in *OUTP. */
extern int pthread_attr_getdetachstate (const pthread_attr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Set the guard size in ATTRP to SIZE bytes. */
extern int pthread_attr_setguardsize (pthread_attr_t *__attrp,
  size_t __size) __THROW __nonnull ((1));

/* Get the guard size in bytes for ATTRP in *OUTP. */
extern int pthread_attr_getguardsize (const pthread_attr_t *__attrp,
  size_t *__outp) __THROW __nonnull ((1, 2));

/* Set the thread scope in ATTRP to SCOPE. */
extern int pthread_attr_setscope (pthread_attr_t *__attrp,
  int __scope) __THROW __nonnull ((1));

/* Get the thread scope for ATTRP in *OUTP. */
extern int pthread_attr_getscope (const pthread_attr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Set the scheduling inheritance in ATTRP to INHERIT. */
extern int pthread_attr_setinheritsched (pthread_attr_t *__attrp,
  int __inherit) __THROW __nonnull ((1));

/* Get the scheduling inheritance for ATTRP in *OUTP. */
extern int pthread_attr_getinheritsched (const pthread_attr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Set the scheduling parameter in ATTRP to *PARMP. */
extern int pthread_attr_setschedparam (pthread_attr_t *__attrp,
  const struct sched_param *__parmp) __THROW __nonnull ((1, 2));

/* Get the scheduling parameter for ATTRP in *OUTP. */
extern int pthread_attr_getschedparam (const pthread_attr_t *__attrp,
  struct sched_param *__outp) __THROW __nonnull ((1, 2));

/* Set the scheduling policy in ATTRP to POLICY. */
extern int pthread_attr_setschedpolicy (pthread_attr_t *__attrp,
  int __policy) __THROW __nonnull ((1));

/* Get the scheduling policy for ATTRP in *OUTP. */
extern int pthread_attr_getschedpolicy (const pthread_attr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Get the thread attributes for THR in *ATTRP. */
extern int pthread_getattr_np (pthread_t __thr,
  pthread_attr_t *__attrp) __THROW __nonnull ((2));

/* Destroy thread attributes ATTRP. */
extern int pthread_attr_destroy (pthread_attr_t *__attrp)
  __THROW __nonnull ((1));

/* Create a new thread using the attributes in ATTRP if non-null,
 * and have it execute the function START_FCT with argument ARGP.
 * Store the thread descriptor in *THRP. */
extern int pthread_create (pthread_t *__thrp, 
  const pthread_attr_t *__attrp,
  void* (*__start_fct) (void *), void *__argp)
    __THROWNL __nonnull ((1, 3));

/* Return the descriptor for the calling thread. */
extern pthread_t pthread_self (void) __THROW __attribute__ ((const));

/* Join the calling thread with THR, waiting for it to finish.
 * If RETP is non-null, store the return value in it. This function
 * is a cancellation point. */
extern int pthread_join (pthread_t __thr, void **__retp);

/* Like 'pthread_join', but only wait until TSP elapses. This function
 * is a cancellation point. */
extern int pthread_timedjoin_np (pthread_t __thr,
  void **__retp, const struct timespec *__tsp) __nonnull ((3));

/* Like 'pthread_join', but don't block if THR hasn't finished. */
extern int pthread_tryjoin_np (pthread_t __thr, void **__retp) __THROW;

/* Make THR detached, so that its resources are deallocated
 * immediately upon exit, and no thread may join with it. */
extern int pthread_detach (pthread_t __thr) __THROW;

/* Terminate calling thread, and have its return value be RETVAL. */
extern void pthread_exit (void *__retval) __attribute__ ((noreturn));

/* Set the global concurrency level to LVL. */
extern int pthread_setconcurrency (int __lvl) __THROW;

/* Get the global concurrency level. */
extern int pthread_getconcurrency (void) __THROW;

/* Set the scheduling parameter for thread TH to *PARMP. */
extern int pthread_setschedparam (pthread_t __th,
  const struct sched_param *__parmp) __THROW __nonnull ((2));

/* Get the scheduling parameter for thread TH in *PARMP. */
extern int pthread_getschedparam (pthread_t __th,
  struct sched_param *__parmp) __THROW __nonnull ((2));

/* Set the scheduling priority for thread TH to PRIO. */
extern int pthread_setschedprio (pthread_t __th, int __prio) __THROW;

/* Process shared flags. */
enum
{
  PTHREAD_PROCESS_PRIVATE = 0,
#define PTHREAD_PROCESS_PRIVATE   PTHREAD_PROCESS_PRIVATE
  PTHREAD_PROCESS_SHARED
#define PTHREAD_PROCESS_SHARED    PTHREAD_PROCESS_SHARED
};

/* Mutexes. */

typedef struct
{
  unsigned int __lock;
  unsigned int __owner_id;
  unsigned int __cnt;
  int __extra;
  int __type;
  int __flags;
} pthread_mutex_t;

typedef struct
{
  int __flags;
  int __type;
} pthread_mutexattr_t;

/* Mutex types. */
enum
{
  PTHREAD_MUTEX_NORMAL,
#define PTHREAD_MUTEX_NORMAL       PTHREAD_MUTEX_NORMAL
  PTHREAD_MUTEX_RECURSIVE,
#define PTHREAD_MUTEX_RECURSIVE    PTHREAD_MUTEX_RECURSIVE
  PTHREAD_MUTEX_ERRORCHECK,
#define PTHREAD_MUTEX_ERRORCHECK   PTHREAD_MUTEX_ERRORCHECK
  PTHREAD_MUTEX_TYPE_MAX,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_DEFAULT      PTHREAD_MUTEX_DEFAULT
};

/* Mutex robustness. */
enum
{
  PTHREAD_MUTEX_STALLED,
#define PTHREAD_MUTEX_STALLED   PTHREAD_MUTEX_STALLED
  PTHREAD_MUTEX_ROBUST = 0x100
#define PTHREAD_MUTEX_ROBUST    PTHREAD_MUTEX_ROBUST
};

/* Mutex protocol. */
enum
{
  PTHREAD_PRIO_NONE,
#define PTHREAD_PRIO_NONE      PTHREAD_PRIO_NONE
  PTHREAD_PRIO_INHERIT,
#define PTHREAD_PRIO_INHERIT   PTHREAD_PRIO_INHERIT
  PTHREAD_PRIO_PROTECT
#define PTHREAD_PRIO_PROTECT   PTHREAD_PRIO_PROTECT
};

/* Additional codes for robust locks. */
#define EOWNERDEAD        1073741945
#define ENOTRECOVERABLE   1073741946

/* Static mutex initializers. */

#define PTHREAD_MUTEX_INITIALIZER   \
  { 0, 0, 0, 0, PTHREAD_MUTEX_NORMAL, 0 }

#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP   \
  { 0, 0, 0, 0, PTHREAD_MUTEX_RECURSIVE, 0 }

#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP   \
  { 0, 0, 0, 0, PTHREAD_MUTEX_ERRORCHECK, 0 }

/* Initialize mutex attributes ATTRP. */
extern int pthread_mutexattr_init (pthread_mutexattr_t *__attrp)
  __THROW __nonnull ((1));

/* Set the mutex type in ATTRP to TYPE. */
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attrp,
  int __type) __THROW __nonnull ((1));

/* Get the mutex type for ATTRP in *TYPEP. */
extern int pthread_mutexattr_gettype (const pthread_mutexattr_t *__attrp,
  int *__typep) __THROW __nonnull ((1, 2));

/* Set the process-shared flag in ATTRP to PSHARED. */
extern int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attrp,
  int __pshared) __THROW __nonnull ((1));

/* Get the process-shared flag for ATTRP in *PSHP. */
extern int pthread_mutexattr_getpshared (const pthread_mutexattr_t *__attrp,
  int *__pshp) __THROW __nonnull ((1, 2));

/* Set the robustness flag in ATTRP to ROBUST. */
extern int pthread_mutexattr_setrobust (pthread_mutexattr_t *__attrp,
  int __robust) __THROW __nonnull ((1));

/* Get the robustness flag for ATTRP in *ROBUSTP. */
extern int pthread_mutexattr_getrobust (const pthread_mutexattr_t *__attrp,
  int *__robustp) __THROW __nonnull ((1, 2));

/* Set the priority ceiling in ATTRP to CEILING. */
extern int pthread_mutexattr_setprioceiling (pthread_mutexattr_t *__attrp,
  int __ceiling) __THROW __nonnull ((1));

/* Get the priority ceiling for AP in *OUTP. */
extern int pthread_mutexattr_getprioceiling (const pthread_mutexattr_t *__ap,
  int *__outp) __THROW __nonnull ((1, 2));

/* Set the mutex protocol in ATTRP to PROTOCOL. */
extern int pthread_mutexattr_setprotocol (pthread_mutexattr_t *__attrp,
  int __protocol) __THROW __nonnull ((1));

/* Get the mutex protocol for ATTRP in *OUTP. */
extern int pthread_mutexattr_getprotocol (const pthread_mutexattr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Destroy mutex attributes ATTRP. */
extern int pthread_mutexattr_destroy (pthread_mutexattr_t *__attrp)
  __THROW __nonnull ((1));

/* Initialize mutex MTXP with attributes ATTRP, if non-null. */
extern int pthread_mutex_init (pthread_mutex_t *__mtxp,
  const pthread_mutexattr_t *__attrp) __THROW __nonnull ((1));

/* Lock the mutex MTXP. */
extern int pthread_mutex_lock (pthread_mutex_t *__mtxp)
  __THROWNL __nonnull ((1));

/* Like 'pthread_mutex_lock', but only wait until TSP elapses. */
extern int pthread_mutex_timedlock (pthread_mutex_t *__mtxp,
  const struct timespec *__tsp) __THROWNL __nonnull ((1, 2));

/* Try to lock mutex MTXP without blocking. */
extern int pthread_mutex_trylock (pthread_mutex_t *__mtxp)
  __THROWNL __nonnull ((1));

/* Unlock mutex MTXP. */
extern int pthread_mutex_unlock (pthread_mutex_t *__mtxp)
  __THROWNL __nonnull ((1));

/* Mark the mutex MTXP as consistent. */
extern int pthread_mutex_consistent (pthread_mutex_t *__mtxp)
  __THROW __nonnull ((1));

/* Transfer ownership of mutex MTXP to thread THR. The
 * calling thread must own the mutex. */
extern int pthread_mutex_transfer_np (pthread_mutex_t *__mtxp,
  pthread_t __thr) __THROW __nonnull ((1));

/* Set the priority ceiling for MTXP to CEILING. Return the
 * previous value in *PREVP. */
extern int pthread_mutex_setprioceiling (pthread_mutex_t *__mtxp,
  int __ceiling, int *__prevp) __THROW __nonnull ((1, 3));

/* Get the priority ceiling for MTXP in *OUTP. */
extern int pthread_mutex_getprioceiling (const pthread_mutex_t *__mtxp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Destroy the mutex MTXP. */
extern int pthread_mutex_destroy (pthread_mutex_t *__mtxp)
  __THROW __nonnull ((1));

/* Condition variables. */

typedef struct
{
  union hurd_qval __sw;
  pthread_mutex_t *__mutex;
  int __flags;
} pthread_cond_t;

typedef struct
{
  int __flags;
} pthread_condattr_t;

/* Static condvar initializer. */
#define PTHREAD_COND_INITIALIZER   \
  { { 0 }, 0, CLOCK_REALTIME << 16 }

/* Initialize condvar attributes ATTRP. */
extern int pthread_condattr_init (pthread_condattr_t *__attrp)
  __THROW __nonnull ((1));

/* Set the process-shared flag in ATTRP to PSHARED. */
extern int pthread_condattr_setpshared (pthread_condattr_t *__attrp,
  int __pshared) __THROW __nonnull ((1));

/* Get the process-shared flag for ATTRP in *OUTP. */
extern int pthread_condattr_getpshared (const pthread_condattr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Set the clock-id in ATTRP to CLK. */
extern int pthread_condattr_setclock (pthread_condattr_t *__attrp,
  clockid_t __clk) __THROW __nonnull ((1));

/* Get the clock-id for ATTRP in *OUTP. */
extern int pthread_condattr_getclock (const pthread_condattr_t *__attrp,
  clockid_t *__outp) __THROW __nonnull ((1, 2));

/* Destroy the condvar attributes ATTRP. */
extern int pthread_condattr_destroy (pthread_condattr_t *__attrp)
  __THROW __nonnull ((1));

/* Initialize the condvar CONDP with attributes ATTRP, if non-null. */
extern int pthread_cond_init (pthread_cond_t *__condp,
  const pthread_condattr_t *__attrp) __THROW __nonnull ((1));

/* Wait for condition CONDP to be signaled. The mutex MTXP is held
 * on entry, released while waiting and reacquired on exit. This function
 * is a cancellation point. */
extern int pthread_cond_wait (pthread_cond_t *__condp, 
  pthread_mutex_t *__mtxp) __nonnull ((1, 2));

/* Like 'pthread_cond_wait', but only wait until TSP elapses. */
extern int pthread_cond_timedwait (pthread_cond_t *__condp,
  pthread_mutex_t *__mtxp, const struct timespec *__tsp)
  __nonnull ((1, 2, 3));
  
/* The following behave like the above functions, except they are not
 * cancellation points; instead, they allow the calling thread to be
 * asynchronously interrupted and return EINTR in such a case, but
 * without reacquiring the mutex. */
extern int pthread_hurd_cond_wait_np (pthread_cond_t *__condp,
  pthread_mutex_t *__mtxp) __nonnull ((1, 2));

extern int pthread_hurd_cond_timedwait_np (pthread_cond_t *__condp,
  pthread_mutex_t *__mtxp, const struct timespec *__tsp)
  __nonnull ((1, 2, 3));

/* Wake one of the threads waiting on condvar CONDP. */
extern int pthread_cond_signal (pthread_cond_t *__condp)
  __THROWNL __nonnull ((1));

/* Wake all of the threads waiting on condvar CONDP. */
extern int pthread_cond_broadcast (pthread_cond_t *__condp)
  __THROWNL __nonnull ((1));

/* Destroy condvar CONDP. */
extern int pthread_cond_destroy (pthread_cond_t *__condp)
  __THROW __nonnull ((1));

/* Read-write locks. */

typedef struct
{
  union hurd_qval __pid_nw;
  union hurd_qval __oid_nr;
  int __flags;
} pthread_rwlock_t;

typedef struct
{
  int __flags;
} pthread_rwlockattr_t;

/* Static read-write lock initializer. */
#define PTHREAD_RWLOCK_INITIALIZER   { { 0 }, { 0 }, 0 }

/* Initialize read-write lock attributes ATTRP. */
extern int pthread_rwlockattr_init (pthread_rwlockattr_t *__attrp)
  __THROW __nonnull ((1));

/* Set the process-shared flag in ATTRP to PSHARED. */
extern int pthread_rwlockattr_setpshared (pthread_rwlockattr_t *__attrp,
  int __pshared) __THROW __nonnull ((1));

/* Get the process-shared flag for ATTRP in *OUTP. */
extern int pthread_rwlockattr_getpshared (const pthread_rwlockattr_t *__attrp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Destroy the read-write lock attributes ATTRP. */
extern int pthread_rwlockattr_destroy (pthread_rwlockattr_t *__attrp)
  __THROW __nonnull ((1));

/* Initialize read-write lock RWP with attributes ATTRP, if non-null. */
extern int pthread_rwlock_init (pthread_rwlock_t *__rwp,
  const pthread_rwlockattr_t *__attrp) __THROW __nonnull ((1));

/* Acquire read lock for RWP. */
extern int pthread_rwlock_rdlock (pthread_rwlock_t *__rwp)
  __THROWNL __nonnull ((1));

/* Try to acquire read lock for RWP without blocking. */
extern int pthread_rwlock_tryrdlock (pthread_rwlock_t *__rwp)
  __THROWNL __nonnull ((1));

/* Like 'pthread_rwlock_rdlock', but only wait until TSP elapses. */
extern int pthread_rwlock_timedrdlock (pthread_rwlock_t *__rwp,
  const struct timespec *__tsp) __THROWNL __nonnull ((1, 2));

/* Acquire write lock for RWP. */
extern int pthread_rwlock_wrlock (pthread_rwlock_t *__rwp)
  __THROWNL __nonnull ((1));

/* Try to acquire write lock for RWP without blocking. */
extern int pthread_rwlock_trywrlock (pthread_rwlock_t *__rwp)
  __THROWNL __nonnull ((1));

/* Like 'pthread_rwlock_wrlock', but only wait until TSP elapses. */
extern int pthread_rwlock_timedwrlock (pthread_rwlock_t *__rwp,
  const struct timespec *__tsp) __THROWNL __nonnull ((1, 2));

/* Unlock read-write lock RWP. */
extern int pthread_rwlock_unlock (pthread_rwlock_t *__rwp)
  __THROWNL __nonnull ((1));

/* Destroy read-write lock RWP. */
extern int pthread_rwlock_destroy (pthread_rwlock_t *__rwp)
  __THROW __nonnull ((1));

/* Barriers. */

typedef struct
{
  int __flags;
} pthread_barrierattr_t;

typedef struct
{
  union hurd_qval __seq_cnt;
  unsigned int __nrefs;
  unsigned int __total;
  int __flags;
} pthread_barrier_t;

/* Special value returned to the last barrier waiter. */
#define PTHREAD_BARRIER_SERIAL_THREAD   1

/* Initialize barrier attributes ATTRP. */
extern int pthread_barrierattr_init (pthread_barrierattr_t *__attrp)
  __THROW __nonnull ((1));

/* Set the process-shared flag in ATTRP to PSHARED. */
extern int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attrp,
  int __pshared) __THROW __nonnull ((1));

/* Get the process-shared flag for ATTRP in *OUTP. */
extern int pthread_barrierattr_getpshared (const pthread_barrierattr_t *__atp,
  int *__outp) __THROW __nonnull ((1, 2));

/* Destroy barrier attributes ATTRP. */
extern int pthread_barrierattr_destroy (pthread_barrierattr_t *__attrp)
  __THROW __nonnull ((1));

/* Initialize barrier BARP with attributes ATTRP, if non-null, and
 * with a count of VAL. */
extern int pthread_barrier_init (pthread_barrier_t *__barp,
  const pthread_barrierattr_t *__attrp, unsigned int __val)
    __THROW __nonnull ((1));

/* Wait on barrier BARP. */
extern int pthread_barrier_wait (pthread_barrier_t *__barp)
  __THROWNL __nonnull ((1));

/* Destroy barrier BARP. */
extern int pthread_barrier_destroy (pthread_barrier_t *__barp)
  __THROW __nonnull ((1));

/* Spin-locks. */
typedef long int pthread_spinlock_t;

#define PTHREAD_SPINLOCK_INITIALIZER   0L

/* Initialize spinlock SP. If PSHARED is non-zero, it may be used
 * to synchronize threads in other processes. */
extern int pthread_spin_init (pthread_spinlock_t *__sp, int __pshared)
  __THROW __nonnull ((1));

/* Lock spinlock SP. */
extern int pthread_spin_lock (pthread_spinlock_t *__sp)
  __THROW __nonnull ((1));

/* Try to lock spinlock SP without spinning. */
extern int pthread_spin_trylock (pthread_spinlock_t *__sp)
  __THROW __nonnull ((1));

/* Unlock spinlock SP. */
extern int pthread_spin_unlock (pthread_spinlock_t *__sp)
  __THROW __nonnull ((1));

/* Destroy spinlock SP. */
extern int pthread_spin_destroy (pthread_spinlock_t *__sp)
  __THROW __nonnull ((1));

/* Once control. */
typedef int pthread_once_t;

#define PTHREAD_ONCE_INIT   0

/* Make sure that FCT is called only once, regardless of how many
 * times 'pthread_once' is called on the same once-control variable
 * CTP. This function may be a cancellation point, depending on
 * what FCT actually does. */
extern int pthread_once (pthread_once_t *__ctp, void (*__fct) (void))
  __nonnull ((1, 2));

/* Thread cancellation handling. */
enum
{
  PTHREAD_CANCEL_DISABLE,
#define PTHREAD_CANCEL_DISABLE        PTHREAD_CANCEL_DISABLE
  PTHREAD_CANCEL_ENABLE,
#define PTHREAD_CANCEL_ENABLE         PTHREAD_CANCEL_ENABLE
  PTHREAD_CANCEL_DEFERRED,
#define PTHREAD_CANCEL_DEFERRED       PTHREAD_CANCEL_DEFERRED
  PTHREAD_CANCEL_ASYNCHRONOUS,
#define PTHREAD_CANCEL_ASYNCHRONOUS   PTHREAD_CANCEL_ASYNCHRONOUS
};

/* Special value returned by threads that were cancelled. */
#define PTHREAD_CANCELED   ((void *)-1)

/* Set the calling thread's cancellation state to STATE. If non-null,
 * return the previous value in *PREVP. */
extern int pthread_setcancelstate (int __state, int *__prevp);

/* Set the calling thread's cancellation type to TYPE. If non-null,
 * return the previous value in *PREVP. */
extern int pthread_setcanceltype (int __type, int *__prevp);

/* Cancel thread THR. */
extern int pthread_cancel (pthread_t __thr);

/* Test if there's a pending cancellation request for the calling
 * thread. If so, exit immediately with return value 'PTHREAD_CANCELED'. */
extern void pthread_testcancel (void);

/* Cleanup handlers. */
struct __pthread_cleanup_buf
{
  void (*__fct) (void *);
  void *__argp;
  struct __pthread_cleanup_buf *__next;
};

extern struct __pthread_cleanup_buf** __pthread_cleanup_list (void);

/* Push a new cancellation handler with function FCT and argument ARGP. */
#define pthread_cleanup_push(fct, argp)   \
  do   \
    {   \
      struct __pthread_cleanup_buf **__linkp =   \
        __pthread_cleanup_list ();   \
      struct __pthread_cleanup_buf __handler =   \
        { .__fct = (fct), .__argp = (argp), .__next = *__linkp };   \
      *__linkp = &__handler

/* Pop the top cancellation handler. If EXEC is non-zero, execute
 * its associated routine with its argument. */
#define pthread_cleanup_pop(exec)   \
    *__linkp = __handler.__next;   \
    if (exec)   \
      __handler.__fct (__handler.__argp);   \
    }   \
  while (0)

/* Thread-specific data. */
typedef unsigned int pthread_key_t;

/* Create a key that describes a slot to store thread-specific data. If
 * non-null, DESTR will be called when the value associated to the key
 * is destroyed during thread exit. */
extern int pthread_key_create (pthread_key_t *__keyp,
  void (*__destr) (void *)) __THROW __nonnull ((1));

/* Store VALP in the thread-specific slot described by KEY for
 * the calling thread. */
extern int pthread_setspecific (pthread_key_t __key, 
  const void *__valp) __THROW;

/* Get the value in the thread-specific slot described by KEY for
 * the calling thread. */
extern void* pthread_getspecific (pthread_key_t __key);

/* Delete the key KEY. */
extern int pthread_key_delete (pthread_key_t __key);

/* Non-portable definitions. */

/* Yield the CPU to another thread or process. */
extern void pthread_yield (void) __THROW;

/* Get the kernel port for thread THR. */
extern unsigned long pthread_kport_np (pthread_t __thr) __THROW;

/* Get the signal state for thread THR. */
extern struct hurd_sigstate* pthread_sigstate_np (pthread_t __thr) __THROW;

/* For each thread that is currently running, execute the function
 * FCT with argument ARGP. If the function returns a value that is
 * less than zero, iteration is interrupted early. */
extern int pthread_foreach_np (int (*__fct) (pthread_t, void *), 
  void *__argp) __nonnull ((1));

/* Get the global id for thread THR. */
extern int pthread_gettid_np (pthread_t __thr, unsigned int *__outp)
  __THROW __nonnull ((2));

/* Suspend execution for thread THR. */
extern int pthread_suspend_np (pthread_t __thr) __THROW;

/* Resume execution for thread THR. */
extern int pthread_resume_np (pthread_t __thr) __THROW;

/* FIXME: Should be in <signal.h> */
extern int pthread_kill (pthread_t __thr, int __sig);

extern int pthread_sigmask (int __how,
  const unsigned long *__setp, unsigned long *__prevp);

__END_DECLS

#endif
