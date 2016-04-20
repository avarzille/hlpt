#include "pthread.h"
#include <stdio.h>
#include <errno.h>

#define fail(str)   \
  do   \
    {   \
      puts (str);   \
      return (0);   \
    }   \
  while (0)

static pthread_barrier_t b;
static pthread_mutex_t m;

void* thrfn (void *argp)
{
  int e = pthread_mutex_unlock (&m);
  if (e == 0)
    fail ("child: mutex_unlock succeeded");
  else if (e != EPERM)
    fail ("child: mutex_unlock did not return EPERM");

  e = pthread_mutex_trylock (&m);

  if (e == 0)
    fail ("child: mutex_trylock succeeded");
  else if (e != EBUSY)
    fail ("child: mutex_trylock did not return EBUSY");

  e = pthread_barrier_wait (&b);
  if (e != 0 && e != PTHREAD_BARRIER_SERIAL_THREAD)
    fail ("child: first barrier_wait failed");

  e = pthread_barrier_wait (&b);
  if (e != 0 && e != PTHREAD_BARRIER_SERIAL_THREAD)
    fail ("child: second barrier_wait failed");

  e = pthread_mutex_unlock (&m);
  if (e == 0)
    fail ("child: second mutex_unlock succeeded");
  else if (e != EPERM)
    fail ("child: second mutex_unlock did not return EPERM");

  if (pthread_mutex_trylock (&m) != 0)
    fail ("child: second mutex_trylock failed");
  else if (pthread_mutex_unlock (&m) != 0)
    fail ("child: third mutex_unlock failed");

  return (argp);
}

int main (void)
{
  pthread_mutexattr_t a;
  int e;

  if (pthread_mutexattr_init (&a) != 0)
    fail ("mutexattr_init failed");
  else if (pthread_mutexattr_settype (&a, PTHREAD_MUTEX_ERRORCHECK) != 0)
    fail ("mutexattr_settype failed");

  if (pthread_mutex_init (&m, &a) != 0)
    fail ("mutex_init failed");
  else if (pthread_barrier_init (&b, 0, 2) != 0)
    fail ("barrier_init failed");

  e = pthread_mutex_unlock (&m);
  if (e == 0)
    fail ("mutex_unlock succeeded");
  else if (e != EPERM)
    fail ("mutex_unlock did not return EPERM");
  else if (pthread_mutex_lock (&m) != 0)
    fail ("mutex_lock failed");

  e = pthread_mutex_lock (&m);
  if (e == 0)
    fail ("2nd mutex_lock succeeded");
  else if (e != EDEADLK)
    fail ("2nd mutex_lock did not return EDEADLK");

  pthread_t th;
  if (pthread_create (&th, NULL, thrfn, NULL) != 0)
    fail ("create failed");

  e = pthread_barrier_wait (&b);
  if (e != 0 && e != PTHREAD_BARRIER_SERIAL_THREAD)
    fail ("1st barrier_wait failed");
  else if (pthread_mutex_unlock (&m) != 0)
    fail ("2nd mutex_unlock failed");

  e = pthread_mutex_unlock (&m);
  if (e == 0)
    fail ("3rd mutex_unlock succeeded");
  else if (e != EPERM)
    fail ("3rd mutex_unlock did not return EPERM");

  e = pthread_barrier_wait (&b);
  if (e != 0 && e != PTHREAD_BARRIER_SERIAL_THREAD)
    fail ("2nd barrier_wait failed");
  else if (pthread_join (th, NULL) != 0)
    fail ("thread_join failed");
  else if (pthread_mutex_destroy (&m) != 0)
    fail ("mutex_destroy failed");
  else if (pthread_barrier_destroy (&b) != 0)
    fail ("barrier_destroy failed");
  else if (pthread_mutexattr_destroy (&a) != 0)
    fail ("mutexattr_destroy failed");

  puts ("done");
  return (0);
}
