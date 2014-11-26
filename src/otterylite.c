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

#define MAGIC 0x6f747472u
#define RESEED_AFTER_BLOCKS 2048

#define MAGIC_MAKE_INVALID(m) ((m) = 0 ^ getpid())
#define MAGIC_OKAY(m) ((m == (MAGIC ^ getpid())))

#ifdef OTTERY_STRUCT
struct ottery_state {
  pthread_mutex_t mutex;
  unsigned magic;
  int seeding;
  struct ottery_rng rng;
};
#else
static pthread_mutex_t ottery_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned ottery_magic;
static struct ottery_rng ottery_rng;
static int ottery_seeding;
#endif

#define LOCK()                                  \
  do {                                          \
    pthread_mutex_lock(&STATE_FIELD(mutex));    \
  } while (0)

#define UNLOCK()                                \
  do {                                          \
    pthread_mutex_unlock(&STATE_FIELD(mutex));  \
  } while (0)

static int
ottery_seed(OTTERY_STATE_ARG_FIRST int release_lock)
{
  int n;
  unsigned char entropy[KEYLEN+OTTERY_ENTROPY_MAXLEN];
#if OTTERY_DIGEST_LEN > KEYLEN
  unsigned char digest[OTTERY_DIGEST_LEN];
#else
  unsigned char digest[KEYLEN];
  memset(digest, 0, sizeof(digest));
#endif

  ottery_bytes(RNG, entropy, KEYLEN);

  STATE_FIELD(seeding) = 1;
  RNG->count = 0;

  if (release_lock)
    UNLOCK();
  /* Release the lock in this section, since it can take a while to get
   * entropy. */

  n = ottery_getentropy(entropy+KEYLEN);

  if (release_lock)
    LOCK();

  if (n < OTTERY_ENTROPY_MINLEN)
    return -1;

  OTTERY_DIGEST(digest, entropy, n+KEYLEN);

  STATE_FIELD(seeding) = 0;
  ottery_setkey(RNG, digest);
  RNG->count = 0;

  memwipe(digest, sizeof(digest));
  memwipe(entropy, sizeof(entropy));

  return 0;
}

#ifndef OTTERY_STRUCT
static
#endif
void
OTTERY_PUBLIC_FN(init)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  pthread_mutex_init(&STATE_FIELD(mutex), NULL);
#endif

  memset(RNG, 0, sizeof(*RNG));

  if (ottery_seed(OTTERY_STATE_ARG_OUT COMMA 0) < 0)
    abort();

  STATE_FIELD(magic) = MAGIC ^ getpid();
}

void
OTTERY_PUBLIC_FN(teardown)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  pthread_mutex_destroy(&STATE_FIELD(mutex));
#endif
  memset(RNG, 0, sizeof(*RNG));
  MAGIC_MAKE_INVALID(STATE_FIELD(magic));
}

#define INIT()                                                  \
  do {                                                          \
    if (UNLIKELY(! MAGIC_OKAY(STATE_FIELD(magic)))) {           \
      OTTERY_PUBLIC_FN(init)(OTTERY_STATE_ARG_OUT);             \
    }                                                           \
  } while (0)

void
OTTERY_PUBLIC_FN(need_reseed)(OTTERY_STATE_ARG_ONLY)
{
  LOCK();
  MAGIC_MAKE_INVALID(STATE_FIELD(magic));
  UNLOCK();
}

#define CHECK()                                                       \
  do {                                                                \
    if (UNLIKELY(RNG->count > RESEED_AFTER_BLOCKS) && !STATE_FIELD(seeding)) { \
      ottery_seed(OTTERY_STATE_ARG_OUT COMMA 1);                      \
    }                                                                 \
  } while (0)

unsigned
OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_ONLY)
{
  unsigned result;
  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG, &result, sizeof(result));
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN(random64)(OTTERY_STATE_ARG_ONLY)
{
  unsigned result;
  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG, &result, sizeof(result));
  UNLOCK();
  return result;
}

unsigned
OTTERY_PUBLIC_FN(random_uniform)(OTTERY_STATE_ARG_FIRST unsigned upper)
{
  const unsigned divisor = UINT_MAX / upper;
  unsigned result;
  LOCK();
  INIT();
  CHECK();
  do {
    ottery_bytes(RNG, &result, sizeof(result));
    result /= divisor;
  } while (result >= upper);
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN(random_uniform64)(OTTERY_STATE_ARG_FIRST uint64_t upper)
{
  const uint64_t divisor = UINT_MAX / upper;
  uint64_t result;
  LOCK();
  INIT();
  CHECK();
  do {
    ottery_bytes(RNG, &result, sizeof(result));
    result /= divisor;
  } while (result >= upper);
  UNLOCK();
  return result;
}

void
OTTERY_PUBLIC_FN(random_bytes)(OTTERY_STATE_ARG_FIRST void *output, size_t n)
{
  LOCK();
  INIT();
  CHECK();
  ottery_bytes(RNG, output, n);
  UNLOCK();
}

#ifdef OTTERY_ENABLE_EGD
int
OTTERY_PUBLIC_FN(set_egd_address)(const struct sockaddr *sa, int socklen)
{
  if (socklen > (int) sizeof(ottery_egd_sockaddr))
      return -1;
  memcpy(&ottery_egd_sockaddr, sa, socklen);
  ottery_egd_socklen = socklen;
  return 0;
}
#endif

