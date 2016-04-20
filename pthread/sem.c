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

#include "semaphore.h"
#include "pt-internal.h"
#include "lowlevellock.h"
#include "sysdep.h"
#include "../sysdeps/atomic.h"
#include "../hurd/list.h"
#include <errno.h>
#include <time.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <mach.h>

int sem_init (sem_t *semp, int pshared, unsigned int val)
{
  if (val > SEM_VALUE_MAX)
    return (EINVAL);

  semp->__vw.lo = val;
  semp->__vw.hi = 0;
  semp->__flags = pshared ? GSYNC_SHARED : 0;
  return (0);
}

int sem_post (sem_t *semp)
{
  union hurd_qval tmp;

  /* Bump the semaphore's counter value and also fetch the number of
   * waiters. Before incrementing, test that the counter does not
   * overflow (return with an error in that case). */

  while (1)
    {
      tmp.qv = atomic_loadq (&semp->__vw.qv);
      if (tmp.lo == SEM_VALUE_MAX)
        {
          errno = EOVERFLOW;
          return (-1);
        }
      else if (atomic_casq_bool (&semp->__vw.qv,
          tmp.lo, tmp.hi, tmp.lo + 1, tmp.hi))
        break;
    }

  /* If there were waiters, wake one of them. */
  if (tmp.hi > 0)
    lll_wake (&semp->__vw.lo, semp->__flags);

  return (0);
}

static inline int
__sem_trywait (sem_t *semp)
{
  while (1)
    {
      unsigned int val = atomic_load (&semp->__vw.lo);
      if (val == 0)
        return (-1);
      else if (atomic_cas_bool (&semp->__vw.lo, val, val - 1))
        return (0);
    }
}

static void
cleanup (void *argp)
{
  atomic_add ((unsigned int *)argp, -1);
}

int sem_wait (sem_t *semp)
{
  int ret = 0;

  /* See if we can decrement the counter's value without blocking. */
  if (__sem_trywait (semp) == 0)
    return (0);

  /* Slow path: Add ourselves as a waiter, set up things for
   * cancellation handling and begin looping. */
  atomic_add (&semp->__vw.hi, +1);
  pthread_cleanup_push (cleanup, &semp->__vw.hi);

  while (1)
    {
      /* Try to simultaneously decrement the counter and
       * remove ourselves as a waiter. */
      union hurd_qval tmp = { atomic_loadq (&semp->__vw.qv) };
      if (tmp.lo > 0)
        {
          if (atomic_casq_bool (&semp->__vw.qv,
              tmp.lo, tmp.hi, tmp.lo - 1, tmp.hi - 1))
            break;
          else
            continue;
        }

      /* Enable async cancellation, block on the counter's
       * address and restore the previous cancellation type. */
      int prev = __pthread_cancelpoint_begin ();
      int res = lll_wait (&semp->__vw.lo, 0, semp->__flags);
      __pthread_cancelpoint_end (prev);

      if (res == KERN_INTERRUPTED)
        {
          /* We were interrupted. Bail out. */
          errno = EINTR;
          ret = -1;
          break;
        }
    }

  /* Execute the cleanup callback if we were interrupted. */
  pthread_cleanup_pop (ret != 0);
  return (ret);
}

int sem_trywait (sem_t *semp)
{
  int ret = __sem_trywait (semp);
  if (ret < 0)
    errno = EAGAIN;

  return (ret);
}

int sem_timedwait (sem_t *semp, const struct timespec *tsp)
{
  int ret = 0;

  if (__sem_trywait (semp) == 0)
    return (0);

  atomic_add (&semp->__vw.hi, +1);
  pthread_cleanup_push (cleanup, &semp->__vw.hi);

  while (1)
    {
      union hurd_qval tmp = { atomic_loadq (&semp->__vw.qv) };
      if (tmp.lo > 0)
        {
          if (atomic_casq_bool (&semp->__vw.qv,
              tmp.lo, tmp.hi, tmp.lo - 1, tmp.hi - 1))
            break;
          else
            continue;
        }
      /* We have to check the timeout parameter on every iteration,
       * because its value may not be examined if the semaphore can
       * be locked without blocking. */
      else if (__glibc_unlikely (tsp->tv_sec < 0 ||
          tsp->tv_nsec >= 1000000000))
        {
          errno = ETIMEDOUT;
          ret = -1;
          break;
        }

      int prev = __pthread_cancelpoint_begin ();
      int res = lll_abstimed_wait (&semp->__vw.lo, 0, tsp, semp->__flags);
      __pthread_cancelpoint_end (prev);

      if (res == KERN_INTERRUPTED || res == KERN_TIMEDOUT)
        {
          errno = res == KERN_INTERRUPTED ? EINTR : ETIMEDOUT;
          ret = -1;
          break;
        }
    }

  /* Execute the cleanup callback if we were interrupted or timed out. */
  pthread_cleanup_pop (ret != 0);
  return (ret);
}

int sem_getvalue (sem_t *semp, int *outp)
{
  *outp = atomic_load (&semp->__vw.lo);
  return (0);
}

int sem_destroy (sem_t *semp)
{
  if (__glibc_unlikely (atomic_load (&semp->__vw.hi) != 0))
    {
      errno = EBUSY;
      return (-1);
    }

  return (0);
}

/* Shared semaphores. */

struct shared_sem
{
  sem_t *semp;
  int refcount;
  struct hurd_list link;
  char *name;
};

struct file_data
{
  mach_port_t file;
  mach_port_t dir;
  mach_port_t memobj;
};

/* Declare some functions that are implemented by 
 * the Hurd's IO and FS servers. */
extern mach_port_t file_name_lookup (const char *, int, int);
extern int dir_mkfile (mach_port_t, int, int, mach_port_t *);
extern int file_getlinknode (mach_port_t, mach_port_t *);
extern int io_write (mach_port_t, const void *,
  size_t, loff_t, mach_msg_type_number_t *);
extern int dir_link (mach_port_t, mach_port_t, const char *, int);
extern int io_map (mach_port_t, mach_port_t *, mach_port_t *);
extern int unlink (const char *);

#define file_data_init(fp)   \
  (fp)->file = (fp)->dir = (fp)->memobj = MACH_PORT_NULL

void file_data_fini (struct file_data *fp)
{
#define maybe_dealloc(port)   \
  if (port != MACH_PORT_NULL) mach_port_deallocate (mach_task_self (), port)

  maybe_dealloc (fp->file);
  maybe_dealloc (fp->dir);
  maybe_dealloc (fp->memobj);
#undef maybe_dealloc

  file_data_init (fp);
}

/* The directory where shared semaphores are created. */
#define SHM_DIR   "/dev/shm/"

/* The prefix used to identify POSIX semaphores. */
#define SEM_PREFIX   "sem."

/* Create an unnamed file in the semaphore directory. */
static int
create_anon_file (struct file_data *fp, int flags)
{
  fp->dir = file_name_lookup (SHM_DIR, 0, 0);
  if (fp->dir == MACH_PORT_NULL ||
      dir_mkfile (fp->dir, O_RDWR, flags, &fp->file) != 0)
    return (-1);

  return (0);
}

/* Write [DATAP .. DATAP + LEN) to the anonymous file
 * opened in FP, retrying in case we're interrupted. */
static int
write_full (struct file_data *fp,
  const void *datap, size_t size)
{
  while (1)
    {
      mach_msg_type_number_t num;
      int res = io_write (fp->file, datap, size, -1, &num);
      if (res != 0)
        return (-1);
      else if ((size -= num) == 0)
        return (0);

      datap = (const char *)datap + num;
    }
}

/* Create a hard link between the unnamed file in FP and
 * the file in $SHM_DIR/$SEM_PREFIX$name. */
static int
link_file (struct file_data *fp, const char *name, size_t len)
{
  mach_port_t node;
  int res = file_getlinknode (fp->file, &node);

  if (res != 0)
    return (-1);

  /* Create the name with the semaphore prefix. */
  char *bufp = (char *)alloca (sizeof (SEM_PREFIX) + len);
  memcpy (mempcpy (bufp, SEM_PREFIX,
    sizeof (SEM_PREFIX) - 1), name, len);

  res = dir_link (fp->dir, node, bufp, 1);
  mach_port_deallocate (mach_task_self (), node);
  return (res != 0 ? -1 : res);
}

/* Map the contents of the file into the task's address space. This is
 * pretty much the same as 'mmap', only we don't check for some parameters,
 * since we know the mapping is read-write and shared. */
static void*
map_file (struct file_data *fp)
{
  mach_port_t dummy;
  if (io_map (fp->file, &dummy, &fp->memobj) != 0)
    return (NULL);
  else if (fp->memobj == dummy)
    mach_port_deallocate (mach_task_self (), dummy);

  vm_offset_t addr;
  vm_prot_t prot = VM_PROT_READ | VM_PROT_WRITE;

  if (vm_map (mach_task_self (), &addr, sizeof (sem_t), 0, 1,
      fp->memobj, 0, 0, prot, prot, VM_INHERIT_SHARE) != 0)
    return (NULL);

  return ((void *)addr);
}

/* The list of shared semaphores that have been mapped by this task,
 * and the lock protecting access to it. We could use a more efficient
 * structure, like a RB tree, but a linked list is sufficient in most
 * cases, since programs rarely need more than a handful semaphores. */
static HURD_LIST_DECL (__mapped_sems);
static unsigned int __mapped_sems_lock;

/* Convert a node in the global list to a shared semaphore. */
#define node_to_shsem(nodep)   \
  hurd_list_entry ((nodep), struct shared_sem, link)

static sem_t*
add_mapping (const char *name, size_t namelen,
  struct file_data *fp, sem_t *prevp)
{
  sem_t *ret = SEM_FAILED;
  lll_lock (&__mapped_sems_lock, 0);

  /* First, check if there's already a mapping with this name. This can
   * happen if we managed to open the backing file, but didn't create it. */
  struct hurd_list *runp;
  hurd_list_each (&__mapped_sems, runp)
    {
      struct shared_sem *sp = node_to_shsem (runp);
      if (memcmp (sp->name, name, namelen) == 0)
        {
          /* Found the semaphore. Bump its refcount and return. */
          ret = sp->semp;
          ++sp->refcount;
          goto done;
        }
    }

  /* We need to create an entry for this semaphore. Allocate the structure
   * and insert it in the global list. */
  struct shared_sem *sp =
    (struct shared_sem *)malloc (sizeof (*sp) + namelen + 1);
  if (sp != NULL)
    {
      /* If the caller didn't provide a mapping, create it now. */
      if (prevp == SEM_FAILED && !(prevp = (sem_t *)map_file (fp)))
        {
          free (sp);
          errno = ENOMEM;
          goto done;
        }

      sp->refcount = 1;
      sp->semp = ret = prevp;
      memcpy (sp->name = (char *)&sp[1], name, namelen + 1);
      hurd_list_add_tail (&__mapped_sems, &sp->link);
    }

done:
  lll_unlock (&__mapped_sems_lock, 0);
  if (ret != prevp && prevp != SEM_FAILED)
    vm_deallocate (mach_task_self (),
      (vm_address_t)prevp, sizeof (*prevp));

  return (ret);
}

static int
validate_name (const char *name, size_t len)
{
  return (*name == '/' && memchr (name + 1, '/', len - 1) == NULL);
}

sem_t* sem_open (const char *name, int oflag, ...)
{
  struct file_data fd;
  sem_t *ret = SEM_FAILED;
  size_t len = strlen (name);
  char *bufp = (char *)alloca (sizeof (SHM_DIR) +
    sizeof (SEM_PREFIX) + len + 1);

  file_data_init (&fd);

  if (!validate_name (name, len))
    {
      errno = EINVAL;
      return (ret);
    }
  else if ((oflag & (O_CREAT | O_EXCL)) != (O_CREAT | O_EXCL))
    {
      /* The semaphore may exist, and we may not have to create it. */
    try_open:

      /* Make up the complete filename. */
      memcpy (mempcpy (mempcpy (bufp, SHM_DIR,
        sizeof (SHM_DIR) - 1), SEM_PREFIX,
        sizeof (SEM_PREFIX) - 1), name + 1, len);

      fd.file = file_name_lookup (bufp, (oflag &
        ~(O_CREAT | O_ACCMODE)) | O_NOFOLLOW | O_RDWR, 0);

      if (fd.file == MACH_PORT_NULL)
        {
          /* We couldn't open an existing file, but maybe we
           * can create it. See if that's a possibility. */
          if ((oflag & O_CREAT) != 0 && errno == ENOENT)
            goto try_create;
        }
      else
        ret = add_mapping (name + 1, len - 1, &fd, ret);
    }
  else
    {
      va_list ap;
      sem_t tmp = { .__vw = { .hi = 0 }, .__flags = GSYNC_SHARED };
      mode_t mode;

    try_create:
      va_start (ap, oflag);
      mode = va_arg (ap, mode_t);
      tmp.__vw.lo = va_arg (ap, unsigned int);
      va_end (ap);

      if (tmp.__vw.lo > SEM_VALUE_MAX)
        {
          errno = EINVAL;
          return (ret);
        }

      /* We have to create an unnamed file first, because the backing file
       * used for shared semaphores must have the actual contents before
       * it can be used. */

      if (create_anon_file (&fd, mode) < 0)
        return (ret);
      else if (write_full (&fd, &tmp, sizeof (tmp)) == 0 &&
          (ret = (sem_t *)map_file (&fd)) != NULL)
        {
          if (link_file (&fd, name + 1, len) < 0)
            {
              /* The hard link failed. This may be because another
               * thread beat us to it. However, if the O_EXCL flag is not
               * set, we can try to open the (now existing) file. */
              vm_deallocate (mach_task_self (),
                (vm_address_t)ret, sizeof (*ret));
              ret = SEM_FAILED;

              if ((oflag & O_EXCL) == 0 && errno == EEXIST)
                {
                  file_data_fini (&fd);
                  goto try_open;
                }
            }
          else
            ret = add_mapping (name + 1, len - 1, &fd, ret);
        }
    }

  file_data_fini (&fd);
  return (ret);
}

int sem_close (sem_t *semp)
{
  int ret = -1;
  lll_lock (&__mapped_sems_lock, 0);

  struct hurd_list *runp;
  hurd_list_each (&__mapped_sems, runp)
    {
      struct shared_sem *sp = node_to_shsem (runp);
      if (sp->semp == semp)
        {
          /* Found a match. Decrement the refcount and delete the
           * mapping if it reached zero. */
          if (--sp->refcount == 0)
            {
              hurd_list_del (&sp->link);
              vm_deallocate (mach_task_self (),
                (vm_address_t)sp->semp, sizeof (*semp));
              free (sp);
            }

          /* Signal success for this call, and also break out of the
           * loop, since there can be no more semaphores with this name. */
          ret = 0;
          break;
        }
    }

  lll_unlock (&__mapped_sems_lock, 0);
  if (__glibc_unlikely (ret != 0))
    /* Not a shared semaphore. */
    errno = EINVAL;

  return (ret);
}

int sem_unlink (const char *name)
{
  size_t len = strlen (name);
  if (!validate_name (name, len))
    return (ENOENT);

  char *sname = (char *)alloca (sizeof (SHM_DIR) +
    sizeof (SEM_PREFIX) + len + 1);

  memcpy (mempcpy (mempcpy (sname, SHM_DIR,
    sizeof (SHM_DIR) - 1), SEM_PREFIX,
    sizeof (SEM_PREFIX) - 1), name + 1, len);

  int ret = unlink (sname);
  if (ret < 0 && errno == EPERM)
    errno = EACCES;
  return (ret);
}

