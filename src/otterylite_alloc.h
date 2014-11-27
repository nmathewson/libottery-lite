
#ifdef OTTERY_RNG_NO_HEAP

#define ALLOCATE_RNG(p) memset((p), 0, sizeof(*(p)))
#define FREE_RNG(p) memwipe((p), sizeof(*(p)))
#define set_rng_to_null(p) ((void)0)
#define DECLARE_RNG(name) struct ottery_rng name;

#else

#define ALLOCATE_RNG(p) allocate_rng_(&(p))
#define FREE_RNG(p) free_rng_(&(p))
#define DECLARE_RNG(name) struct ottery_rng *name;

#ifdef OTTERY_RNG_NO_MMAP

static int
allocate_rng_(struct ottery_rng **rng)
{
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

#else

static int
allocate_rng_(struct ottery_rng **rng)
{
  *rng = mmap(NULL, sizeof(**rng),
              PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,
              -1, 0);
  if (*rng) {
    mlock(*rng, sizeof(**rng));
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
    munlock(*rng, sizeof(**rng)); /* XXXX is this necessary? */
    munmap(*rng, sizeof(**rng));
    *rng = NULL;
  }
}

#endif

#endif

