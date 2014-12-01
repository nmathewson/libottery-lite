/* otterylite_fallback.h -- fallback kludge for when all other entropy sources
   fail.
 */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

/*
  A buffer we use for grafting entopy together.  We fill up 'buf', keeping
  'cp' pointing towards the next spot that we can write to.  If we would
  overflow, we instead run blake2b on the buffer, put that back at the
  start, and begin again.
 */
struct fallback_entropy_accumulator {
  u8 buf[4096];
  u8 *cp;
  uint64_t bytes_added; /* Tracks how many bytes of imput we actually got */
};

static void
fallback_entropy_accumulator_init(struct fallback_entropy_accumulator *fbe)
{
  memset(fbe, 0, sizeof(*fbe));
  fbe->cp = fbe->buf;
}

/*
  Extract an ENTROPY_CHUNK from 'fbe', and store it in 'out'.  Invalidates
  fbe.
*/
static int
fallback_entropy_accumulator_get_output(
                                  struct fallback_entropy_accumulator *fbe,
                                  u8 *out)
{
  TRACE(("I looked at %llu bytes\n", (unsigned long long)fbe->bytes_added));
  blake2(out, ENTROPY_CHUNK, fbe->buf, sizeof(fbe->buf),
         0x07735, 1);
  memwipe(fbe, sizeof(fbe));
  return ENTROPY_CHUNK;
}

/*
  Add a 'len'-byte blob to 'fbe'.
 */
static void
fallback_entropy_accumulator_add_chunk(
                                  struct fallback_entropy_accumulator *fbe,
                                  const void *chunk,
                                  size_t len)
{
  /* If len > 128, take the digest of it first.  Else just copy it in. */
  const size_t addbytes = len > 128 ? OTTERY_DIGEST_LEN : len;

  if (fbe->cp - fbe->buf + addbytes > 4096) {
    /* We need to digest the buffer first or it won't fit. */
    ottery_digest(fbe->buf, fbe->buf, sizeof(fbe->buf));
    fbe->cp = fbe->buf + OTTERY_DIGEST_LEN;
  }

  if (len > 128)
    ottery_digest(fbe->cp, chunk, len);
  else
    memcpy(fbe->buf, chunk, len);

  fbe->cp += addbytes;
  fbe->bytes_added += len;
}

/*
  Macros for implementing fallback kludges.
 */
#define FBENT_ADD_CHUNK(chunk, len_) \
  fallback_entropy_accumulator_add_chunk(accumulator, (chunk), (len_))
#define FBENT_ADD(object)                                               \
  fallback_entropy_accumulator_add_chunk(accumulator, &(object), sizeof(object))
#define FBENT_ADD_ADDR(ptr)                           \
  do {                                                \
    void *p = (void*)ptr;                             \
    FBENT_ADD(p);                                     \
  } while (0)
#define FBENT_ADD_FN_ADDR(ptr)                                          \
  do {                                                                  \
    int (*fnp)();                                                       \
    uint64_t p = 0;                                                     \
    fnp = (int(*)())(ptr);                                              \
    p = (uint64_t)(void*)fnp;                                           \
    FBENT_ADD(p);                                                       \
  } while (0)

#ifdef _WIN32
#include "otterylite_fallback_win32.h"
#else
#include "otterylite_fallback_unix.h"
#endif

static int
ottery_getentropy_fallback_kludge(u8 *out)
{
  int iter;

  struct fallback_entropy_accumulator fbe;
  fallback_entropy_accumulator_init(&fbe);

  ottery_getentropy_fallback_kludge_nonvolatile(&fbe);
  for (iter = 0; iter < FALLBACK_KLUDGE_ITERATIONS; ++iter)
    ottery_getentropy_fallback_kludge_volatile(iter, &fbe);

  return fallback_entropy_accumulator_get_output(&fbe, out);
}

#undef FBENT_ADD_CHUNK
#undef ADD
#undef FBENT_ADD_FILE
#undef FBENT_ADD_ADDR
#undef FBENT_ADD_FN_ADDR
