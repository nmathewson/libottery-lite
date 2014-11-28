
#define ADD_CHUNK(chunk, len)                   \
  do {                                          \
    if (cp - buf + OTTERY_DIGEST_LEN > 4096) {  \
      ottery_digest(buf, buf, sizeof(buf));     \
      cp = buf + OTTERY_DIGEST_LEN;             \
    }                                           \
    bytes_added += (len);                       \
    ottery_digest(cp, (void*)(chunk), (len));   \
    cp += OTTERY_DIGEST_LEN;                    \
  } while (0)
#define ADD(object)                             \
  do {                                          \
    if (cp - buf + sizeof(object) > 4096) {     \
      ottery_digest(buf, buf, sizeof(buf));     \
      cp = buf + OTTERY_DIGEST_LEN;             \
    }                                           \
    bytes_added += sizeof(object);              \
    memcpy(buf, &object, sizeof(object));       \
    cp += sizeof(object);                       \
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
