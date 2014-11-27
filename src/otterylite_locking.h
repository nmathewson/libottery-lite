
#ifdef OTTERY_DISABLE_LOCKING

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

#define DECLARE_INITIALIZED_LOCK(scope, name)                   \
  scope int name ## _initialized = 0;                           \
  scope CRITICAL_SECTION *name = NULL;
#define DECLARE_LOCK(name) \
  CRITICAL_SECTION name;
#define INIT_LOCK(lock) \
  InitializeCriticalSectionAndSpinCount(lock, 3000)
#define DESTROY_LOCK(lock) \
  DeleteCriticalSection(lock)
#define GET_LOCK(lock) \
  EnterCriticalSection(lock)
#define RELEASE_LOCK(lock) \
  LeaveCriticalSection(lock)
/* XXXX Get some review on this */
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

#else

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

#endif
