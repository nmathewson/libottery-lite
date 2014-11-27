
#include "otterylite.h"
#include <stdio.h>

#include <sys/time.h>

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

