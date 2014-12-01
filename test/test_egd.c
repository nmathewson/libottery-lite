/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#ifndef _WIN32

#include <sys/un.h>

const u8 notsorandom[] =
  "This fixed string is the reason why you should not use this module "
  "as an actual EGD server. I'm going to make sure it's at least 255 "
  "bytes, though, which is why I am still typing. I ought to be hacking "
  "something else right now, but I have this thing about test coverage. "
  "If you've never gotten a nontrivial project to 100% coverage, I can't "
  "explain it; you need to feel it yourself.";

static void run_egd_server(int fd_listen, int fd_out)
  __attribute__((noreturn));

static void
run_egd_server(int fd_listen, int fd_out)
{
  u8 data[16];
  struct sockaddr_storage ss;
  int n;
  socklen_t slen = sizeof(ss);
  int fd_in = accept(fd_listen, (struct sockaddr*)&ss, &slen);
  if (fd_in < 0)
    goto fail;

  n = (int) recv(fd_in, data, sizeof(data), 0);

  if (n != 2)
    goto fail;
  if (data[0] != 1)
    goto fail;

  assert(sizeof(notsorandom) >= 255);
  n = (int) send(fd_in, notsorandom, data[1], 0);
  if (n != data[1])
    goto fail;
  closesocket(fd_in);
  closesocket(fd_listen);
  write(fd_out, "Y", 1);
  exit(0);

 fail:
  write(fd_out, "N", 1);
  exit(1);
}

static void
test_egd_success(void *arg)
{
  int pipefds[2] = {-1,-1};
  struct sockaddr_un sun;
  char dir[128] = "/tmp/otterylite_test_XXXXXX";
  char fname[128] = {0};
  int listener = -1;
  pid_t pid;
  int r, exitstatus=0;
  u8 buf[64];
  unsigned flags;

  (void)arg;


  tt_assert(mkdtemp(dir) != NULL);
  tt_int_op(-1, ==, ottery_egd_socklen);
  tt_int_op(-1, ==, OTTERY_PUBLIC_FN(set_egd_address)((struct sockaddr*)&sun, 16384));

  tt_assert(strlen(dir) < 128 - 32);
  snprintf(fname, sizeof(fname), "%s/fifo", dir);
  tt_int_op(strlen(fname), <, sizeof(sun.sun_path));

  tt_int_op(-2, ==, ottery_getentropy_egd(buf, &flags)); /* not turned on. */

  memset(&sun, 0, sizeof(sun));
  sun.sun_family = -1; /* Bad family */
  tt_int_op(0, ==, OTTERY_PUBLIC_FN(set_egd_address)((struct sockaddr*)&sun, sizeof(sun)));
  tt_int_op(-1, ==, ottery_getentropy_egd(buf, &flags)); /* bad family */

  sun.sun_family = AF_UNIX;
  memcpy(sun.sun_path, fname, strlen(fname)+1);
  listener = socket(AF_UNIX, SOCK_STREAM, 0);
  tt_int_op(listener, >=, 0);

  tt_int_op(0, ==, pipe(pipefds));

  tt_int_op(0, ==, OTTERY_PUBLIC_FN(set_egd_address)((struct sockaddr*)&sun, sizeof(sun)));

  tt_int_op(sizeof(sun), ==, ottery_egd_socklen);

  tt_int_op(-1, ==, ottery_getentropy_egd(buf, &flags)); /* nobody listening yet */

  tt_int_op(bind(listener, (struct sockaddr*)&sun, sizeof(sun)), ==, 0);
  tt_int_op(listen(listener, 16), ==, 0);

  if ((pid = fork())) {
    /* parent */
    close(listener);
    close(pipefds[1]);
  } else {
    /* child */
    close(pipefds[0]);
    run_egd_server(listener, pipefds[1]);
    exit(1);
  }

  memset(buf, 0, sizeof(buf));
  r = ottery_getentropy_egd(buf, &flags);
  tt_int_op(r, ==, ENTROPY_CHUNK);
  tt_assert(iszero(buf + ENTROPY_CHUNK, sizeof(buf) - ENTROPY_CHUNK));
  tt_assert(! iszero(buf, ENTROPY_CHUNK));
  tt_mem_op(buf, ==, notsorandom, ENTROPY_CHUNK);

  r = (int)read(pipefds[0], buf, 1);
  tt_int_op(r, ==, 1);
  waitpid(pid, &exitstatus, 0);
  tt_int_op(exitstatus, ==, 0);
  tt_int_op(buf[0], ==, 'Y');

  tt_int_op(0, ==, OTTERY_PUBLIC_FN(set_egd_address)(NULL, 0));
  tt_int_op(-1, ==, ottery_egd_socklen);

 end:
  if (pipefds[0] >= 0)
    close(pipefds[0]);
  if (pipefds[1] >= 1)
    close(pipefds[1]);
  if (*fname)
    unlink(fname);
  rmdir(dir);
}
#endif

static struct testcase_t egd_tests[] = {
#ifndef _WIN32
  { "basic", test_egd_success, TT_FORK, NULL, NULL },
#endif
  END_OF_TESTCASES
};

