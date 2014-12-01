/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

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

#ifndef _WIN32
static void
test_entropy_generic_device(void *arg)
{
  char dir[128] = "/tmp/otterylite_test_XXXXXX";
  char fname[128] = {0};
  char linkname[128] = {0};
  const char junk[] =
    "Here is a sample paragraph that I am testing to make some junk that will "
    "get written into a file to write some unit tests.  I was going to use "
    "an amusing quotation from John Brunner's _The Shockwave Rider_, but "
    "I don't want the headache of figuring out what happens when I try to "
    "do fair use inside cc0.";
  u8 buf[512];
  int fd = -1;
  (void)arg;

  tt_assert(mkdtemp(dir) != NULL);
  tt_assert(strlen(dir) < 128 - 32);
  tt_assert(sizeof(junk) < sizeof(buf));

  snprintf(fname, sizeof(fname), "%s/file", dir);
  snprintf(linkname, sizeof(linkname), "%s/link", dir);

  memset(buf, 0, sizeof(buf));

  /* 1. Reading a nonexistent file is an error. */
  tt_int_op(-1, ==, ottery_getentropy_device_(buf, 32, fname, 0));
  tt_assert(iszero(buf, sizeof(buf)));

  /* 2. But we can read from a file that's there. */
  fd = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0600);
  tt_int_op(fd, >=, 0);
  tt_int_op(sizeof(junk), ==, write(fd, junk, sizeof(junk)));
  close(fd);
  fd = -1;

  tt_int_op(32, ==, ottery_getentropy_device_(buf, 32, fname, 0));
  tt_mem_op(buf, ==, junk, 32);
  tt_assert(iszero(buf+32,32));
  memset(buf, 0, sizeof(buf));

  /* 3. We can't read from it if we insist that it be a device, though. */
  tt_int_op(-1, ==, ottery_getentropy_device_(buf, 32, fname, S_IFCHR));
  tt_assert(iszero(buf, sizeof(buf)));

  /* 4. And we use nofollow, so it won't work if it's a link. */
  tt_assert(0==symlink(fname, linkname));
  tt_int_op(-1, ==, ottery_getentropy_device_(buf, 32, linkname, 0));
  tt_assert(iszero(buf, sizeof(buf)));

  /* 5. EOF shouldn't actually ever happen.  But make sure we handle it.
   */
  tt_int_op(sizeof(junk), ==,
            ottery_getentropy_device_(buf, sizeof(buf), fname, 0));
  tt_mem_op(buf, ==, junk, sizeof(junk));
  tt_assert(iszero(buf+sizeof(junk), sizeof(buf)-sizeof(junk)));

 end:
  if (fd >= 0)
    close(fd);
  if (strlen(fname))
    unlink(fname);
  if (strlen(linkname))
    unlink(linkname);
  if (strlen(dir))
    rmdir(dir);
}
#endif

#define TEST_ENTROPY_DISP_FUNC(name,val)             \
  static int entropy_source_fn_##name##_fails = 0;   \
  static int entropy_source_fn_##name(u8 *out)       \
  {                                                  \
    int f = entropy_source_fn_##name##_fails;        \
    int len = ENTROPY_CHUNK;                         \
    if (f < 0)                                       \
      return f;                                      \
    if (f > 0)                                       \
      len = f;                                       \
    memset(out, (val), len);                         \
    return len;                                      \
  }

TEST_ENTROPY_DISP_FUNC(a,'a')
TEST_ENTROPY_DISP_FUNC(b,'b')
TEST_ENTROPY_DISP_FUNC(c,'c')
TEST_ENTROPY_DISP_FUNC(d,'d')
TEST_ENTROPY_DISP_FUNC(e,'e')

static struct entropy_source test_sources[] = {
  { "a", entropy_source_fn_a, 1,  1, 0 },
  { "b", entropy_source_fn_b, 2,  1, 0 },
  { "c", entropy_source_fn_c, 4,  2, 0 },
  { "d", entropy_source_fn_d, 8,  2, 0 },
  { "e", entropy_source_fn_e, 16, 4, 0 },
};

static void
test_entropy_dispatcher(void *arg)
{
#define N 5
  u8 buf[ENTROPY_CHUNK * (N+1)];
  int status = -10;
  (void)arg;

  memset(buf, 0, sizeof(buf));

  /* Straightforward case. We skip b and d because we had others in the same
   * group */
  tt_int_op(ENTROPY_CHUNK * 3, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));

  tt_mem_op("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "cccccccccccccccccccccccccccccccc"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", ==, buf, ENTROPY_CHUNK*3);
  tt_assert(iszero(buf+ENTROPY_CHUNK*3, ENTROPY_CHUNK*(N-2)));
  tt_int_op(status, ==, 2);


  /* Make a and c fail, so we get b and d and e */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = -2;
  entropy_source_fn_c_fails = -1;
  tt_int_op(ENTROPY_CHUNK * 3, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
            "dddddddddddddddddddddddddddddddd"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", ==, buf, ENTROPY_CHUNK*3);
  tt_assert(iszero(buf+ENTROPY_CHUNK*3, ENTROPY_CHUNK*(N-2)));
  tt_int_op(status, ==, 2);

  /* Make a and c get truncated, so we go on to do them all. */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = 3;
  entropy_source_fn_c_fails = 5;
  tt_int_op(ENTROPY_CHUNK * 3 + 8, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("aaa" "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
            "ccccc" "dddddddddddddddddddddddddddddddd"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", ==, buf, ENTROPY_CHUNK*3 + 8);
  tt_assert(iszero(buf+ENTROPY_CHUNK*3 + 8, ENTROPY_CHUNK*(N-2) - 8));
  tt_int_op(status, ==, 2);

  /* Mark e as best avoided, and d as weak. */
  test_sources[4].flags |= FLAG_AVOID|FLAG_WEAK;
  test_sources[3].flags |= FLAG_WEAK;
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = 0;
  entropy_source_fn_c_fails = 0;
  tt_int_op(ENTROPY_CHUNK * 2, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "cccccccccccccccccccccccccccccccc", ==, buf, ENTROPY_CHUNK*2);
  tt_assert(iszero(buf+ENTROPY_CHUNK*2, ENTROPY_CHUNK*(N-1)));
  tt_int_op(status, ==, 2);

  /* Have c fail. we will do a and d. */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_c_fails = -1;
  tt_int_op(ENTROPY_CHUNK * 2, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "dddddddddddddddddddddddddddddddd", ==, buf, ENTROPY_CHUNK*2);
  tt_assert(iszero(buf+ENTROPY_CHUNK*2, ENTROPY_CHUNK*(N-1)));
  tt_int_op(status, ==, 2);

  /* Have a fail and b not exist. Since d is weak, we will do e too, and the
     status will be 1.
   */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = -1;
  test_sources[1].getentropy_fn = NULL;
  tt_int_op(ENTROPY_CHUNK * 2, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("dddddddddddddddddddddddddddddddd"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", ==, buf, ENTROPY_CHUNK*2);
  tt_assert(iszero(buf+ENTROPY_CHUNK*2, ENTROPY_CHUNK*(N-1)));
  tt_int_op(status, ==, 1);
  test_sources[1].getentropy_fn = entropy_source_fn_b;

  /* Have everything but e fail */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = -1;
  entropy_source_fn_b_fails = -1;
  entropy_source_fn_c_fails = -1;
  entropy_source_fn_d_fails = -1;
  tt_int_op(ENTROPY_CHUNK, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", ==, buf, ENTROPY_CHUNK*1);
  tt_assert(iszero(buf+ENTROPY_CHUNK, ENTROPY_CHUNK*N));
  tt_int_op(status, ==, 1);

  /* Have everything fail. */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_e_fails = -1;
  tt_int_op(0, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_assert(iszero(buf, sizeof(buf)));
  tt_int_op(status, ==, -1);

  /* Have everything truncated. */
  memset(buf, 0, sizeof(buf));
  status = -10;
  entropy_source_fn_a_fails = 5;
  entropy_source_fn_b_fails = 10;
  entropy_source_fn_c_fails = 4;
  entropy_source_fn_d_fails = 8;
  entropy_source_fn_e_fails = 6;
  tt_int_op(33, ==,
            ottery_getentropy_impl(buf, &status, test_sources, N));
  tt_mem_op("aaaaabbbbbbbbbbccccddddddddeeeeee", ==, buf, 33);
  tt_assert(iszero(buf+33, sizeof(buf)-33));
  tt_int_op(status, ==, 0);

 end:
  ;
#undef N
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
  ENTROPY(linux_sysctl, 0),
  ENTROPY(bsd_sysctl, 0),
  ENTROPY(fallback_kludge, 0),
#ifndef _WIN32
  { "generic_device", test_entropy_generic_device, TT_FORK, NULL, NULL },
#endif
  { "dispatcher", test_entropy_dispatcher, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

