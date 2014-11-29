/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#if ((defined(__OpenBSD__) && OpenBSD >= 201405 /* 5.5 */))

#define memwipe(p, n) explicit_bzero((p), (n))

#elif defined(_WIN32)

#define memwipe(p, n) SecureZeroMemory((p), (n))

#else

static inline void
memwipe(volatile void *p, size_t n)
{
  volatile char *cp = p;

  while (n--)
    *cp++ = 0;

  /* ???? I think this next part is needless */
  asm volatile ("" : : : "memory"); /* for good measure. */
}
#endif
