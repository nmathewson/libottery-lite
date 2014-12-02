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

#ifdef OTTERY_X86
#define USING_OTTERY_CPUTICKS
#ifdef _MSC_VER
#define ottery_cputicks() __rdtsc()
#else
static uint64_t
ottery_cputicks(void)
{
  uint32_t lo, hi;
  __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
  return lo | ((uint64_t)hi << 32);
}
#endif
#endif

static void fallback_entropy_add_clocks(struct fallback_entropy_accumulator *accumulator);

#ifdef USING_MMAP
/*
  Try to extract entropy from the system's virtual memory manager and memory
  fragmentation status.

  We're going to mmap some blocks of different (prime) sizes, and add their
  addresses and the time it took us to access them.

  (I first saw this trick in libressl-portable.)

 */
#define N_SIZES 9
static void
fallback_entropy_add_mmap(struct fallback_entropy_accumulator *accumulator)
{
  int i;
  const long sizes[N_SIZES] = { 7, 1, 11, 3, 17, 2, 5, 3, 13 };
  char *pointers[N_SIZES];
  size_t offset;
#ifndef _WIN32
  long pagesize = sysconf(_SC_PAGESIZE);
#else
  long pagesize;
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  pagesize = (long) si.dwPageSize;
#endif
  offset = pagesize * 37;

  for (i = 0; i < N_SIZES; ++i) {
    size_t this_size = pagesize * sizes[i];
    pointers[i] = ottery_mmap_anon(this_size);
    if (pointers[i])
      pointers[i][offset % this_size]++;
    fallback_entropy_add_clocks(accumulator);
    offset *= 10103;
  }

  fallback_entropy_accumulator_add_chunk(accumulator,
                                         pointers, sizeof(pointers));

  for (i = 0; i < N_SIZES; ++i) {
    size_t this_size = pagesize * sizes[i];
    if (pointers[i])
      ottery_munmap_anon(pointers[i], this_size);
  }
}
#undef N_SIZES
#endif

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
    uintptr_t p = 0;                                                    \
    fnp = (int(*)())(ptr);                                              \
    p = (uintptr_t)(void*)fnp;                                          \
    FBENT_ADD(p);                                                       \
  } while (0)

#ifdef _WIN32
#include "otterylite_fallback_win32.h"
#else
#include "otterylite_fallback_unix.h"
#endif

static int
ottery_getentropy_fallback_kludge(u8 *out, unsigned *flags_out)
{
  int iter;

  struct fallback_entropy_accumulator fbe;
  *flags_out = 0;

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
