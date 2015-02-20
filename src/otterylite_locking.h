/* otterylite_locking.h -- Lock manipulation and declaration macros */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/
#ifndef OTTERYLITE_LOCKING_H_INCLUDED
#define OTTERYLITE_LOCKING_H_INCLUDED

#ifdef __GNUC__
#define INITIALIZER_FUNC(name)                          \
  static void name(void) __attribute__((constructor));  \
  static void name(void)
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU",read)
#define INITIALIZER_FUNC(name)                                              \
  static void __cdecl name(void);                                           \
  __declspec(allocate(".CRT$XCU")) void(__cdecl * name ## _) (void) = name; \
  static void name(void)
#endif

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
#define RELEASE_STATIC_LOCK(lock) ((void)0)

#elif defined(_WIN32)

/*
  On Windows, the fast mutex is called "CRITICAL_SECTION".
*/

#define DECLARE_LOCK(name)                      \
  CRITICAL_SECTION name;
#define INIT_LOCK(lock)                                 \
  InitializeCriticalSectionAndSpinCount(lock, 6000)
#define DESTROY_LOCK(lock)                      \
  DeleteCriticalSection(lock)
#define GET_LOCK(lock)                          \
  EnterCriticalSection(lock)
#define RELEASE_LOCK(lock)                      \
  LeaveCriticalSection(lock)

/*
  XXXX DOCUMENT
*/
#define DECLARE_INITIALIZED_LOCK(scope, name)   \
  scope CRITICAL_SECTION name;                  \
  INITIALIZER_FUNC(initialize_cs_ ## name)      \
  {                                             \
    INIT_LOCK(&name);                           \
  }
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#elif defined(__APPLE__)

#define DECLARE_INITIALIZED_LOCK(scope, name)   \
  scope OSSpinLock name = OS_SPINLOCK_INIT;     \
  INITIALIZER_FUNC(init_spinlock_ ## name) {    \
    name = 0;                                   \
  }
#define DECLARE_LOCK(name)                      \
  OSSpinLock name;
#define INIT_LOCK(lock)                         \
  (*(lock) = OS_SPINLOCK_INIT)
#define DESTROY_LOCK(lock)                      \
  ((void)0)
#define GET_LOCK(lock)                          \
  OSSpinLockLock(lock)
#define RELEASE_LOCK(lock)                      \
  OSSpinLockUnlock(lock)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#elif !defined(OTTERY_NO_PTHREAD_SPINLOCKS)

#define DECLARE_INITIALIZED_LOCK(scope, name)   \
  scope pthread_spinlock_t name;                \
  INITIALIZER_FUNC(initialize_cs_ ## name)      \
  {                                             \
    pthread_spin_init(&name, 0);                \
  }

#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#define DECLARE_LOCK(name)                      \
  pthread_spinlock_t name;
#define INIT_LOCK(lock)                         \
  pthread_spin_init(lock, 0)
#define DESTROY_LOCK(lock)                      \
  pthread_spin_destroy(lock)
#define GET_LOCK(lock)                          \
  pthread_spin_lock(lock)
#define RELEASE_LOCK(lock)                      \
  pthread_spin_unlock(lock)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)

#else /* !_WIN32, !__APPLE__, !DISABELD */

/* pthreads makes all of that stuff fairly easy. */

#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER;
#define DECLARE_LOCK(name)                      \
  pthread_mutex_t name;
#define INIT_LOCK(lock)                         \
  pthread_mutex_init(lock, NULL)
#define DESTROY_LOCK(lock)                      \
  pthread_mutex_destroy(lock)
#define GET_LOCK(lock)                          \
  pthread_mutex_lock(lock)
#define RELEASE_LOCK(lock)                      \
  pthread_mutex_unlock(lock)
#define GET_STATIC_LOCK(lock) GET_LOCK(&lock)
#define RELEASE_STATIC_LOCK(lock) RELEASE_LOCK(&lock)
#endif /* !_WIN32, !__APPLE__, !DISABLED */
#endif /* OTTERYLITE_LOCKING_H_INCLUDED */
