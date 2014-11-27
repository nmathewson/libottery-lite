
#ifdef OTTERY_DISABLE_LOCKING

#define DECLARE_INITIALIZED_LOCK(scope, name)
#define DECLARE_LOCK(name)
#define INIT_LOCK(lock) ((void)0)
#define DESTROY_LOCK(lock) ((void)0)
#define GET_LOCK(lock) ((void)0)
#define RELEASE_LOCK(lock) ((void)0)

#elif defined(_WIN32)

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

#endif
