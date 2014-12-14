/* otterylite.c -- main implementation and top-level APIs for libottery-lite
 */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

/* Include the headers that actually look like headers */

#include "otterylite.h"
#include "otterylite-impl.h"

/* These "headers" really define a bunch of static functions. Sorry! A more
   sensible person might use some means to indicate that the linker shouldn't
   expose them. */

#include "otterylite_wipe.h"
#include "otterylite_rng.h"
#include "otterylite_alloc.h"
#include "otterylite_digest.h"
#include "otterylite_entropy.h"
#include "otterylite_locking.h"


/* Magic number for ottery_magic or ottery_state.magic */
#define OTTERY_MAGIC 0x6f747472u
/* Magic number for the RNG structure */
#define RNG_MAGIC 0x00480A01 /*ohai*/
/* How many times can we generate a block of ChaCha20 stuff before we
   reseed? */
#define RESEED_AFTER_BLOCKS 2048

#define OTTERY_MAGIC_MAKE_INVALID(m) ((m) = 0)
#define OTTERY_MAGIC_MAKE_VALID(m) ((m) = OTTERY_MAGIC)
#define OTTERY_MAGIC_IS_OKAY(m) ((m) == OTTERY_MAGIC)

#ifdef WIN32
#define ottery_getpid() 1
#define PID_OKAY(x) 1
#define SETPID(x) ((void)0)
#else
#define PID_OKAY(x) ((x) == getpid())
#define SETPID(x) ((x) = getpid())
#endif

#if defined(OTTERY_DISABLE_LOCKING) || defined(_WIN32) ||       \
  defined(USING_INHERIT_ZERO)
/*
  no locking or no forking means no pthread_atfork.

  INHERIT_ZERO means that we don't need pthread_atfork.
*/
#define install_atfork_handler() ((void)0)
#else
/*
  We have a pthread_atfork that we're going to use to see how many time
  we've forked.

  (We need to keep track of forks so that child processes can reseed before
  they get themselves into trouble.)
*/
#define USING_ATFORK
/*
  True if we have called pthread_atfork already.
*/
  static unsigned ottery_atfork_handler_installed;
/*
  Count of how many times we've forked.  Can wrap around.
*/
static volatile /* ???? */ unsigned ottery_fork_count;
static void ottery_child_atfork_handler(void)
{
  /* Is this necessary, or can we just increment ????*/
  __sync_fetch_and_add(&ottery_fork_count, 1);
}
/*
  Install the atfork handler if it isn't already installed.
*/
static void
install_atfork_handler(void)
{
  /*
    Does every subprocess have to do this again ????
  */
  if (!ottery_atfork_handler_installed)
    {
      /* Is this necessary, or can we just set it to 1 ???? */
      if (__sync_bool_compare_and_swap(&ottery_atfork_handler_installed, 0, 1))
        {
          if (pthread_atfork(NULL, NULL, ottery_child_atfork_handler))
            abort();
        }
    }
}
#endif

#ifdef OTTERY_STRUCT
/*
  Declaration for ottery_state.

  These fields map 1:1 to those defined below.
*/
struct ottery_state {
  DECLARE_LOCK(mutex)
  unsigned magic;
#ifndef _WIN32
  pid_t pid;
#endif
  unsigned forkcount;
  int seeding;
  int entropy_status;
  unsigned seed_counter;
  DECLARE_RNG(rng)
};
#define LOCK()                                  \
  do {                                          \
    GET_LOCK(&STATE_FIELD(mutex));              \
  } while (0)
#define UNLOCK()                                \
  do {                                          \
    RELEASE_LOCK(&STATE_FIELD(mutex));          \
  } while (0)
#else
/*
  Lock to protect the other ottery_ fields and the RNG.
*/
DECLARE_INITIALIZED_LOCK(static, ottery_mutex)
/*
  OTTERY_MAGIC, if we are initialized.
*/
static unsigned ottery_magic;
#ifndef _WIN32
/* The PID with which we last initialized  */
static pid_t ottery_pid;
#endif
/*
  The core RNG.  Probably a pointer to it, stored in an mmap.
*/
static DECLARE_RNG(ottery_rng);
/*
  True if we are currently doing an on-demand seed because of having written
  too much already.  See ottery_seed.
*/
static int ottery_seeding;
/*
  Status to determine whether we're seeded, and how well. See ottery_status().
*/
static int ottery_entropy_status;
/*
  How many times have we called ottery_seed?
*/
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

/*
  Get entropy from the entropy sources, then fold it into the RNG state.

  If 'release_lock' is set, then drop the lock while we're reading all the
  entropy sources.  (We use this when we're doing a "soft reseed" because of
  having generated a lot of data.)

  Callers must hold the lock.
*/
static int
ottery_seed(OTTERY_STATE_ARG_FIRST int release_lock)
{
  int n, new_status = 0;
  /*
    We generate one OTTERY_DIGEST_LEN-sized chunk when we begin, and another
    when we're done.  In the middle, we generate up to OTTERY_ENTROPY_MAXLEN
    bytes of new entropy.
  */
  unsigned char entropy[OTTERY_DIGEST_LEN * 2 + OTTERY_ENTROPY_MAXLEN];
  unsigned char digest[OTTERY_DIGEST_LEN];

  /*
    Start out with some bytes from the current RNG state.  If the RNG is being
    newly initialized, these will just come from the RNG with key 0, but
    that doesn't hurt anything.
  */
  ottery_bytes(RNG_PTR, entropy, OTTERY_DIGEST_LEN);

  /*
    Note that we currently have a seed in progress, so that we don't launch
    another one.
  */
  STATE_FIELD(seeding) = 1;
  /*
    Prevent another seed from being triggered immediately.
  */
  RNG_PTR->count = 0;

  if (release_lock)
    UNLOCK();
  /* Release the lock in this section, since it can take a while to get
   * entropy. */

  n = ottery_getentropy(entropy + OTTERY_DIGEST_LEN, &new_status);

  /* Once done, reacquire the lock. */
  if (release_lock)
    LOCK();

  /*
    If we didn't get enough entropy, or we got an error, we failed.
  */
  if (n < OTTERY_ENTROPY_MINLEN)
    {
      return -1;
    }

  /*
    We do this again here in case more entropy got added in the meantime
    using ottery_addrandom or because of a fork.
  */
  ottery_bytes(RNG_PTR, entropy + n + OTTERY_DIGEST_LEN, OTTERY_DIGEST_LEN);

  /*
    Now compress the whole input down to an OTTERY_DIGEST_LEN-sized blob
  */
  ottery_digest(digest, entropy, n + OTTERY_DIGEST_LEN * 2);

  /*
    And update our current state once more
  */
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
#define RNG_MAGIC_IS_OKAY() (RNG_PTR->magic == RNG_MAGIC)
#define NEED_REINIT (!RNG_MAGIC_IS_OKAY())
#else
/*
  Otherwise, we have one or two ways of telling whether we forked. We can look
  to see if getpid() changed, or we can look to see whether the atfork handler
  was called.  Neither is a perfect method.

  Checking for changes in getpid() can fail if a child doesn't use the RNG,
  and the grandchild later gets the same pid as its grandparent.

  pthread_atfork() can fail if we invoke the underlying fork() system call
  directly, or if you clone() instead of fork()ing, or some other trickery.

  INHERIT_ZERO is a much better solution.
*/
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

#define NEED_REINIT (!OTTERY_MAGIC_IS_OKAY(STATE_FIELD(magic)) ||       \
                     !PID_OKAY(STATE_FIELD(pid)) ||                     \
                     FORK_COUNT_INCREASED())
#endif

/*
  (Re)initialize a state.  If 'postfork' is set, then we just forked.
  Otherwise, we're initializing it for the first time.

  Return 0 on success, -1 on failure.
*/
static int
  ottery_init_backend(OTTERY_STATE_ARG_FIRST int postfork)
{
  const int should_reallocate = !postfork
#ifdef USING_INHERIT_NONE
    /* If INHERIT_NONE is in use, we must always reallocate the mapping. */
    || 1
#endif
    ;

#ifdef OTTERY_STRUCT
  if (!postfork)
    INIT_LOCK(&STATE_FIELD(mutex));
#endif

  if (should_reallocate)
    {
      if (ALLOCATE_RNG(RNG_PTR) < 0)
        return -1;
    }

  install_atfork_handler(); /* This should be idempotent. */

  STATE_FIELD(entropy_status) = -2; /* We start out uninitialized */

  if (ottery_seed(OTTERY_STATE_ARG_OUT COMMA 0) < 0)
    {
      FREE_RNG(RNG_PTR);
      RNG_PTR = NULL;
      return -1;
    }

  RNG_PTR->magic = RNG_MAGIC;
  RESET_FORK_COUNT();
  OTTERY_MAGIC_MAKE_VALID(STATE_FIELD(magic));
  SETPID(STATE_FIELD(pid));
  return 0;
}

/*
  We've noted that we need to reinitialize.  Figure out whether it's because
  of a fork, and act accordingly.
*/
static int
ottery_handle_reinit(OTTERY_STATE_ARG_ONLY)
{
  int postfork;

  /* ???? audit this carefully */
#ifdef _WIN32
  /* No fork means no postfork */
  postfork = 0;
#elif defined(USING_INHERIT_ZERO)
  /* If the magic is set to something but the RNG magic got zeroed, we
     forked. */
  postfork = STATE_FIELD(magic) && RNG_PTR && RNG_PTR->magic == 0;
#else
  /* If the magic is set to something, we need to reinit. */
  postfork = STATE_FIELD(magic);
#endif
  return ottery_init_backend(OTTERY_STATE_ARG_OUT COMMA postfork);
}

#ifdef OTTERY_STRUCT
void
OTTERY_PUBLIC_FN2 (init)(OTTERY_STATE_ARG_ONLY)
{
  memset(state, 0, sizeof(*state));
  if (ottery_init_backend(OTTERY_STATE_ARG_OUT COMMA 0) < 0)
    abort();
}
int
OTTERY_PUBLIC_FN2 (try_init)(OTTERY_STATE_ARG_ONLY)
{
  memset(state, 0, sizeof(*state));
  return(ottery_init_backend(OTTERY_STATE_ARG_OUT COMMA 0) < 0 ? -1 : 0);
}
#endif

void
OTTERY_PUBLIC_FN2 (teardown)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  /*
    The lock is statically allocated otherwise.
  */
  DESTROY_LOCK(&STATE_FIELD(mutex));
#endif
  FREE_RNG(RNG_PTR);
  OTTERY_MAGIC_MAKE_INVALID(STATE_FIELD(magic));
}

/* XXXX document */
static inline int
init_or_reseed_as_needed(OTTERY_STATE_ARG_ONLY)
{
  if (UNLIKELY(NEED_REINIT)) {
    if (ottery_handle_reinit(OTTERY_STATE_ARG_OUT) < 0)
      return -1;
  } else if (UNLIKELY(RNG_PTR->count > RESEED_AFTER_BLOCKS) &&
             !STATE_FIELD(seeding)) {
    ottery_seed(OTTERY_STATE_ARG_OUT COMMA 1);
  }
  return 0;
}


/*
  Helper: reinitialize and reseed the RNG if we have not initialized it
  previously, or we have forked.  Abort on failure.
*/
#define INIT()                                                  \
  do {                                                          \
    if (init_or_reseed_as_needed(OTTERY_STATE_ARG_OUT) < 0)     \
      abort();                                                  \
  } while (0)

void
OTTERY_PUBLIC_FN2 (need_reseed)(OTTERY_STATE_ARG_ONLY)
{
  LOCK();
  RNG_PTR->count = RESEED_AFTER_BLOCKS + 1;
  UNLOCK();
}

unsigned
OTTERY_PUBLIC_FN (random)(OTTERY_STATE_ARG_ONLY)
{
  unsigned result;

  LOCK();
  INIT();
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
  ottery_bytes(RNG_PTR, &result, sizeof(result));
  UNLOCK();
  return result;
}

unsigned
OTTERY_PUBLIC_FN (random_uniform)(OTTERY_STATE_ARG_FIRST unsigned upper)
{
  unsigned divisor, result;

  if (upper == 0)
    return 0; /* arc4random(0) works this way, so let's treat it as
                 the least-wrong response to "give me an unsigned int less
                 than 0". */

  divisor = UINT_MAX / upper;

  LOCK();
  INIT();
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
  uint64_t divisor, result;

  if (upper == 0)
    return 0;

  divisor = UINT64_MAX / upper;

  LOCK();
  INIT();
  do
    {
      ottery_bytes(RNG_PTR, &result, sizeof(result));
      result /= divisor;
    } while (result >= upper);
  UNLOCK();
  return result;
}

#define LARGE_BUFFER_CUTOFF  (OTTERY_BUFLEN - OTTERY_KEYLEN)

void
OTTERY_PUBLIC_FN (random_buf)(OTTERY_STATE_ARG_FIRST void *output, size_t n)
{
  LOCK();
  INIT();
  if (n < LARGE_BUFFER_CUTOFF)
    {
      ottery_bytes(RNG_PTR, output, n);
      UNLOCK();
    }
  else
    {
      u8 key[OTTERY_KEYLEN + CHACHA_BLOCKSIZE - 1];
      size_t leftovers = n & (CHACHA_BLOCKSIZE - 1);
      ottery_bytes(RNG_PTR, key, OTTERY_KEYLEN + leftovers);
      UNLOCK();
      chacha20_blocks(key, n / CHACHA_BLOCKSIZE, output);
      memcpy(((u8*)output) + (n - leftovers),
             key + OTTERY_KEYLEN,
             leftovers);
      memwipe(key, sizeof(key));
    }
}

void
OTTERY_PUBLIC_FN2 (addrandom)(OTTERY_STATE_ARG_FIRST const unsigned char *inp, int n)
{
  if (n <= 0)
    return;
  LOCK();
  INIT();
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
  if (socklen > (int)sizeof(ottery_egd_sockaddr)) {
    errno = EINVAL;
    return -1;
  }
  if (socklen <= 0 || !sa)
    {
      memset(&ottery_egd_sockaddr, 0, sizeof(ottery_egd_sockaddr));
      ottery_egd_socklen = -1;
    }
  else
    {
      memcpy(&ottery_egd_sockaddr, sa, socklen);
      ottery_egd_socklen = socklen;
    }
  return 0;
}
#endif

int
OTTERY_PUBLIC_FN2 (status)(OTTERY_STATE_ARG_ONLY)
{
  int r;

  LOCK();
  /* This would ordinarily abort, but this function is special. */
  if (init_or_reseed_as_needed(OTTERY_STATE_ARG_OUT) < 0) {
    UNLOCK();
    return -2;
  }
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
