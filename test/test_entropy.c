
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
  END_OF_TESTCASES
};

