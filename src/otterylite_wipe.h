/* otterylite_wipe.h -- declarations for a secure deletion function. */
/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/
#ifndef OTTERYLITE_H_WIPE_INCLUDED
#define OTTERYLITE_H_WIPE_INCLUDED

/*
  When a compiler determines that an assigned-to object is about to disappear,
  it's allowed to eliminate the assignment.  That's a problem if the reason
  you're assigning to the thing is to clear sensitive data from memory.

  There are many interfaces to deal with this; see below for two of them,
  and a workaround that should work on all the gccs and clangs.

  C11 defines a memset_s() function that would also do the job here.

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
#endif /* OTTERYLITE_H_WIPE_INCLUDED */
