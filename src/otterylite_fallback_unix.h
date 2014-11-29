/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
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

#define MIBLIST_ENTRY(mib) \
  { mib_ ## mib, (sizeof(mib_ ## mib) / sizeof(mib_ ## mib[0])) }

#ifdef __APPLE__
static const int mib_files[] = { CTL_KERN, KERN_FILE };
static const int mib_procs[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
static const int mib_vnode[] = { CTL_KERN, KERN_VNODE };
static const int mib_inet_tcp[] =
  { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_STATS };
static const int mib_inet_udp[] =
  { CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_STATS };
static const int mib_inet_ip[] =
  { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_STATS };
static const int mib_inet6_tcp[] =
  { CTL_NET, PF_INET6, IPPROTO_TCP, TCPCTL_STATS };
static const int mib_inet6_udp[] =
  { CTL_NET, PF_INET6, IPPROTO_UDP, UDPCTL_STATS };
static const int mib_inet6_ip[] =
  { CTL_NET, PF_INET6, IPPROTO_IP, IPCTL_STATS };
static const int mib_loadavg[] =
  { CTL_VM, VM_LOADAVG };
static const struct {
  const int *mib;
  int miblen;
} miblist[] = {
  MIBLIST_ENTRY(files),
  MIBLIST_ENTRY(procs),
  MIBLIST_ENTRY(vnode),
  MIBLIST_ENTRY(inet_tcp),
  MIBLIST_ENTRY(inet_udp),
  MIBLIST_ENTRY(inet_ip),
  MIBLIST_ENTRY(inet6_tcp),
  MIBLIST_ENTRY(inet6_udp),
  MIBLIST_ENTRY(inet6_ip),
  MIBLIST_ENTRY(loadavg),
};
#define USE_SYSCTL
#endif

#ifdef USE_SYSCTL
#define MIB_LIST_LEN (sizeof(miblist) / sizeof(miblist[0]))
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

static void
fallback_entropy_accumulator_add_file(struct fallback_entropy_accumulator *fbe,
                                      const char *fname,
                                      size_t tailbytes)
{
  unsigned char tmp[1024];
  int fd;
  int n;
  size_t max_to_read = 1024*1024;
  struct stat st;
  fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return;
  if (fstat(fd, &st) == 0) {
    fallback_entropy_accumulator_add_chunk(fbe, &st, sizeof(st));
  }
  if (tailbytes) {
    lseek(fd, 2, 0-(ssize_t)tailbytes);
    max_to_read = tailbytes;
  }

  while (1) {
    n = read(fd, tmp, sizeof(tmp));
    if (n <= 0)
      break;
    fallback_entropy_accumulator_add_chunk(fbe, tmp, n);
    if (n >= max_to_read)
      break;
    max_to_read -= n;
  }
}

static int
ottery_getentropy_fallback_kludge(unsigned char *out)
{
  struct fallback_entropy_accumulator fbe, *accumulator = &fbe;
  int iter, i;

  fallback_entropy_accumulator_init(accumulator);

#define FBENT_ADD_FILE(fname)                                   \
  fallback_entropy_accumulator_add_file(accumulator, (fname), 0)
#define FBENT_ADD_FILE_TAIL(fname, bytes)                               \
  fallback_entropy_accumulator_add_file(accumulator, (fname), (bytes))

  (void)i;

#ifdef __APPLE__
  {
    struct timespec ts = {0, 10*1000*1000 };
    uuid_t id;
    if (0 == gethostuuid(id, &ts)) {
      FBENT_ADD(id);
    }
  }
#endif

  {
  pid_t pid;
  pid = getppid();
  FBENT_ADD(pid);
  pid = getpid();
  FBENT_ADD(pid);
  pid = getpgid(0);
  FBENT_ADD(pid);
  }
  FBENT_ADD_FILE_TAIL("/var/log/system.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/cron.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/messages", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/secure", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/lastlog", 8192);
  FBENT_ADD_FILE_TAIL("/var/log/wtmp", 8192);

#ifdef __APPLE__
  FBENT_ADD_FILE_TAIL("/var/log/appfirewall.log", 16384);
  FBENT_ADD_FILE_TAIL("/var/log/wifi.log", 16384);
#endif
  {
  long hostid = gethostid();
  FBENT_ADD(hostid);
}
#ifdef __APPLE__
  FBENT_ADD_FILE("/var/run/utmpx");
  FBENT_ADD_FILE("/var/run/resolv.conf");
#endif
#ifdef __linux__
  FBENT_ADD_FILE("/proc/cmdline");
  FBENT_ADD_FILE("/proc/iomem");
  FBENT_ADD_FILE("/proc/keys");
  FBENT_ADD_FILE("/proc/modules");
  FBENT_ADD_FILE("/proc/mounts");
  FBENT_ADD_FILE("/proc/net/unix");
  FBENT_ADD_FILE("/proc/self/cmdline");
  FBENT_ADD_FILE("/proc/self/environ");
  FBENT_ADD_FILE("/proc/self/stack");
  FBENT_ADD_FILE("/proc/scsi/device_info");
  FBENT_ADD_FILE("/proc/version");
  FBENT_ADD_FILE("/proc/kallsyms");
  {
  char fname_buf[64];
  for (i = 0; i < 32; ++i)
    {
  int n = snprintf(fname_buf, sizeof(fname_buf), "/proc/irc/%d/spurious", i);
  if (n > 0 && n < (int)sizeof(fname_buf))
    FBENT_ADD_FILE(fname_buf);
}
}
#endif

  FBENT_ADD_FN_ADDR(ottery_getentropy_fallback_kludge);
  FBENT_ADD_FN_ADDR(socket);
  FBENT_ADD_FN_ADDR(printf);
  FBENT_ADD_ADDR(&iter);

  for (iter = 0; iter < 8; ++iter)
    {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0)
    FBENT_ADD(tv);

#ifdef CLOCK_MONOTONIC
  {
  struct timespec delay = { 0, 10 };
  for (i = 0; i < (int)N_CLOCK_IDS; ++i)
    {
  struct timespec ts;
  if (clock_gettime(clock_ids[i], &ts) == 0)
    {
  FBENT_ADD(ts);
  clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
}
}
}
#endif
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
#ifndef __APPLE__
  {
  ucontext_t uc;
  if (getcontext(&uc) == 0)
    FBENT_ADD(uc);
}
#endif
#ifdef __linux__
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

#ifdef CLOCK_MONOTONIC
  {
  struct timespec ts = { 0, 100 };
  clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}
#endif

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
  if (r < 0 || n > sizeof(tmp))
    continue;
  FBENT_ADD_CHUNK(tmp, n);
}
#endif
  /* FFFF try some mmap trickery like libressl does */
}

  return fallback_entropy_accumulator_get_output(accumulator, out);
}
