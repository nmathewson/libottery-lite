
#include <sys/syscall.h>
#include <linux/random.h>

#define OTTERY_ENTROPY_MAXLEN OTTERY_DIGEST_LEN
#define OTTERY_ENTROPY_MINLEN OTTERY_DIGEST_LEN

static int
getrandom(void *out, size_t n, unsigned flags)
{
  return syscall(__NR_getrandom, out, n, flags);
}

static int
ottery_getentropy(unsigned char *out)
{
  if (getrandom(out, OTTERY_DIGEST_LEN, 0) != OTTERY_DIGEST_LEN)
    return -1;
  return OTTERY_DIGEST_LEN;
}
