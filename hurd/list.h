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

#ifndef __HURD_LIST_H__
#define __HURD_LIST_H__   1

/* Basic double-linked list, designed to be embedded within types. */

struct hurd_list
{
  struct hurd_list *prev;
  struct hurd_list *next;
};

#define HURD_LIST_DECL(name)   \
  struct hurd_list name = { &name, &name }

#define hurd_list_init(lp)   \
  (void)   \
    ({   \
      struct hurd_list *__lp = (lp);   \
      __lp->next = __lp->prev = __lp;   \
    })   \

#define hurd_list_empty_p(lp)   \
  ((lp)->next == (lp))

#define hurd_list_entry(lp, type, mem)   \
  ((type *)((char *)(lp) - __builtin_offsetof (type, mem)))

#define hurd_list_add(prevp, nextp, nodep)   \
  (void)   \
    ({   \
      struct hurd_list *__prev = (prevp);   \
      struct hurd_list *__next = (nextp);   \
      struct hurd_list *__node = (nodep);   \
      \
      __next->prev = __node;   \
      __node->next = __next;   \
      \
      __prev->next = __node;   \
      __node->prev = __prev;   \
    })   \

#define hurd_list_add_head(listp, nodep)   \
  hurd_list_add ((listp), (listp)->next, (nodep))

#define hurd_list_add_tail(listp, nodep)   \
  hurd_list_add ((listp)->prev, (listp), (nodep))

#define hurd_list_del(nodep)   \
  (void)   \
    ({   \
      struct hurd_list *__node = (nodep);   \
      __node->prev->next = __node->next;   \
      __node->next->prev = __node->prev;   \
    })   \

#define hurd_list_end_p(listp, nodep)   ((listp) == (nodep))

#define hurd_list_each(listp, nodep)   \
  for (nodep = (listp)->next; nodep != (listp); nodep = nodep->next)

#endif
