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
#include "lowlevellock.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

typedef struct atfork
{
  void (*prepare) (void);
  void (*parent) (void);
  void (*child) (void);
  void *dso;
  struct atfork *prev;
  struct atfork *next;
} atfork_t;

static atfork_t *atfork_handlers;
static atfork_t *atfork_last;
static unsigned int atfork_lock;

#ifdef SHARED
#  define C_CONST
#else
#  define C_CONST   const
#endif

#define defsymbol(set, sym)   \
  static const void * C_CONST __elf_set_##set##_element_##sym##__   \
    __attribute__ ((used, section (#set))) = &(sym)

#define atfork_sym(type, fct)   \
  defsymbol (_hurd_atfork_##type##_hook, fct)

static void
pt_atfork_prepare (void)
{
  atfork_t *hp, *lastp;

  lll_lock (&atfork_lock, 0);
  hp = atfork_handlers;
  lastp = atfork_last;
  lll_unlock (&atfork_lock, 0);

  /* Prepare handlers are executed in reverse order. */
  if (lastp)
    {
      while (1)
        {
          if (lastp->prepare != NULL)
            lastp->prepare ();
          if (lastp == hp)
            break;

          lastp = lastp->prev;
        }
    }

  /* No more threads added or removed, until the child is created. */
  lll_lock (&__running_threads_lock, 0);
}

atfork_sym (prepare, pt_atfork_prepare);

static void
pt_atfork_parent (void)
{
  atfork_t *hp;

  lll_lock (&atfork_lock, 0);
  hp = atfork_handlers;
  lll_unlock (&atfork_lock, 0);

  for (; hp != NULL; hp = hp->next)
    if (hp->parent != NULL)
      hp->parent ();

  /* Thread creation and destruction allowed once again. */
  lll_unlock (&__running_threads_lock, 0);
}

atfork_sym (parent, pt_atfork_parent);

static void
pt_atfork_child (void)
{
  /* In the child, we have to re-initialize the whole lib. */
  struct pthread *self = PTHREAD_SELF;

  /* Reset the list lock. */
  __running_threads_lock = 0;

  /* The child starts with one pthread. */
  __pthread_total = 1;

  /* Set the ID to one, like all main threads. */
  self->id = __pthread_id_counter = 1;

  /* Clear global keys and misc variables. */
  memset (__pthread_keys, 0,
    PTHREAD_KEYS_MAX * sizeof (__pthread_keys[0]));
  __pthread_concurrency = 0;
  __pthread_mtflag = 0;

  /* Deallocate every stack but this thread's. */
  __pthread_free_stacks (self);

  /* Reset the list with this single thread. */
  hurd_list_init (&__running_threads);
  hurd_list_add_tail (&__running_threads, &self->link);

  /* Finally, execute the child handlers and clear them. */
  atfork_t *hp;
  for (hp = atfork_handlers; hp != NULL; )
    {
      atfork_t *tmp = hp->next;
      if (hp->child != NULL)
        hp->child ();
      free (hp);
      hp = tmp;
    }

  atfork_lock = 0;
  atfork_handlers = atfork_last = NULL;
}

atfork_sym (child, pt_atfork_child);

/* The current DSO. */
extern void *__dso_handle
  __attribute__ ((__weak__, __visibility__ ("hidden")));

int pthread_atfork (void (*prepare) (void),
  void (*parent) (void), void (*child) (void))
{
  atfork_t *hp = (atfork_t *)malloc (sizeof (*hp));
  if (!hp)
    return (ENOMEM);

  hp->prepare = prepare;
  hp->parent = parent;
  hp->child = child;
  hp->dso = &__dso_handle == NULL ? NULL : __dso_handle;

  lll_lock (&atfork_lock, 0);

  if ((hp->next = atfork_handlers) != NULL)
    atfork_handlers->prev = hp;
  atfork_handlers = hp;

  if (!atfork_last)
    atfork_last = hp;

  lll_unlock (&atfork_lock, 0);
  return (0);
}

void unregister_atfork (void *dso)
{
  atfork_t *hp;

  lll_lock (&atfork_lock, 0);

  for (hp = atfork_handlers; hp != NULL; )
    {
      /* Skip this node if the DSO doesn't match. */
      if (hp->dso != dso)
        continue;

      /* Unlink the node from the chain. Be careful, since
       * this isn't a circular list. */
      atfork_t *tmp = hp->next;

      *(hp->prev ? &hp->prev->next : &atfork_handlers) = tmp;
      if (hp->next != NULL)
        hp->next->prev = hp->prev;

      free (hp);
      hp = tmp;
    }

  lll_unlock (&atfork_lock, 0);
}

