/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#include "otterylite.h"
#include "otterylite-impl.h"

#include "otterylite_wipe.h"
#include "otterylite_rng.h"
#include "otterylite_digest.h"
#include "otterylite_entropy.h"
#include "otterylite_locking.h"
#include "otterylite_alloc.h"

#define OTTERY_MAGIC 0x6f747472u
#define RNG_MAGIC 0x00480A01 /*ohai*/
#define RESEED_AFTER_BLOCKS 2048

#ifdef _WIN32
#define ottery_getpid() 0
#else
#define ottery_getpid() getpid()
#endif

#define OTTERY_MAGIC_MAKE_INVALID(m) ((m) = 0 ^ ottery_getpid())
#define OTTERY_MAGIC_OKAY(m) ((m == (OTTERY_MAGIC ^ ottery_getpid())))

#if defined(OTTERY_DISABLE_LOCKING) || defined(_WIN32)
/* no locking or no forking means no pthread_atfork. */
#define install_atfork_handler() ((void)0)
#else
#define USING_ATFORK
static unsigned ottery_atfork_handler_installed;
static volatile /* ???? */ unsigned ottery_fork_count;
static void ottery_child_atfork_handler(void)
{
  /* Is this necessary, or can we just ++ ????*/
  __sync_fetch_and_add(&ottery_fork_count, 1);
}
static void
install_atfork_handler(void)
{
  if (!ottery_atfork_handler_installed) {
    /* Is this necessary, or can we just set it to 1 ???? */
    if (__sync_bool_compare_and_swap(&ottery_atfork_handler_installed, 0, 1)) {
      if (pthread_atfork(NULL, NULL, ottery_child_atfork_handler))
        abort();
    }
  }
}
#endif

#ifdef OTTERY_STRUCT
struct ottery_state {
  DECLARE_LOCK(mutex)
  unsigned magic;
  unsigned forkcount;
  int seeding;
  int entropy_status;
  unsigned seed_counter;
  DECLARE_RNG(rng);
};
#define LOCK()                                  \
  do {                                          \
    GET_LOCK(&STATE_FIELD(mutex));              \
  } while (0)
#define UNLOCK()                                \
  do {                                          \
    RELEASE_LOCK(&STATE_FIELD(mutex));       \
  } while (0)
#else
DECLARE_INITIALIZED_LOCK(static, ottery_mutex)
static unsigned ottery_magic;
static DECLARE_RNG(ottery_rng);
static int ottery_seeding;
static int ottery_entropy_status;
static unsigned ottery_seed_counter;
#define LOCK()                                  \
  do {                                          \
    GET_STATIC_LOCK(ottery_mutex);              \
  } while (0)
#define UNLOCK()                                \
  do {                                          \
    RELEASE_STATIC_LOCK(ottery_mutex);          \
  } while (0)
#endif

#if OTTERY_DIGEST_LEN < OTTERY_KEYLEN
/* If we ever need to use a 32-byte digest, we can pad it or stretch it
 * or something */
#error "We need a digest that is longer then the key we mean to use."
#endif

static int
ottery_seed(OTTERY_STATE_ARG_FIRST int release_lock)
{
  int n, new_status=0;
  unsigned char entropy[OTTERY_DIGEST_LEN*2 + OTTERY_ENTROPY_MAXLEN];
  unsigned char digest[OTTERY_DIGEST_LEN];

  ottery_bytes(RNG_PTR, entropy, OTTERY_DIGEST_LEN);

  STATE_FIELD(seeding) = 1;
  RNG_PTR->count = 0;

  if (release_lock)
    UNLOCK();
  /* Release the lock in this section, since it can take a while to get
   * entropy. */

  n = ottery_getentropy(entropy + OTTERY_DIGEST_LEN, &new_status);

  if (release_lock)
    LOCK();

  if (n < OTTERY_ENTROPY_MINLEN) {
    return -1;
  }

  /* We do this again here in case more entropy was added in the meantime
   * using ottery_addrandom */
  ottery_bytes(RNG_PTR, entropy + n + OTTERY_DIGEST_LEN, OTTERY_DIGEST_LEN);

  ottery_digest(digest, entropy, n + OTTERY_DIGEST_LEN*2);

  STATE_FIELD(entropy_status) = new_status;
  STATE_FIELD(seeding) = 0;
  ottery_setkey(RNG_PTR, digest);
  RNG_PTR->count = 0;
  ++STATE_FIELD(seed_counter);

  memwipe(digest, sizeof(digest));
  memwipe(entropy, sizeof(entropy));

  return 0;
}

#if defined(USING_INHERIT_ZERO)
/* If we really have inherit_zero, then we can avoid messing with pids
 * and atfork completely. */
#define RNG_MAGIC_OKAY() (RNG_PTR->magic == RNG_MAGIC)
#define NEED_REINIT ( !RNG_MAGIC_OKAY() )
#else

#if !defined(USING_ATFORK)
#define FORK_COUNT_INCREASED() 0
#define RESET_FORK_COUNT() ((void)0)
#elif defined(OTTERY_STRUCT)
#define FORK_COUNT_INCREASED() (state->forkcount != ottery_fork_count)
#define RESET_FORK_COUNT() (state->forkcount = ottery_fork_count);
#else
#define FORK_COUNT_INCREASED() (ottery_fork_count != 0)
#define RESET_FORK_COUNT() (ottery_fork_count = 0)
#endif

#define NEED_REINIT ( !OTTERY_MAGIC_OKAY(STATE_FIELD(magic)) || \
                      FORK_COUNT_INCREASED() )
#endif

static int
ottery_init_backend(OTTERY_STATE_ARG_FIRST int postfork)
{
  const int should_reallocate = !postfork
#ifdef USING_INHERIT_NONE
    || 1
#endif
    ;

#ifdef OTTERY_STRUCT
  if (!postfork)
    INIT_LOCK(&STATE_FIELD(mutex));
#endif

  if (should_reallocate) {
    if (ALLOCATE_RNG(RNG_PTR) < 0)
      return -1;
  }

  install_atfork_handler();

  RNG_PTR->magic = RNG_MAGIC;
  STATE_FIELD(entropy_status) = -2;

  if (ottery_seed(OTTERY_STATE_ARG_OUT COMMA 0) < 0) {
    FREE_RNG(RNG_PTR);
    RNG_PTR = NULL;
    return -1;
  }

  RESET_FORK_COUNT();
  STATE_FIELD(magic) = OTTERY_MAGIC ^ ottery_getpid();
  return 0;
}

static int
ottery_handle_reinit(OTTERY_STATE_ARG_ONLY)
{
  int postfork;
  /* ???? audit this carefully */
#ifdef _WIN32
  postfork = 0;
#elif USING_INHERIT_ZERO
  postfork = STATE_FIELD(magic) && RNG_PTR && RNG_PTR->magic == 0;
#else
  postfork = STATE_FIELD(magic) && FORK_COUNT_INCREASED();
#endif
  return ottery_init_backend(OTTERY_STATE_ARG_OUT COMMA postfork);
}

#ifdef OTTERY_STRUCT
void
OTTERY_PUBLIC_FN2 (init)(OTTERY_STATE_ARG_ONLY)
{
  if (ottery_init_backend(OTTERY_STATE_ARG_OUT COMMA 0) < 0)
    abort();
}
#endif

void
OTTERY_PUBLIC_FN2 (teardown)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  DESTROY_LOCK(&STATE_FIELD(mutex));
#endif
  FREE_RNG(RNG_PTR);
  OTTERY_MAGIC_MAKE_INVALID(STATE_FIELD(magic));
}

#define INIT()                                           \
  do {                                                   \
  if (UNLIKELY( NEED_REINIT )) {                         \
    if (ottery_handle_reinit(OTTERY_STATE_ARG_OUT) < 0)  \
      abort();                                           \
    }                                                    \
  } while (0)

void
OTTERY_PUBLIC_FN2 (need_reseed)(OTTERY_STATE_ARG_ONLY)
{
  LOCK();
  OTTERY_MAGIC_MAKE_INVALID(STATE_FIELD(magic));
  UNLOCK();
}

#define CHECK()                                                         \
  do {                                                                  \
    if (UNLIKELY(RNG_PTR->count > RESEED_AFTER_BLOCKS) && !STATE_FIELD(seeding)) { \
      ottery_seed(OTTERY_STATE_ARG_OUT COMMA 1);                        \
    }                                                                   \
  } while (0)

unsigned
OTTERY_PUBLIC_FN (random)(OTTERY_STATE_ARG_ONLY)
{
  unsigned result;

  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG_PTR, &result, sizeof(result));
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN (random64)(OTTERY_STATE_ARG_ONLY)
{
  uint64_t result;

  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG_PTR, &result, sizeof(result));
  UNLOCK();
  return result;
}

unsigned
OTTERY_PUBLIC_FN (random_uniform)(OTTERY_STATE_ARG_FIRST unsigned upper)
{
  const unsigned divisor = UINT_MAX / upper;
  unsigned result;

  LOCK();
  INIT();
  CHECK();
  do
    {
      ottery_bytes(RNG_PTR, &result, sizeof(result));
      result /= divisor;
    } while (result >= upper);
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN (random_uniform64)(OTTERY_STATE_ARG_FIRST uint64_t upper)
{
  const uint64_t divisor = UINT64_MAX / upper;
  uint64_t result;

  LOCK();
  INIT();
  CHECK();
  do
    {
      ottery_bytes(RNG_PTR, &result, sizeof(result));
      result /= divisor;
    } while (result >= upper);
  UNLOCK();
  return result;
}

void
OTTERY_PUBLIC_FN (random_buf)(OTTERY_STATE_ARG_FIRST void *output, size_t n)
{
  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG_PTR, output, n);
  UNLOCK();
}

void
OTTERY_PUBLIC_FN2 (addrandom)(OTTERY_STATE_ARG_FIRST const unsigned char *inp, int n)
{
  if (n <= 0)
    return;
  LOCK();
  INIT();
  CHECK();
  {
    u8 buf[OTTERY_DIGEST_LEN * 2];
    u8 digest[OTTERY_DIGEST_LEN];

    ottery_bytes(RNG_PTR, buf, OTTERY_DIGEST_LEN);
    ottery_digest(buf + OTTERY_DIGEST_LEN, inp, n);
    ottery_digest(digest, buf, sizeof(buf));
    ottery_setkey(RNG_PTR, digest);
    RNG_PTR->count = 0;

    memwipe(digest, sizeof(digest));
    memwipe(buf, sizeof(buf));
  }
  UNLOCK();
}

#ifndef OTTERY_DISABLE_EGD
int
OTTERY_PUBLIC_FN (set_egd_address)(const struct sockaddr *sa, int socklen)
{
  if (socklen > (int)sizeof(ottery_egd_sockaddr))
    return -1;
  memcpy(&ottery_egd_sockaddr, sa, socklen);
  ottery_egd_socklen = socklen;
  return 0;
}
#endif

int
OTTERY_PUBLIC_FN2 (status)(OTTERY_STATE_ARG_ONLY)
{
  int r;
  LOCK();
  /* This would ordinarily abort, but this function is special. */
  if (NEED_REINIT) {
    if (ottery_handle_reinit(OTTERY_STATE_ARG_OUT) < 0) {
      UNLOCK();
      return -2;
    }
  }
  CHECK();
  r = STATE_FIELD(entropy_status);
  UNLOCK();
  return r;
}

#ifdef OTTERY_STRUCT
size_t
OTTERY_PUBLIC_FN2 (state_size)(void)
{
  return sizeof(struct ottery_state);
}
#endif
