/*
   To the extent possible under law, Nick Mathewson has waived all copyright and
   related or neighboring rights to libottery-lite, using the creative commons
   "cc0" public domain dedication.  See doc/cc0.txt or
   <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
 */

#include "otterylite.c"
#include <stdio.h>

#include <sys/time.h>

#ifndef timersub
#  define timersub(a, b, c)                               \
  do {                                                  \
      (c)->tv_sec = (a)->tv_sec - (b)->tv_sec;            \
      (c)->tv_usec = (a)->tv_usec - (b)->tv_usec;         \
      if ((c)->tv_usec < 0) {                             \
          (c)->tv_usec += 1000000;                          \
          --(c)->tv_sec;                                    \
        }                                                   \
    } while (0)
#endif

typedef struct {
  struct timeval tv;
  uint64_t ticks;
} btimer_t;

typedef struct {
  uint64_t ns;
  uint64_t ticks;
} btimer_diff_t;

static void
btimer_gettime(btimer_t *t)
{
  gettimeofday(&t->tv, NULL);
#ifdef USING_OTTERY_CPUTICKS
  t->ticks = ottery_cputicks();
#endif
}
static void
btimer_diff(btimer_diff_t *diff, const btimer_t *start, const btimer_t *end)
{
  struct timeval tv_tmp;

  timersub(&end->tv, &start->tv, &tv_tmp);
  diff->ns = tv_tmp.tv_sec * (uint64_t)1000000000 + tv_tmp.tv_usec * 1000;
#ifdef USING_OTTERY_CPUTICKS
  diff->ticks = end->ticks - start->ticks;
#endif
}

static const char *
diff_fmt(btimer_diff_t *diff, uint64_t divisor)
{
  static char buf[64];
  const char *unit = "ns";
  long quantity =  (long)diff->ns / divisor;
  if (quantity > 10000) {
    quantity /= 1000;
    unit = "us";
  }
  if (quantity > 10000) {
    quantity /= 1000;
    unit = "ms";
  }

#ifdef USING_OTTERY_CPUTICKS
  snprintf(buf, sizeof(buf), "%ld %s (%ld ticks)", quantity, unit, (long)diff->ticks / divisor);
#else
  snprintf(buf, sizeof(buf), "%ld %s", quantity, unit);
#endif
  return buf;
}


int
main(int c, char **v)
{
  btimer_t t_start, t_end;
  btimer_diff_t t_diff;
  u8 block[4096];
  int i, j;
  const int N = 100000;
  const int NENT = 100;

  (void)c; (void)v;

  btimer_gettime(&t_start);
  for (i = 0; i < N * 100; ++i)
    {
      ottery_random();
    }
  btimer_gettime(&t_end);
  btimer_diff(&t_diff, &t_start, &t_end);
  printf("%s per call to ottery_random()\n", diff_fmt(&t_diff, N * 100));


  btimer_gettime(&t_start);
  for (i = 0; i < N * 10; ++i)
    {
      ottery_random_buf(block, 1024);
    }
  btimer_gettime(&t_end);
  btimer_diff(&t_diff, &t_start, &t_end);
  printf("%s per call to ottery_random_buf(1024)\n", diff_fmt(&t_diff, N * 10));

  btimer_gettime(&t_start);
  for (i = 0; i < N; ++i)
    {
      chacha20_blocks(block, OTTERY_BUFLEN / 64, block);
    }
  btimer_gettime(&t_end);
  btimer_diff(&t_diff, &t_start, &t_end);
  printf("%s per buffer refill\n", diff_fmt(&t_diff, N));

  for (j = 0; j < (int)N_ENTROPY_SOURCES; ++j)
    {
      const struct entropy_source *es = &entropy_sources[j];
      unsigned flags=0;
      unsigned bad=0;
      if (!es->getentropy_fn)
        continue;
      btimer_gettime(&t_start);
      for (i = 0; i < NENT; ++i)
        {
          if (es->getentropy_fn(block, &flags) < ENTROPY_CHUNK)
            ++bad;
        }
      btimer_gettime(&t_end);
      btimer_diff(&t_diff, &t_start, &t_end);
      if (bad == (int)NENT)
        continue;
      printf("%s per %s\n", diff_fmt(&t_diff, NENT), es->name);
      if (bad)
        printf("%u failed.\n", bad);
    }

  return 0;
}

