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

#ifndef __SYSDEPS_I386_H__
#define __SYSDEPS_I386_H__   1

/* Dynamic thread vector. */
typedef union
{
  size_t counter;
  struct
    {
      void *val;
      bool is_static;
    } pointer;
} dtv_t;

/* Thread control block. Ideally should be exported by glibc,
 * but for now, we'll just copy the definition. We are using
 * the slot 'private_futex' as a pointer to the pthread descriptor,
 * since private futexes are always implemented for gnumach. */
typedef struct
{
  void *tcb;
  dtv_t *dtv;
  unsigned long self;
  int multiple_threads;
  unsigned long sysinfo;
  unsigned long stack_guard;
  unsigned long pointer_guard;
  int gscope_flag;
  struct pthread *thrdesc;
  void *__private_tm[4];
  void *__private_ss;
  unsigned long reply_port;
  struct hurd_sigstate *_hurd_sigstate;
} tcbhead_t;

/* Access a pthread's signal state. */
#define __pthread_sigstate(pt)   \
  ((tcbhead_t *)(pt)->tcb)->_hurd_sigstate

/* Get the stack pointer from a 'jmp_buf'. */
#define JBUF_SP(env)   ((env)->__jmpbuf[4])

/* Get a pointer to the thread control block. For the main
 * thread, this is allocated by ld.so; for other threads,
 * we allocate them ourselves. */
#define GET_TCB()   \
  ({   \
     tcbhead_t *__tp;   \
     __asm__ ("movl %%gs:%c1, %0" : "=r" (__tp)   \
       : "i" (__builtin_offsetof (tcbhead_t, tcb)));   \
     __tp;   \
   })

/* The pthread descriptor is stored inside the TCB. */
#define PTHREAD_SELF   \
  ({   \
     struct pthread *__self;   \
     __asm__ ("movl %%gs:%c1, %0" : "=r" (__self)   \
       : "i" (__builtin_offsetof (tcbhead_t, thrdesc)));   \
     __self;   \
   })

/* Set some members for the TCB. */
#define SETUP_TCB(tcbp, pt, ktid)   \
  (void)   \
    ({   \
       tcbhead_t *__tp = (tcbhead_t *)(tcbp);   \
       __tp->tcb = __tp;   \
       __tp->self = (ktid);   \
       __tp->thrdesc = (pt);   \
     })

#undef atomic_cas_bool

/* On x86, we can get away with using a weak CAS. */
#define atomic_cas_bool(ptr, exp, nval)   \
  ({   \
     typeof (exp) __e = (exp);   \
     __atomic_compare_exchange_n ((ptr), &__e, (nval), 1,   \
       __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);   \
   })

/* Atomic operations for 64-bit values. */

#if __SSE2__

  /* When SSE is enabled, we can use one of its registers
   * to move around 64-bit values. */

#  define atomic_loadq(ptr)   \
    ({   \
       typeof (*ptr) __R;   \
       __asm__ __volatile__   \
         (   \
           "movq %1, %0"   \
           : "=x" (__R) : "m" (*ptr) : "memory"   \
         );   \
       __R;   \
     })

#  define atomic_storeq(dst, src)   \
    ({   \
       __asm__ __volatile__   \
         (   \
           "movq %1, %0"   \
           : "=m" (*dst) : "x" (src) : "memory"   \
         );   \
       (void)0;   \
     })

#else

  /* If we can't use SSE, then we have to use floating
   * point operations to guarantee atomicity. CMPXCHG8B can
   * also be used, but it's slower and messier. */

  /* XXX: What should we do with the floating point status word?
   * Should we save it and restore it on each call? */

#  define atomic_loadq(ptr)   \
    ({   \
       union { double __d; unsigned long long __q; }   \
         __U = { *(const double *)(ptr) };   \
       __U.__q;   \
     })

#  define atomic_storeq(dst, src)   \
    ({   \
       union { unsigned long long __q; double __d; } __U = { (src) };   \
       *(double *)(dst) = __U.__d;   \
       (void)0;   \
     })

#endif

/* Atomically compare *PTR with the values <ELO, EHI>, and
 * swap them with <NLO, NHI> if equal. Returns nonzero if
 * the operation succeeded, zero otherwise. */

#if defined (__PIC__) && __GNUC__ < 5

  /* When PIC is turned on, the %ebx register is used to save
   * the value of the GOT pointer. As such, we must preserve
   * %ebx across calls. */

  /* Note that the above doesn't apply anymore for GCC 5, which
   * doesn't need to preserve %ebx across calls. */

#  ifdef __OPTIMIZE__

#    define atomic_casq_bool(ptr, elo, ehi, nlo, nhi)   \
      ({   \
         long __S, __V = (nlo);   \
         char __R;   \
         __asm__ __volatile__   \
           (   \
             "movl %%ebx, %2\n\t"   \
             "leal %0, %%edi\n\t"   \
             "movl %7, %%ebx\n\t"   \
             "lock; cmpxchg8b (%%edi)\n\t"   \
             "movl %2, %%ebx\n\t"   \
             "setz %1"   \
             : "=m" (*ptr), "=a" (__R), "=m" (__S)   \
             : "m" (*ptr), "d" (ehi), "a" (elo),   \
               "c" (nhi), "m" (__V)   \
             : "%edi", "memory"   \
           );   \
         (int)__R;   \
       })

#  else

    /* When using -O0, we have to preserve %edi, otherwise it
     * fails while finding a register in class 'GENERAL_REGS'. */

#    define atomic_casq_bool(ptr, elo, ehi, nlo, nhi)   \
      ({   \
         long __D, __S, __V = (nlo);   \
         char __R;   \
         __asm__ __volatile__   \
           (   \
             "movl %%edi, %3\n\t"   \
             "movl %%ebx, %2\n\t"   \
             "leal %0, %%edi\n\t"   \
             "movl %8, %%ebx\n\t"   \
             "lock; cmpxchg8b (%%edi)\n\t"   \
             "movl %2, %%ebx\n\t"   \
             "movl %3, %%edi\n\t"   \
             "setz %1"   \
             : "=m" (*ptr), "=a" (__R), "=m" (__S), "=m" (__D)   \
             : "m" (*ptr), "d" (ehi), "a" (elo),   \
               "c" (nhi), "m" (__V)   \
             : "memory"   \
           );   \
         (int)__R;   \
       })

#  endif

#else

  /* In non-PIC mode, we can use %ebx to load the
   * lower word in NLO. */

#  define atomic_casq_bool(ptr, elo, ehi, nlo, nhi)   \
    ({   \
       char __R;   \
       __asm__ __volatile__   \
         (   \
           "lock; cmpxchg8b %0\n\t"   \
           "setz %1"   \
           : "+m" (*ptr), "=a" (__R)   \
           : "d" (ehi), "a" (elo),   \
             "c" (nhi), "b" (nlo)   \
           : "memory"   \
         );   \
       (int)__R;   \
     })

#endif

#ifdef __need_pthread_machine

#include <mach.h>
#include <mach/i386/thread_status.h>
#include <mach/i386/mach_i386.h>
#include <mach/mig_errors.h>
#include <mach/thread_status.h>

#define HURD_TLS_DESC_DECL(desc, tcb)   \
  struct descriptor desc =   \
    {   \
      0xffff |   \
      (((unsigned int)(tcb)) << 16),   \
      \
      ((((unsigned int)(tcb)) >> 16) & 0xff) |   \
      ((0x12 | 0x60 | 0x80) << 8) |   \
      (0xf << 16) | ((4 | 8) << 20) |   \
      (((unsigned int)(tcb)) & 0xff000000)   \
    }

/* Given a thread port that has been currently suspended,
 * modify its register set. For this implementation, we
 * only need to change the stack pointer, program counter
 * and thread-specific register. */

static inline int
__pthread_set_ctx (mach_port_t ktid, int set_ip,
  void *ip, int set_sp, void *sp, int set_tp, void *tp)
{
  struct i386_thread_state state;
  mach_msg_type_number_t cnt = i386_THREAD_STATE_COUNT;
  int ret = thread_get_state (ktid, i386_REGS_SEGS_STATE,
    (thread_state_t)&state, &cnt);

  if (ret != 0)
    return (ret);

  if (set_ip)
    state.eip = (unsigned long)ip;
  if (set_sp)
    state.uesp = (unsigned long)sp;
  if (set_tp)
    {
      HURD_TLS_DESC_DECL (desc, tp);
      int sel;

      __asm__ ("mov %%gs, %w0" : "=q" (sel) : "0" (0));
      if (sel & 4)
        ret = __i386_set_ldt (ktid, sel, &desc, 1);
      else
        ret = __i386_set_gdt (ktid, &sel, desc);

      if (ret)
        return (ret);

      state.gs = sel;
    }

  return (thread_set_state (ktid, i386_REGS_SEGS_STATE,
    (thread_state_t)&state, i386_THREAD_STATE_COUNT));
}

#define STACK_ALIGNMENT   16

static inline int
__pthread_set_machine_state (struct pthread *pt,
  void (*fct) (struct pthread *))
{
  /* Place the thread stack directly below the descriptor. */
  unsigned long *up = (unsigned long *)
    ((unsigned long)pt & ~(STACK_ALIGNMENT - 1));

  *--up = (unsigned long)pt;   /* Function argument. */
  *--up = 0;   /* (Fake) return address. */

  return (__pthread_set_ctx (__pthread_kport (pt),
    1, (void *)fct, 1, up, 1, pt->tcb));
}

#endif   /* __need_pthread_machine */

#endif
