
#include "otterylite.c"

#include "tinytest.h"
#include "tinytest_macros.h"

#include "test_blake2.c"
#include "test_chacha.c"

static struct testgroup_t groups[] = {
  { "blake2/", blake2_tests },
  { "chacha_dump/" , chacha_testvectors_tests },
  END_OF_GROUPS
};

int
main(int c, const char **v)
{
  return tinytest_main(c, v, groups);
}
