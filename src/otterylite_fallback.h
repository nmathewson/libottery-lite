
struct fallback_entropy_accumulator {
  u8 buf[4096];
  u8 *cp;
  uint64_t bytes_added;
};

static void
fallback_entropy_accumulator_init(struct fallback_entropy_accumulator *fbe)
{
  memset(fbe, 0, sizeof(*fbe));
  fbe->cp = fbe->buf;
}

static int
fallback_entropy_accumulator_get_output(
                                  struct fallback_entropy_accumulator *fbe,
                                  u8 *out)
{
  TRACE(("I looked at %llu bytes\n", (unsigned long long)fbe->bytes_added));
  blake2_noendian(out, ENTROPY_CHUNK, fbe->buf, sizeof(fbe->buf),
                  0x07735, 1);
  memwipe(fbe, sizeof(fbe));
  return ENTROPY_CHUNK;
}

static void
fallback_entropy_accumulator_add_chunk(
                                  struct fallback_entropy_accumulator *fbe,
                                  const void *chunk,
                                  size_t len)
{
  size_t addbytes = len > 128 ? OTTERY_DIGEST_LEN : len;

  if (fbe->cp - fbe->buf + addbytes > 4096) {
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
    uint64_t p = 0;                                                     \
    memcpy(&p, ptr, sizeof(ptr)<sizeof(p) ? sizeof(ptr) : sizeof(p));   \
    FBENT_ADD(p);                                                       \
  } while (0)

#ifdef _WIN32
#include "otterylite_fallback_win32.h"
#else
#include "otterylite_fallback_unix.h"
#endif

#undef FBENT_ADD_CHUNK
#undef ADD
#undef FBENT_ADD_FILE
#undef FBENT_ADD_ADDR
#undef FBENT_ADD_FN_ADDR
