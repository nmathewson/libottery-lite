
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
  asm volatile ("" : : : "memory"); /* for good measure. *//* XXXX needless.*/
}
#endif
