
struct entropy_source_test_data {
  int (*fn)(u8 *out);
  int is_iffy;
};

static void *
setup_entropy_source(const struct testcase_t *tc)
{
  struct entropy_source_test_data *d = malloc(sizeof(*d));
  d->is_iffy = tc->flags & OT_ENT_IFFY;
  d->fn = tc->setup_data;
  return d;
}
static int
cleanup_entropy_source(const struct testcase_t *tc, void *arg)
{
  struct entropy_source_test_data *d = arg;
  (void)tc;
  free(d);
  return 1;
}

static struct testcase_setup_t entropy_source_setup = {
  setup_entropy_source, cleanup_entropy_source
};

static void
test_entropy_source(void *arg)
{
  struct entropy_source_test_data *d = arg;
  u8 buf[ENTROPY_CHUNK*2];
  int n;

  if (d->fn == NULL)
    tt_skip();

  memset(buf, 0, sizeof(buf));
  n = d->fn(buf);
  if (n == -2)
    tt_skip();
  if (n == -1 && d->is_iffy)
    tt_skip();

  tt_int_op(n, ==, ENTROPY_CHUNK);
  tt_assert(!iszero(buf, ENTROPY_CHUNK));
  tt_assert(iszero(buf+ENTROPY_CHUNK, ENTROPY_CHUNK));

 end:
  ;
}

#define ENTROPY(name,flags)                                             \
  { #name, test_entropy_source, TT_FORK|(flags), &entropy_source_setup, \
    (void*) ottery_getentropy_ ## name }

static struct testcase_t entropy_tests[] = {
  ENTROPY(rdrand, 0),
  ENTROPY(getrandom, 0),
  ENTROPY(getentropy, 0),
  ENTROPY(cryptgenrandom, 0),
  ENTROPY(dev_urandom, 0),
  ENTROPY(dev_hwrandom, OT_ENT_IFFY),
  ENTROPY(egd, 0),
  ENTROPY(proc_uuid, 0),
  ENTROPY(linux_sysctl, OT_ENT_IFFY),
  ENTROPY(bsd_sysctl, 0),
  ENTROPY(fallback_kludge, 0),
  END_OF_TESTCASES
};

