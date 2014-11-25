
// #define OTTERY_STRUCT

#ifdef OTTERY_STRUCT
#define STATE_ARG_ONLY struct ottery_state *state
#define STATE_ARG_FIRST STATE_ARG_ONLY ,
#define STATE_ARG_OUT state
#define RNG (&(state)->rng)
#define STATE_FIELD(fld) (state->fld)
#else
#define STATE_ARG_ONLY void
#define STATE_ARG_FIRST
#define STATE_ARG_OUT
#define RNG &ottery_rng
#define STATE_FIELD(fld) (ottery_ ## fld)
#endif

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "otterylite_rng.h"
#include "otterylite_digest.h"
#include "otterylite_entropy.h"

#define MAGIC 0x6f747472
#define RESEED_AFTER_BLOCKS 2048

#ifdef OTTERY_STRUCT
struct ottery_state {
  pthread_mutex_t mutex;
  unsigned magic;
  struct ottery_rng rng;
};
#else
static pthread_mutex_t ottery_mutex;
static unsigned ottery_magic;
static struct ottery_rng ottery_rng;
#endif

static int
ottery_seed(STATE_ARG_ONLY)
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

  n = ottery_getentropy(entropy+KEYLEN);
  if (n < OTTERY_ENTROPY_MINLEN)
    return -1;

  OTTERY_DIGEST(digest, entropy, n+KEYLEN);

  ottery_setkey(RNG, digest);

  memwipe(digest, sizeof(digest));
  memwipe(entropy, sizeof(entropy));

  return 0;
}

static void
ottery_init(STATE_ARG_ONLY)
{
  pthread_mutex_init(&STATE_FIELD(mutex), NULL);

  memset(RNG, 0, sizeof(*RNG));

  if (ottery_seed(STATE_ARG_OUT) < 0)
    abort();

  STATE_FIELD(magic) = MAGIC ^ getpid();
}

#define INIT()                                          \
  do {                                                  \
    if (STATE_FIELD(magic) != (MAGIC ^ getpid())) {     \
      ottery_init(STATE_ARG_OUT);                       \
    }                                                   \
  } while (0)

#define LOCK()                                  \
  do {                                          \
    pthread_mutex_lock(&STATE_FIELD(mutex));    \
  } while (0)

#define UNLOCK()                                \
  do {                                          \
    pthread_mutex_unlock(&STATE_FIELD(mutex));  \
  } while (0)

#define CHECK()                                                       \
  do {                                                                \
    if (RNG->counter > RESEED_AFTER_BLOCKS) {                         \
      ottery_seed(STATE_ARG_OUT);                                     \
      RNG->counter = 0;                                               \
    }                                                                 \
  } while (0)

unsigned
ottery_st_random(STATE_ARG_ONLY)
{
  unsigned result;
  INIT();
  LOCK();
  ottery_bytes(RNG, &result, sizeof(result));
  UNLOCK();
  return result;
}

uint64_t
ottery_st_random_uint64(STATE_ARG_ONLY)
{
  unsigned result;
  INIT();
  LOCK();
  ottery_bytes(RNG, &result, sizeof(result));
  UNLOCK();
  return result;
}

unsigned
ottery_st_random_range(STATE_ARG_FIRST unsigned upper)
{
  const unsigned divisor = UINT_MAX / upper;
  unsigned result;
  INIT();
  LOCK();
  do {
    ottery_bytes(RNG, &result, sizeof(result));
    result /= divisor;
  } while (result >= upper);
  UNLOCK();
  return result;
}

uint64_t
ottery_st_random_range64(STATE_ARG_FIRST uint64_t upper)
{
  const uint64_t divisor = UINT_MAX / upper;
  uint64_t result;
  INIT();
  LOCK();
  do {
    ottery_bytes(RNG, &result, sizeof(result));
    result /= divisor;
  } while (result >= upper);
  UNLOCK();
  return result;
}

void
ottery_st_random_bytes(STATE_ARG_FIRST void *output, size_t n)
{
  INIT();
  LOCK();
  ottery_bytes(RNG, output, n);
  UNLOCK();
}

/* This should get pulled into its own file before we go to production. */
#include <stdio.h>
#include <sys/time.h>

int main(int c, char **v)
{
  struct timeval tv_start, tv_end, tv_diff;
  u8 block[1024];
  int i;
  const int N=10000;
  unsigned long long ns;
  gettimeofday(&tv_start, NULL);
  for(i=0;i<N;++i) {
    ottery_st_random_bytes(block, 1024);
  }
  gettimeofday(&tv_end, NULL);
  //printf("%u\n", u);
  timersub(&tv_end, &tv_start, &tv_diff);
  ns = tv_diff.tv_sec * 1000000000ull + tv_diff.tv_usec * 1000;
  ns /= N;
  printf("%llu ns per call\n", ns);
  return 0;
}
