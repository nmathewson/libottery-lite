
#define BLAKE2S_OUTBYTES 32
#define BLAKE2B_OUTBYTES 64
#include "blake2-kat.h"

static void
test_kat(void *arg)
{
  (void)arg;

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
#if BLAKE2_WORDBITS == 64
      const u8 *expected = blake2b_kat[i];
#else
      const u8 *expected = blake2s_kat[i];
#endif

      tt_int_op(BLAKE2_MAX_OUTPUT, ==,
                blake2_noendian(out, BLAKE2_MAX_OUTPUT, buf, i, 0, 0));

      tt_mem_op(out, ==, expected, BLAKE2_MAX_OUTPUT);
    }

 end:
  ;
}

static void
test_output_len(void *arg)
{
  u8 msg[] = "hi";
  u8 out[128];
  (void)arg;

  memset(out, 0, sizeof(out));

  tt_int_op(-1, ==, blake2_noendian(out, 128, msg, 3, 0, 0));
  tt_assert(iszero(out, sizeof(out)));
  tt_int_op(-1, ==, blake2_noendian(out, 0, msg, 3, 0, 0));
  tt_assert(iszero(out, sizeof(out)));
  tt_int_op(1, ==, blake2_noendian(out, 1, msg, 3, 0, 0));
  tt_assert(iszero(out+1, sizeof(out)-1));
  tt_int_op(20, ==, blake2_noendian(out, 20, msg, 3, 0, 0));
  tt_assert(iszero(out+20, sizeof(out)-20));
  tt_assert(! iszero(out, 20));
  tt_int_op(21, ==, blake2_noendian(out+20, 21, msg, 3, 0, 0));
  tt_mem_op(out, !=, out+20, 20);
 end:
  ;
}

static struct testcase_t blake2_tests[] = {
  { "kat", test_kat, 0, NULL, NULL },
  { "output_len", test_output_len, 0, NULL, NULL },
  END_OF_TESTCASES
};

