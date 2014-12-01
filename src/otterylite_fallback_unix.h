/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#ifndef __linux__
#if defined(__OpenBSD__)
#include <netinet/tcp_timer.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/udp_var.h>
#include <netinet/tcp_var.h>
#endif

/*
  Here are a bunch of things we can ask clock_gettime() about.  There probably
  isn't more than a bit of entropy in each, but we can hope.
 */
static const int clock_ids[] = {
#ifdef CLOCK_MONOTONIC
  CLOCK_MONOTONIC,
#endif
#ifdef CLOCK_MONOTONIC_RAW
  CLOCK_MONOTONIC,
#endif
#ifdef CLOCK_REALTIME
  CLOCK_REALTIME,
#endif
#ifdef CLOCK_REALTIME_PRECISE
  CLOCK_REALTIME_PRECISE,
#endif
#ifdef CLOCK_BOOTTIME
  CLOCK_BOOTTIME,
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
  CLOCK_PROCESS_CPUTIME_ID,
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
  CLOCK_THREAD_CPUTIME_ID,
#endif
#ifdef CLOCK_UPTIME
  CLOCK_UPTIME,
#endif
#ifdef CLOCK_UPTIME_PRECISE
  CLOCK_UPTIME_PRECISE,
#endif
#ifdef CLOCK_VIRTUAL
  CLOCK_VIRTUAL,
#endif
};
#define N_CLOCK_IDS (sizeof(clock_ids) / sizeof(clock_ids[0]))

#ifndef __linux__
#define USING_SYSCTL

#define MIB2(a,b)         { 2, { a, b, 0, 0, 0, 0 } }
#define MIB3(a,b,c)       { 3, { a, b, c, 0, 0, 0 } }
#define MIB4(a,b,c,d)     { 4, { a, b, c, d, 0, 0 } }
#define MIB5(a,b,c,d,e)   { 5, { a, b, c, d, e, 0 } }
#define MIB6(a,b,c,d,e,f) { 6, { a, b, c, d, e, f } }

/* FFFF mib lists for more platforms.
 */
/*
  Here are a bunch of things we can ask sysctl about.
 */
static const struct {
  int miblen;
  const int mib[6];
} miblist[] = {
#if defined(CTL_KERN)
#  if defined(KERN_FILE)
  MIB2( CTL_KERN, KERN_FILE ),
#  endif
#  if defined(KERN_PROC) && defined (KERN_PROC_ALL)
  MIB3( CTL_KERN, KERN_PROC, KERN_PROC_ALL ),
#  endif
#  if defined(KERN_VNODE)
  MIB2( CTL_KERN, KERN_VNODE ), /* X  */
#  endif
#endif
#if defined(CTL_NET)
  MIB4( CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_STATS ),
  MIB4( CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_STATS ),
  MIB4( CTL_NET, PF_INET, IPPROTO_IP, IPCTL_STATS ),
#  if defined(PF_INET6)
  MIB4( CTL_NET, PF_INET6, IPPROTO_TCP, IPV6CTL_STATS ), /* X */
  MIB4( CTL_NET, PF_INET6, IPPROTO_UDP, IPV6CTL_STATS ), /* X */
  MIB4( CTL_NET, PF_INET6, IPPROTO_IP, IPV6CTL_STATS ), /*X*/
#  endif
#endif
#if defined(CTL_VM)
#  if defined(VM_LOADAVG)
  MIB2( CTL_VM, VM_LOADAVG ),
#  endif
#endif
};

#define MIB_LIST_LEN (sizeof(miblist) / sizeof(miblist[0]))
#endif /* !__linux__ */

/*
  Read the contents of 'fname' into 'fbe'.  If 'tailbytes' is nonzero, read
  only the last 'tailbytes' bytes of the file.  Never read more than 1MB.
 */
static void
fallback_entropy_accumulator_add_file(struct fallback_entropy_accumulator *fbe,
                                      const char *fname,
                                      int tailbytes)
{
  unsigned char tmp[1024];
  int fd;
  int n;
  ssize_t max_to_read = 1024*1024;
  struct stat st;
  fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return;
  if (fstat(fd, &st) == 0) {
    fallback_entropy_accumulator_add_chunk(fbe, &st, sizeof(st));
  }
  if (tailbytes) {
    lseek(fd, 2, -tailbytes);
    max_to_read = tailbytes;
  }

  while (1) {
    n = (int) read(fd, tmp, sizeof(tmp));
    if (n <= 0)
      break;
    fallback_entropy_accumulator_add_chunk(fbe, tmp, n);
    if (n >= max_to_read)
      break;
    max_to_read -= n;
  }
}

static void
fallback_entropy_add_clocks(struct fallback_entropy_accumulator *accumulator)
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0)
    FBENT_ADD(tv);

#ifdef USING_OTTERY_CPUTICKS
  {
    uint64_t t = ottery_cputicks();
    FBENT_ADD(t);
  }
#endif

#ifdef CLOCK_MONOTONIC
  {
    int i;
    struct timespec delay = { 0, 10 };
    for (i = 0; i < (int)N_CLOCK_IDS; ++i)
      {
        struct timespec ts;
        if (clock_gettime(clock_ids[i], &ts) == 0)
          {
            FBENT_ADD(ts);
            nanosleep(&delay, NULL);
          }
      }
  }
#endif

}

#define FBENT_ADD_FILE(fname)                                   \
  fallback_entropy_accumulator_add_file(accumulator, (fname), 0)
#define FBENT_ADD_FILE_TAIL(fname, bytes)                               \
  fallback_entropy_accumulator_add_file(accumulator, (fname), (bytes))

/*
  Poll less-volatile kludgy entropy sources into 'accumulator'.

  This stuff doesn't change enough over the couse of a second that it's worth
  polling more than once.

  Here we use a mixture of things that are (we hope) hard to predict for
  somebody not running locally on the same host, and things that should be
  hard for *anyone* to predict.
 */
static void
ottery_getentropy_fallback_kludge_nonvolatile(
                     struct fallback_entropy_accumulator *accumulator)
{
  int i;

#ifdef __APPLE__
  {
    /* This isn't volatile at all. */
    struct timespec ts = {0, 10*1000*1000 }; /* Give up after 10 ms */
    uuid_t id;
    if (0 == gethostuuid(id, &ts)) {
      FBENT_ADD(id);
    }
  }
#endif

  fallback_entropy_add_clocks(accumulator);

  /*
     The pid and the current time; what could go wrong?  (Ask the Debian
     openssl maintainers.
  */
  {
    pid_t pid;
    pid = getppid();
    FBENT_ADD(pid);
    pid = getpid();
    FBENT_ADD(pid);
    pid = getpgid(0);
    FBENT_ADD(pid);
  }
  /*
    Try to tail the logs a bit. This is pretty easy to find out if you're on
    the same host, but not if you're not.
   */
  FBENT_ADD_FILE_TAIL("/var/log/system.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/cron.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/messages", 16384);
  fallback_entropy_add_clocks(accumulator);
  FBENT_ADD_FILE_TAIL("/var/log/secure", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/lastlog", 8192);
  FBENT_ADD_FILE_TAIL("/var/log/wtmp", 8192);
#ifdef __APPLE__
  FBENT_ADD_FILE_TAIL("/var/log/appfirewall.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/wifi.log", 16384);
#endif
  {
    /* This isn't volatile at all. */
    long hostid = gethostid();
    FBENT_ADD(hostid);
}
#ifdef __APPLE__
  FBENT_ADD_FILE("/var/run/utmpx");
  FBENT_ADD_FILE("/var/run/resolv.conf");
#endif
#ifdef __linux__
  /*
    It's a little weird to be reading from proc at this point; if we
    have a working proc, how did /proc/sys/kernel/random/uuid fail for us?
  */
  FBENT_ADD_FILE("/proc/cmdline");
  FBENT_ADD_FILE("/proc/iomem");
  FBENT_ADD_FILE("/proc/keys");
  fallback_entropy_add_clocks(accumulator);
  FBENT_ADD_FILE("/proc/modules");
  FBENT_ADD_FILE("/proc/mounts");
  FBENT_ADD_FILE("/proc/net/unix");
  FBENT_ADD_FILE("/proc/self/cmdline");
  FBENT_ADD_FILE("/proc/self/environ");
  FBENT_ADD_FILE("/proc/self/stack");
  fallback_entropy_add_clocks(accumulator);
  FBENT_ADD_FILE("/proc/scsi/device_info");
  FBENT_ADD_FILE("/proc/version");
  FBENT_ADD_FILE("/proc/kallsyms");
  {
    char fname_buf[64];
    for (i = 0; i < 32; ++i)
      {
        int n = snprintf(fname_buf, sizeof(fname_buf),
                         "/proc/irc/%d/spurious", i);
        if (n > 0 && n < (int)sizeof(fname_buf))
          FBENT_ADD_FILE(fname_buf);
      }
  }
#endif
  fallback_entropy_add_clocks(accumulator);

  /* Add in addresses from this library, from the socket library (if
     separate), from libc, and from the stack. */
  FBENT_ADD_FN_ADDR(ottery_getentropy_fallback_kludge_nonvolatile);
  FBENT_ADD_FN_ADDR(socket);
  FBENT_ADD_FN_ADDR(printf);
  FBENT_ADD_ADDR(&i);
}

/* How many times to run the volatile poll? */
#define FALLBACK_KLUDGE_ITERATIONS 8

static void
ottery_getentropy_fallback_kludge_volatile(
                     int iteration,
                     struct fallback_entropy_accumulator *accumulator)
{
  int i;
  (void)iteration;

#ifdef OTTERY_X86
  {
    unsigned regs[4];
    for (i = 0; i < 16; ++i)
      {
        cpuid_(i, regs);
        FBENT_ADD(regs);
      }
  }
#endif
#ifdef __MACH__
  {
    uint64_t t = mach_absolute_time();
    FBENT_ADD(t);
  }
#endif
#if !defined(__APPLE__) && !defined(__OpenBSD__)
  {
    ucontext_t uc;
    if (getcontext(&uc) == 0)
      FBENT_ADD(uc);
  }
#endif
#ifdef __linux__
  /* On linux, this can change frequently.  But see notes above. */
  FBENT_ADD_FILE("/proc/diskstats");
  FBENT_ADD_FILE("/proc/interrupts");
  FBENT_ADD_FILE("/proc/loadavg");
  FBENT_ADD_FILE("/proc/locks");
  FBENT_ADD_FILE("/proc/meminfo");
  FBENT_ADD_FILE("/proc/net/dev");
  FBENT_ADD_FILE("/proc/net/udp");
  FBENT_ADD_FILE("/proc/net/tcp");
  FBENT_ADD_FILE("/proc/pagetypeinfo");
  FBENT_ADD_FILE("/proc/sched_debug");
  FBENT_ADD_FILE("/proc/self/stat");
  FBENT_ADD_FILE("/proc/self/statm");
  FBENT_ADD_FILE("/proc/self/syscall");
  FBENT_ADD_FILE("/proc/stat");
  FBENT_ADD_FILE("/proc/sysvipc/shm");
  FBENT_ADD_FILE("/proc/timer_list");
  FBENT_ADD_FILE("/proc/uptime");
  FBENT_ADD_FILE("/proc/vmstat");
  FBENT_ADD_FILE("/proc/zoneinfo");
#endif

  {
    /* Add a miniscule delay, to try to juice the clocks a little. */
    struct timespec ts = { 0, 100 };
    nanosleep(&ts, NULL);
  }

  {
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0)
      FBENT_ADD(ru);
    if (getrusage(RUSAGE_CHILDREN, &ru) == 0)
      FBENT_ADD(ru);
  }

  {
    struct stat st;
    struct statvfs stv;
    if (stat(".", &st) == 0)
      FBENT_ADD(st);
    if (stat("/", &st) == 0)
      FBENT_ADD(st);
    if (statvfs(".", &stv) == 0)
      FBENT_ADD(stv);
    if (statvfs("/", &stv) == 0)
      FBENT_ADD(stv);
  }

#ifdef USE_SYSCTL
  for (i = 0; i < MIB_LIST_LEN; ++i)
    {
      u8 tmp[1024];
      size_t n = sizeof(tmp);
      int r = sysctl((int*)miblist[i].mib, miblist[i].miblen, tmp, &n, NULL, 0);
      if (r < 0 || n > sizeof(tmp)) {
        TRACE(("mib %d said %d: %s.\n", i, r, strerror(errno)));
        continue;
      }
      TRACE(("mib %d okay; %d bytes\n", i, (int)n));
      FBENT_ADD_CHUNK(tmp, n);
    }
#endif

  fallback_entropy_add_mmap(accumulator);
}
