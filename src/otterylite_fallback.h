
#define ADD_CHUNK(chunk, len_)                                  \
  do {                                                          \
    size_t len = (len_);                                        \
    size_t addbytes = len > 128 ? OTTERY_DIGEST_LEN : len;      \
    if (cp - buf + addbytes > 4096) {                           \
      ottery_digest(buf, buf, sizeof(buf));                     \
      cp = buf + OTTERY_DIGEST_LEN;                             \
    }                                                           \
    bytes_added += len;                                         \
    if (len > 128)                                              \
      ottery_digest(cp, (void*)(chunk), len);                   \
    else                                                        \
      memcpy(buf, chunk, len);                                  \
    cp += addbytes;                                             \
  } while (0)
#define ADD(object)                             \
  do {                                          \
    ADD_CHUNK(&(object), sizeof(object));       \
  } while (0)
#define ADD_ADDR(ptr)                           \
  do {                                          \
    void *p = (void*)ptr;                       \
    ADD(p);                                     \
  } while (0)
#define ADD_FN_ADDR(ptr)                        \
  do {                                          \
    uint64_t p = (uint64_t)&ptr;                \
    ADD(p);                                     \
  } while (0)

#ifdef _WIN32
#include "otterylite_fallback_win32.h"
#else
#include "otterylite_fallback_unix.h"
#endif
