/*
   To the extent possible under law, Nick Mathewson has waived all copyright and
   related or neighboring rights to libottery-lite, using the creative commons
   "cc0" public domain dedication.  See doc/cc0.txt or
   <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "otterylite.h"

static int
write3(unsigned u)
{
  unsigned char b3[3];

  b3[0] = u & 0xff;
  b3[1] = (u >> 8) & 0xff;
  b3[2] = (u >> 16) & 0xff;
  return write(1, b3, 3) == 3;
}
static int
write7(uint64_t u64)
{
  uint32_t u32 = (uint32_t)u64;

  return write(1, &u32, 4) == 4 &&
         write3((unsigned)(u64 >> 32));
}

int
main(int argc, char **argv)
{
  unsigned char buf[9000];
  int yes_really = 0;
  int type = 0;
  int n = 0, i;
  int ok = 1;

  for (i = 1; i < argc; ++i)
    {
      if (!strcmp(argv[i], "--yes-really"))
        {
          yes_really = 1;
        }
      else if (!strcmp(argv[i], "-m"))
        {
          if (i + 1 < argc)
            type = atoi(argv[i + 1]);
        }
    }

  if (!yes_really || type < 0 || type > 8)
    {
      fprintf(stderr, "Hi! This program is designed to spew an infinite stream "
              "of random bytes to stdout, for testing. If you just called it "
              "wondering what it did... that's what. If you really want to blit "
              "randomness to stderr, say --yes-really\n");
      fprintf(stderr,
              "You can get output generated in different ways, too:\n"
              " -m 0  -- 1024 bytes at a time\n"
              " -m 1  -- 1775 bytes at a time\n"
              " -m 2  -- 9000 bytes at a time\n"
              " -m 3  -- one unsigned at a time\n"
              " -m 4  -- one u64 at a time\n"
              " -m 5  -- three bytes at a time, using uniform\n"
              " -m 6  -- three bytes at a time, using uniform with cutoff\n"
              " -m 7  -- seven bytes at a time, using uniform64\n"
              " -m 8  -- seven bytes at a time, using uniform64 with cutoff\n");
      return 1;
    }

  switch (type)
    {
    case 0:
      n = 1024; goto case_buf;

    case 1:
      n = 1775; goto case_buf;

    case 2:
      n = 9000; goto case_buf;
case_buf:
      while (ok)
        {
          ottery_random_buf(buf, n);
          ok = write(1, buf, n) == n;
        }
      break;

    case 3:
      while (ok)
        {
          unsigned u = ottery_random();
          ok = write(1, &u, sizeof(u)) == sizeof(u);
        }
      break;

    case 4:
      while (ok)
        {
          uint64_t u64 = ottery_random64();
          ok = write(1, &u64, sizeof(u64)) == sizeof(u64);
        }
      break;

    case 5:
      while (ok)
        {
          unsigned u = ottery_random_uniform(1 << 24);
          ok = write3(u);
        }
      break;

    case 6:
      while (ok)
        {
          unsigned u = ottery_random_uniform((1 << 24) + 200000);
          if (u >= (1 << 24))
            continue;
          ok = write3(u);
        }
      break;

    case 7:
      while (ok)
        {
          uint64_t u64 = ottery_random_uniform64(((uint64_t)1) << 56);
          ok = write7(u64);
        }
      break;

    case 8:
      while (ok)
        {
          uint64_t u64 = ottery_random_uniform64((((uint64_t)1) << 56) +
                                                 (((uint64_t)1) << 54));
          if (u64 >= (((uint64_t)1) << 56))
            continue;
          ok = write7(u64);
        }
      break;
    }

  return 0;
}

