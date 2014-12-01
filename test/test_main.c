/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#define OTTERY_BUILDING_TESTS

#include "otterylite.c"

#include "tinytest.h"
#include "tinytest_macros.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

static int iszero(u8 *, size_t);

#define OT_ENT_IFFY TT_FIRST_USER_FLAG

#ifdef OTTERY_STRUCT
#define DECLARE_STATE() struct ottery_state *state
#define INIT_STATE() \
  do { state = malloc(sizeof(*state)); ottery_st_init(state); } while (0)
#define RELEASE_STATE() \
  do { if (state) { ottery_st_teardown(state); free(state); } } while (0)
#else
#define DECLARE_STATE()
#define INIT_STATE()
#define RELEASE_STATE()
#endif

#include "test_blake2.c"
#include "test_chacha.c"
#include "test_entropy.c"
#include "test_fork.c"
#include "test_rng_core.c"
#include "test_shallow.c"
#include "test_egd.c"

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
#ifndef _WIN32
  { "fork/", fork_tests },
#endif
  { "rng_core/", rng_core_tests },
  { "shallow/", shallow_tests },
  { "egd/", egd_tests },
  END_OF_GROUPS
};

int
main(int c, const char **v)
{
  return tinytest_main(c, v, groups);
}
