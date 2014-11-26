
#include "otterylite.h"
#include "otterylite-impl.h"
#include "otterylite_wipe.h"
#include "otterylite_digest.h"

#define BLAKE2S_OUTBYTES 32
#define BLAKE2B_OUTBYTES 64
#include "blake2-kat.h"

static int
test_kat(void)
{
#define ITERATIONS 256
  int i;
  /* this will fail if the platform is big-endian. Don't worry. */
  u8 buf[KAT_LENGTH];
  u8 out[64];

  for (i = 0; i < KAT_LENGTH; ++i)
    {
      buf[i] = (u8)i;
    }

  for (i = 0; i < ITERATIONS; ++i)
    {
      const u8 * expected =
#if WORDBITS >= 64
        blake2b_kat[i]
#else
        blake2s_kat[i]
#endif
        ;
      blake2_noendian(out, BLAKE2_MAX_OUTPUT, buf, i, 0, 0);

      if (memcmp(out, expected, BLAKE2_MAX_OUTPUT))
        {
          printf("Error: %d\n", i);
          return -1;
        }
    }

  return 0;
}

static int
nonzero(u8 *p, size_t n)
{
  while (n--) {
    if (*p++)
      return 1;
  }
  return 0;
}

static int
test_output_len(void)
{
  u8 msg[] = "hi";
  u8 out[128];

  memset(out, 0, sizeof(out));

  if (-1 != blake2_noendian(out, 128, msg, 3, 0, 0))
    return -1;
  if (nonzero(out, sizeof(out)))
    return -1;
  if (-1 != blake2_noendian(out, 0, msg, 3, 0, 0))
    return -1;
  if (nonzero(out, sizeof(out)))
    return -1;
  if (1 != blake2_noendian(out, 1, msg, 3, 0, 0))
    return -1;
  if (nonzero(out+1, sizeof(out)-1))
    return -1;
  if (20 != blake2_noendian(out, 20, msg, 3, 0, 0))
    return -1;
  if (nonzero(out+20, sizeof(out)-20))
    return -1;
  if (21 != blake2_noendian(out+20, 21, msg, 3, 0, 0))
    return -1;
  if (!memcmp(out, out+20, 20))
    return -1;

  return 0;
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  if (test_kat() < 0)
    return 1;
  if (test_output_len() < 0)
    return 1;

  return 0;
}
