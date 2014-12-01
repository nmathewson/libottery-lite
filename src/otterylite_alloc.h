/* otterylite_alloc.h -- allocation for libottery-lite */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#ifndef OTTERYLITE_ALLOC_H_INCLUDED
#define OTTERYLITE_ALLOC_H_INCLUDED

IF_TESTING(static int ottery_testing_make_alloc_fail;)

#if defined(OTTERY_RNG_NO_HEAP) && defined(OTTERY_RNG_NO_MMAP)

/*
  If the RNG is just stored inside the libottery-lite state, then
  we just need to wipe it on startup and teardown.
 */

#define ALLOCATE_RNG(p) memset((p), 0, sizeof(*(p)))
#define FREE_RNG(p) memwipe((p), sizeof(*(p)))
#define set_rng_to_null(p) ((void)0)
#define DECLARE_RNG(name) struct ottery_rng name;

#else

/*
  Otherwise, it's a pointer of some kind and we need functions to allocate and
  free it.
 */

#define ALLOCATE_RNG(p) allocate_rng_(&(p))
#define FREE_RNG(p) free_rng_(&(p))
#define DECLARE_RNG(name) struct ottery_rng *name;

#ifdef OTTERY_RNG_NO_MMAP

/*
  If it's on the heap, we just calloc and free.
 */

static int
allocate_rng_(struct ottery_rng **rng)
{
  IF_TESTING({
      if (ottery_testing_make_alloc_fail)
        return -1;
    })
  *rng = calloc(1, sizeof(**rng));
  return rng ? 0 : -1;
}

static void
free_rng_(struct ottery_rng **rng)
{
  if (*rng) {
    memwipe(*rng, sizeof(**rng));
    free(*rng);
    *rng = NULL;
  }
}

#elif defined(_WIN32)

#define USING_MMAP

/* If we're mmaping it in Windows, this is about the best we can do */

static void *
ottery_mmap_anon(size_t length)
{
  void *ptr;
  HANDLE mapping = CreateFileMapping(INVALID_HANDLE_VALUE,
                                     NULL, /*attributes*/
                                     PAGE_READWRITE,
                                     length >> 32,
                                     length & 0xffffffff,
                                     NULL /* name */);
  if (mapping == NULL)
    return NULL;

  ptr = MapViewOfFile(mapping,
                       PAGE_EXECUTE_READWRITE,
                       0, 0, /* offset */
                       0 /* Extends to end of mapping */);

  CloseHandle(mapping); /* The mapped view holds a reference. */

  return ptr;
}
static void
ottery_munmap_anon(void *p, size_t length)
{
  (void) length;
  UnmapViewOfFile(p);
}

static int
allocate_rng_(struct ottery_rng **rng)
{
  IF_TESTING({
      if (ottery_testing_make_alloc_fail)
        return -1;
  })

  *rng = ottery_mmap_anon(sizeof(**rng));

  if (*rng) {
    VirtualLock(*rng, sizeof(**rng));
    return 0;
  } else {
    return -1;
  }
}

static void
free_rng_(struct ottery_rng **rng)
{
  if (*rng) {
    memwipe(*rng, sizeof(**rng));
    VirtualUnlock(*rng, sizeof(**rng)); /* ???? Is this needed */
    ottery_munmap_anon(*rng, sizeof(rng));
    *rng = NULL;
  }
}

#else


static void *
ottery_mmap_anon(size_t length)
{
  return mmap(NULL, length,
              PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,
              -1, 0);
}
static void
ottery_munmap_anon(void *p, size_t length)
{
  munmap(p, length);
}

/*
  On unix, we try to use mlock and minherit to keep the RNG's guts from
  leaking to swap or to.

  Using mmap has a couple of advantages: First, it ensures that the RNG goes
  on its own page, so that it's convenient to call mlock and minherit on it.
  Second, if the kernel is feeling helpful, it will stick the RNG at a
  hard-to-predict location in the address space.
*/

#define USING_MMAP

static int
allocate_rng_(struct ottery_rng **rng)
{
  IF_TESTING({
      if (ottery_testing_make_alloc_fail)
        return -1;
    })

  *rng = ottery_mmap_anon(sizeof(**rng));
  if (NULL == *rng)
    return -1;

#if defined(INHERIT_ZERO)
#define USING_INHERIT_ZERO
  if (minherit(*rng, sizeof(**rng), INHERIT_ZERO) < 0) {
    ottery_munmap_anon(*rng, sizeof(**rng));
    *rng = NULL;
    return -1;
  }
#elif defined(INHERIT_NONE)
#define USING_INHERIT_NONE
  if (minherit(*rng, sizeof(**rng), INHERIT_NONE) < 0) {
    ottery_munmap_anon(*rng, sizeof(**rng));
    *rng = NULL;
    return -1;
  }
#endif

  mlock(*rng, sizeof(**rng));
  return 0;
}

static void
free_rng_(struct ottery_rng **rng)
{
  if (*rng) {
    memwipe(*rng, sizeof(**rng));
    munlock(*rng, sizeof(**rng)); /* ???? is this necessary? */
    ottery_munmap_anon(*rng, sizeof(**rng));
    *rng = NULL;
  }
}

#endif /* Unix mmap */

#endif /* heap or mmap */

#endif /* OTTERYLITE_ALLOC_H_INCLUDED */

