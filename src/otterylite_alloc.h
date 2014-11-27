
#ifdef OTTERY_RNG_NO_HEAP

#define ALLOCATE_RNG(p) memset((p), 0, sizeof(*(p)))
#define FREE_RNG(p) memwipe((p), sizeof(*(p)))
#define set_rng_to_null(p) ((void)0)
#define DECLARE_RNG(name) struct ottery_rng name;

#else

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
#define ALLOCATE_RNG(p) allocate_rng_(&(p))
#define FREE_RNG(p) free_rng_(&(p))
#define DECLARE_RNG(name) struct ottery_rng *name;

#endif

