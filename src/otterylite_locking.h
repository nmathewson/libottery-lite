/* otterylite_locking.h -- Lock manipulation and declaration macros */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/
#ifndef OTTERYLITE_LOCKING_H_INCLUDED
#define OTTERYLITE_LOCKING_H_INCLUDED

#ifdef OTTERY_DISABLE_LOCKING
/*
  If locking is disabled, a lot of things become a no-op.
 */


#define DECLARE_INITIALIZED_LOCK(scope, name)
#define DECLARE_LOCK(name)
#define INIT_LOCK(lock) ((void)0)
#define DESTROY_LOCK(lock) ((void)0)
#define GET_LOCK(lock) ((void)0)
#define RELEASE_LOCK(lock) ((void)0)
#define GET_STATIC_LOCK(lock) ((void)0)
#define CHECK_LOCK_INITIALIZED(lock) ((void)0)
#define RELEASE_STATIC_LOCK(lock) ((void)0)

#elif defined(_WIN32)

/*
  On Windows, the fast mutex is called "CRITICAL_SECTION".
*/

#define DECLARE_LOCK(name) \
  CRITICAL_SECTION name;
#define INIT_LOCK(lock) \
  InitializeCriticalSectionAndSpinCount(lock, 6000)
#define DESTROY_LOCK(lock) \
  DeleteCriticalSection(lock)
#define GET_LOCK(lock) \
  EnterCriticalSection(lock)
#define RELEASE_LOCK(lock) \
  LeaveCriticalSection(lock)

/*
  One tricky point here is that you're not actually supposed to declare
  an initializer for a CRITICAL_SECTION.  So instead, we have a separate
  "initialized" variable for it, and we do atomic operations to try to
  make sure we initialize it only once.
*/
#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope int name ## _initialized = 0;                           \
  scope CRITICAL_SECTION *name = NULL;

/* ???? Get some review on this; the only thing I'm really sure about
   ???? when it comes to atomic operations is that they're harder
   ???? than you think.

   This macro checks whether the statically allocated critical section in
   "lock" has been initialized yet, and if not, creates a new critical
   section for it.
 */
#define CHECK_LOCK_INITIALIZED(lock)                                    \
  do {                                                                  \
    if (UNLIKELY(! lock ## _initialized )) {                            \
      CRITICAL_SECTION *csp;                                            \
      CRITICAL_SECTION *new_cs = malloc(sizeof(CRITICAL_SECTION));      \
      void *cspp = &lock;                                               \
      if (!new_cs)                                                      \
        abort();                                                        \
      INIT_LOCK(new_cs);                                                \
      /* Now set it to new_cs if it was NULL */                         \
      csp = InterlockedCompareExchangePointer(cspp, new_cs, NULL);      \
      if (csp != NULL) { /* Someone else set it. */                     \
        DeleteCriticalSection(new_cs);                                  \
        free(new_cs);                                                   \
      }                                                                 \
      /* Do I need a memory barrier here? */                            \
    }                                                                   \
  } while (0)

#define GET_STATIC_LOCK(lock)                   \
    do {                                        \
      CHECK_LOCK_INITIALIZED(lock);             \
      GET_LOCK(lock);                           \
      lock ## _initialized = 1;                 \
    } while(0)
#define RELEASE_STATIC_LOCK(lock) \
    RELEASE_LOCK(lock)

#elif __APPLE__

#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope OSSpinLock name = OS_SPINLOCK_INIT;
#define DECLARE_LOCK(name) \
  OSSpinLock name;
#define INIT_LOCK(lock) \
  (*(lock) = OS_SPINLOCK_INIT)
#define DESTROY_LOCK(lock) \
  ((void)0)
#define GET_LOCK(lock) \
  OSSpinLockLock(lock)
#define RELEASE_LOCK(lock) \
  OSSpinLockUnlock(lock)
#define CHECK_LOCK_INITIALIZED(lock) ((void)0)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#elif !defined(OTTERY_NO_PTHREAD_SPINLOCKS)

#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope pthread_spin_t name = 0;
#define DECLARE_LOCK(name) \
  pthread_spin_t name;
#define INIT_LOCK(lock) \
  pthread_spin_init(lock, 0)
#define DESTROY_LOCK(lock) \
  pthread_spin_destroy(lock)
#define GET_LOCK(lock) \
  pthread_spin_lock(lock)
#define RELEASE_LOCK(lock) \
  pthread_spin_unlock(lock)
#define CHECK_LOCK_INITIALIZED(lock) ((void)0)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)


#else /* !_WIN32, !__APPLE__, !DISABELD */

/* pthreads makes all of that stuff fairly easy. */

#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER;
#define DECLARE_LOCK(name) \
  pthread_mutex_t name;
#define INIT_LOCK(lock) \
  pthread_mutex_init(lock, NULL)
#define DESTROY_LOCK(lock) \
  pthread_mutex_destroy(lock)
#define GET_LOCK(lock) \
  pthread_mutex_lock(lock)
#define RELEASE_LOCK(lock) \
  pthread_mutex_unlock(lock)
#define CHECK_LOCK_INITIALIZED(lock) ((void)0)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#endif /* !_WIN32, !__APPLE__, !DISABLED */

#endif /* OTTERYLITE_LOCKING_H_INCLUDED */
