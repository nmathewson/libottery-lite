/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#include "otterylite.h"
#include <stdio.h>

#include <sys/time.h>

#ifndef timersub
#  define timersub(a,b,c)                               \
  do {                                                  \
    (c)->tv_sec = (a)->tv_sec - (b)->tv_sec;            \
    (c)->tv_usec = (a)->tv_usec - (b)->tv_usec;         \
    if ((c)->tv_usec < 0) {                             \
      (c)->tv_usec += 1000000;                          \
      --(c)->tv_sec;                                    \
    }                                                   \
  } while (0)
#endif

int
main(int c, char **v)
{
  struct timeval tv_start, tv_end, tv_diff;
  char block[1024];
  int i;
  const int N = 10000;
  uint64_t ns;

  (void)c; (void)v;

  gettimeofday(&tv_start, NULL);
  for (i = 0; i < N; ++i)
    {
      ottery_random();
    }
  gettimeofday(&tv_end, NULL);
  timersub(&tv_end, &tv_start, &tv_diff);
  ns = tv_diff.tv_sec * (uint64_t)1000000000 + tv_diff.tv_usec * 1000;
  ns /= N;
  printf("%ld ns per call to ottery_random()\n", (long)ns);


  gettimeofday(&tv_start, NULL);
  for (i = 0; i < N; ++i)
    {
      ottery_random_buf(block, 1024);
    }
  gettimeofday(&tv_end, NULL);
  timersub(&tv_end, &tv_start, &tv_diff);
  ns = tv_diff.tv_sec * (uint64_t)1000000000 + tv_diff.tv_usec * 1000;
  ns /= N;
  printf("%ld ns per call to ottery_random_buf(1024)\n", (long)ns);

  return 0;
}

