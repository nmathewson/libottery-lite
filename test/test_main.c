
#include "otterylite.c"

#include "tinytest.h"
#include "tinytest_macros.h"

static int iszero(u8 *, size_t);

#define OT_ENT_IFFY TT_FIRST_USER_FLAG

#include "test_blake2.c"
#include "test_chacha.c"
#include "test_entropy.c"
#include "test_fork.c"
#include "test_rng_core.c"

static int
iszero(u8 *p, size_t n)
{
  while (n--) {
    if (*p++)
      return 0;
  }
  return 1;
}

static struct testgroup_t groups[] = {
  { "blake2/", blake2_tests },
  { "chacha_dump/" , chacha_testvectors_tests },
  { "entropy/", entropy_tests },
  { "fork/", fork_tests },
  { "rng_core/", rng_core_tests },
  END_OF_GROUPS
};

int
main(int c, const char **v)
{
  return tinytest_main(c, v, groups);
}
