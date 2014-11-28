
#ifndef _WIN32 /* Windows has no fork. */

struct fork_test_data {
  int pipefds[2];
  pid_t child;
  int forked;
  int shouldcrash;
};

static void *
setup_fork_data(const struct testcase_t *tc)
{
  struct fork_test_data *d = calloc(1,sizeof(*d));
  (void)tc;
  if (pipe(d->pipefds) < 0) {
    TT_GRIPE(("pipe failed: %s", strerror(errno)));
    free(d);
    return NULL;
  }
  return d;
}
#define IN_PARENT(forkd) do {                   \
    close((forkd)->pipefds[1]);                 \
    (forkd)->pipefds[1] = -1;                   \
    (forkd)->forked = 1;                        \
  } while (0)
#define FORK_OK(forkd) write((forkd)->pipefds[1], "y", 1)
#define FORK_FAIL(forkd) write((forkd)->pipefds[1], "n", 1)
static int
cleanup_fork_data(const struct testcase_t *tc, void *arg)
{
  struct fork_test_data *d = arg;
  char msg;
  int ok, crashed = 0;
  int exit_status = 0;
  int r;
  (void)tc;
  if (d->forked == 0) {
    TT_GRIPE(("Fork never happened"));
    ok = 0;
  } else if ((r = read(d->pipefds[0], &msg, 1)) != 1) {
    if (! d->shouldcrash) {
      if (r < 0)
        TT_GRIPE(("read_failed on %d: %s", d->pipefds[0], strerror(errno)));
      else
        TT_GRIPE(("child died before reporting"));
    }
    ok = 0;
    crashed = 1;
  } else if (msg == 'y') {
    ok = 1;
  } else if (msg == 'n') {
    TT_GRIPE(("Child reported failure"));
    ok = 0;
  } else {
    TT_GRIPE(("Weird message: %d", (int)msg));
  }
  if (d->shouldcrash) {
    ok = crashed;
  }

  waitpid(d->child, &exit_status, 0);

  if (!!d->shouldcrash != !!WIFSIGNALED(exit_status)) {
    TT_GRIPE(("Crash status not as expected."));
    ok = 0;
  }

  close(d->pipefds[0]);
  if (d->pipefds[1] >= 0)
    close(d->pipefds[1]);
  free(d);
  return ok;
}

static struct testcase_setup_t fork_data_setup = {
  setup_fork_data, cleanup_fork_data
};

#ifdef INHERIT_ZERO
static void
test_fork_backend_inherit_zero(void *arg)
{
  /* Let's make sure INHERIT_ZERO works */

  struct fork_test_data *d = arg;
  char *cp ;
  cp = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
  tt_assert(cp);
  memcpy(cp, "Hello", 5);

  tt_int_op(0, ==, minherit(cp, 4096, INHERIT_ZERO));

  if ((d->child = fork())) {
    IN_PARENT(d);
  } else {
    if (!memcmp(cp, "Hello", 5))
      FORK_FAIL(d);
    if (iszero((void*)cp, 5))
      FORK_OK(d);

    FORK_FAIL(d);
    exit(0);
  }
 end:
  if (cp)
    munmap(cp, 4096);
}
#endif

#ifdef INHERIT_NONE
static void
test_fork_backend_inherit_none(void *arg)
{
  struct fork_test_data *d = arg;
  /* Let's make sure INHERIT_NONE works */
  char *cp ;
  cp = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
  tt_assert(cp);
  memcpy(cp, "Hello", 5);

  tt_int_op(0, ==, minherit(cp, 4096, INHERIT_NONE));

  d->shouldcrash = 1;

  if ((d->child = fork())) {
    IN_PARENT(d);
  } else {
    if (!memcmp(cp, "Hello", 5))
      FORK_FAIL(d);

    FORK_FAIL(d);
    exit(0);
  }
 end:
  if (cp)
    munmap(cp, 4096);
}
#endif

static void
test_fork_handling(void *arg)
{
  struct fork_test_data *d = arg;
  u8 buf[64], buf2[32];
  int in_child = 0;
  DECLARE_STATE();

  (void)arg;

#ifndef OTTERY_STRUCT
  tt_int_op(ottery_seed_counter, ==, 0);
#endif

  INIT_STATE();

  OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf, 64);
  tt_int_op(STATE_FIELD(seed_counter), ==, 1);

  if ((d->child = fork())) {
    IN_PARENT(d);
#ifdef USING_ATFORK
    tt_int_op(ottery_fork_count, ==, 0);
#endif
    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf, 32);
    tt_int_op(RNG_PTR->idx, ==, 96);
  } else {
    in_child = 1;
#ifdef USING_ATFORK
    tt_int_op(ottery_fork_count, ==, 1);
#endif
    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf2, 32);
    tt_int_op(STATE_FIELD(seed_counter), ==, 2);
    tt_int_op(RNG_PTR->idx, ==, 32);
    write(d->pipefds[1], buf2, 32);

    /* Make sure we only reinit once! */
    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf2, 32);
    tt_int_op(STATE_FIELD(seed_counter), ==, 2);

    FORK_OK(d);
    exit(0);
  }
  tt_int_op(32, ==, read(d->pipefds[0], buf2, 32));

  tt_mem_op(buf, !=, buf2, 32);
  tt_int_op(STATE_FIELD(seed_counter), ==, 1);

 end:
  RELEASE_STATE();
  if (in_child) {
    FORK_FAIL(d);
    exit(0);
  }
}

static struct testcase_t fork_tests[] = {
#ifdef INHERIT_NONE
  { "backend/inherit_none", test_fork_backend_inherit_none,
    TT_FORK, &fork_data_setup, NULL },
#endif
#ifdef INHERIT_ZERO
  { "backend/inherit_zero", test_fork_backend_inherit_zero,
    TT_FORK, &fork_data_setup, NULL },
#endif
  { "handling", test_fork_handling, TT_FORK, &fork_data_setup, NULL },

  END_OF_TESTCASES
};

#endif /* _WIN32 */
