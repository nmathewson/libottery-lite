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

#define MAGIC 0x6f747472u
#define RNG_MAGIC 0x00480A01 /*ohai*/
#define RESEED_AFTER_BLOCKS 2048

#ifdef _WIN32
#define ottery_getpid() 0
#else
#define ottery_getpid() getpid()
#endif

#define MAGIC_MAKE_INVALID(m) ((m) = 0 ^ ottery_getpid())
#define MAGIC_OKAY(m) ((m == (MAGIC ^ ottery_getpid())))

#ifdef OTTERY_STRUCT
struct ottery_state {
  DECLARE_LOCK(mutex)
  unsigned magic;
  int seeding;
  int entropy_status;
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
#define LOCK()                                  \
  do {                                          \
    GET_STATIC_LOCK(ottery_mutex);              \
  } while (0)
#define UNLOCK()                                \
  do {                                          \
    RELEASE_STATIC_LOCK(ottery_mutex);          \
  } while (0)
#endif

#if OTTERY_DIGEST_LEN < KEYLEN
/* If we ever need to use a 32-byte digest, we can pad it or stretch it
 * or something */
#error "We need a digest that is longer then the key we mean to use."
#endif

static int
ottery_seed(OTTERY_STATE_ARG_FIRST int release_lock)
{
  int n, new_status=0;
  unsigned char entropy[KEYLEN + OTTERY_ENTROPY_MAXLEN];
  unsigned char digest[OTTERY_DIGEST_LEN];

  ottery_bytes(RNG, entropy, KEYLEN);

  STATE_FIELD(seeding) = 1;
  RNG->count = 0;

  if (release_lock)
    UNLOCK();
  /* Release the lock in this section, since it can take a while to get
   * entropy. */

  n = ottery_getentropy(entropy + KEYLEN, &new_status);

  if (release_lock)
    LOCK();

  if (n < OTTERY_ENTROPY_MINLEN)
    return -1;

  ottery_digest(digest, entropy, n + KEYLEN);

  STATE_FIELD(entropy_status) = new_status;
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
OTTERY_PUBLIC_FN2 (init)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  INIT_LOCK(&STATE_FIELD(mutex));
#endif

  /* This leaks memory postfork */
  if (ALLOCATE_RNG(RNG) < 0)
    abort();

  RNG->magic = RNG_MAGIC;

  if (ottery_seed(OTTERY_STATE_ARG_OUT COMMA 0) < 0)
    abort();

  STATE_FIELD(magic) = MAGIC ^ ottery_getpid();
}

void
OTTERY_PUBLIC_FN2 (teardown)(OTTERY_STATE_ARG_ONLY)
{
#ifdef OTTERY_STRUCT
  DESTROY_LOCK(&STATE_FIELD(mutex));
#endif
  FREE_RNG(RNG);
  MAGIC_MAKE_INVALID(STATE_FIELD(magic));
}

#if defined(USING_MMAP) && defined(INHERIT_ZERO)
/* If we really have inherit_zero, then we can */
#define RNG_MAGIC_OKAY (RNG->magic == RNG_MAGIC)
#else
#define RNG_MAGIC_OKAY 1
#endif

#define INIT()                                          \
  do {                                                  \
    if (UNLIKELY(!MAGIC_OKAY(STATE_FIELD(magic) || !RNG_MAGIC_OKAY))) { \
      OTTERY_PUBLIC_FN2(init) (OTTERY_STATE_ARG_OUT);    \
    }                                                   \
  } while (0)

void
OTTERY_PUBLIC_FN2 (need_reseed)(OTTERY_STATE_ARG_ONLY)
{
  LOCK();
  MAGIC_MAKE_INVALID(STATE_FIELD(magic));
  UNLOCK();
}

#define CHECK()                                                         \
  do {                                                                  \
    if (UNLIKELY(RNG->count > RESEED_AFTER_BLOCKS) && !STATE_FIELD(seeding)) { \
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
  ottery_bytes(RNG, &result, sizeof(result));
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN (random64)(OTTERY_STATE_ARG_ONLY)
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
OTTERY_PUBLIC_FN (random_uniform)(OTTERY_STATE_ARG_FIRST unsigned upper)
{
  const unsigned divisor = UINT_MAX / upper;
  unsigned result;

  LOCK();
  INIT();
  CHECK();
  do
    {
      ottery_bytes(RNG, &result, sizeof(result));
      result /= divisor;
    } while (result >= upper);
  UNLOCK();
  return result;
}

uint64_t
OTTERY_PUBLIC_FN (random_uniform64)(OTTERY_STATE_ARG_FIRST uint64_t upper)
{
  const uint64_t divisor = UINT_MAX / upper;
  uint64_t result;

  LOCK();
  INIT();
  CHECK();
  do
    {
      ottery_bytes(RNG, &result, sizeof(result));
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
  ottery_bytes(RNG, output, n);
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
    /* XXXX what if we're seeding at the same time? */

    u8 buf[OTTERY_DIGEST_LEN * 2];
    u8 digest[OTTERY_DIGEST_LEN];

    ottery_bytes(RNG, buf, OTTERY_DIGEST_LEN);
    ottery_digest(buf + OTTERY_DIGEST_LEN, inp, n); /* XXXX could overflow */
    ottery_digest(digest, buf, sizeof(buf));
    ottery_setkey(RNG, digest);
    RNG->count = 0;

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
  INIT(); /* but never abort. XXXX */
  CHECK(); /* But never abort XXXXX */
  r = STATE_FIELD(entropy_status);
  UNLOCK();
  return r;
}
