
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

#define MIB(mib) { mib_ ## mib, (sizeof(mib_ ## mib) / sizeof(mib_ ## mib[0])) }

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
  MIB(files),
  MIB(procs),
  MIB(vnode),
  MIB(inet_tcp),
  MIB(inet_udp),
  MIB(inet_ip),
  MIB(inet6_tcp),
  MIB(inet6_udp),
  MIB(inet6_ip),
  MIB(loadavg),
};
#define USE_SYSCTL
#endif

#ifdef USE_SYSCTL
#define MIB_LIST_LEN (sizeof(miblist) / sizeof(miblist[0]))
#endif

static int
ottery_getentropy_fallback_kludge(unsigned char *out)
{
  unsigned char buf[4096], *cp=buf;
  int iter, i;
  uint64_t bytes_added = 0;

  memset(buf, 0, sizeof(buf));

#define ADD_FILE(fname)                                         \
  do {                                                          \
    unsigned char tmp[1024];                                    \
    int fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);    \
    int n;                                                      \
    if (fd >= 0) {                                              \
      while (1) {                                               \
        n = read(fd, tmp, sizeof(tmp));                         \
        if (n <= 0)                                             \
          break;                                                \
        ADD_CHUNK(tmp, n);                                      \
      }                                                         \
      close(fd);                                                \
    }                                                           \
  } while (0)

  (void)i;

  {
  pid_t pid;
  pid = getppid();
  ADD(pid);
  pid = getpid();
  ADD(pid);
  pid = getpgid(0);
  ADD(pid);
}
  {
  long hostid = gethostid();
  ADD(hostid);
}
#ifdef __linux__
  ADD_FILE("/proc/cmdline");
  ADD_FILE("/proc/iomem");
  ADD_FILE("/proc/keys");
  ADD_FILE("/proc/modules");
  ADD_FILE("/proc/mounts");
  ADD_FILE("/proc/net/unix");
  ADD_FILE("/proc/self/cmdline");
  ADD_FILE("/proc/self/environ");
  ADD_FILE("/proc/self/stack");
  ADD_FILE("/proc/scsi/device_info");
  ADD_FILE("/proc/version");
  ADD_FILE("/proc/kallsyms");
  {
  char fname_buf[64];
  for (i = 0; i < 32; ++i)
    {
  int n = snprintf(fname_buf, sizeof(fname_buf), "/proc/irc/%d/spurious", i);
  if (n > 0 && n < (int)sizeof(fname_buf))
    ADD_FILE(fname_buf);
}
}
#endif

  ADD_FN_ADDR(ottery_getentropy_fallback_kludge);
  ADD_FN_ADDR(socket);
  ADD_FN_ADDR(printf);
  ADD_ADDR(&iter);

  for (iter = 0; iter < 8; ++iter)
    {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0)
    ADD(tv);

#ifdef CLOCK_MONOTONIC
  {
  struct timespec delay = { 0, 10 };
  for (i = 0; i < (int)N_CLOCK_IDS; ++i)
    {
  struct timespec ts;
  if (clock_gettime(clock_ids[i], &ts) == 0)
    {
  ADD(ts);
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
  ADD(regs);
}
}
#endif
#ifdef __MACH__
  {
  uint64_t t = mach_absolute_time();
  ADD(t);
}
#endif
#ifndef __APPLE__
  {
  ucontext_t uc;
  if (getcontext(&uc) == 0)
    ADD(uc);
}
#endif
#ifdef __linux__
  ADD_FILE("/proc/diskstats");
  ADD_FILE("/proc/interrupts");
  ADD_FILE("/proc/loadavg");
  ADD_FILE("/proc/locks");
  ADD_FILE("/proc/meminfo");
  ADD_FILE("/proc/net/dev");
  ADD_FILE("/proc/net/udp");
  ADD_FILE("/proc/net/tcp");
  ADD_FILE("/proc/pagetypeinfo");
  ADD_FILE("/proc/sched_debug");
  ADD_FILE("/proc/self/stat");
  ADD_FILE("/proc/self/statm");
  ADD_FILE("/proc/self/syscall");
  ADD_FILE("/proc/stat");
  ADD_FILE("/proc/sysvipc/shm");
  ADD_FILE("/proc/timer_list");
  ADD_FILE("/proc/uptime");
  ADD_FILE("/proc/vmstat");
  ADD_FILE("/proc/zoneinfo");
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
    ADD(ru);
  if (getrusage(RUSAGE_CHILDREN, &ru) == 0)
    ADD(ru);
}

  {
  struct stat st;
  struct statvfs stv;
  if (stat(".", &st) == 0)
    ADD(st);
  if (stat("/", &st) == 0)
    ADD(st);
  if (statvfs(".", &stv) == 0)
    ADD(stv);
  if (statvfs("/", &stv) == 0)
    ADD(stv);
}

#ifdef USE_SYSCTL
  for (i = 0; i < MIB_LIST_LEN; ++i)
    {
  u8 tmp[1024];
  size_t n = sizeof(tmp);
  int r = sysctl((int*)miblist[i].mib, miblist[i].miblen, tmp, &n, NULL, 0);
  if (r < 0 || n > sizeof(tmp))
    continue;
  ADD_CHUNK(tmp, n);
}
#endif
  /* XXXX try some mmap trickery like libressl does */
}

#undef ADD

  blake2_noendian(out, ENTROPY_CHUNK, buf, sizeof(buf), 0x101010, 0);

  TRACE(("I looked at %llu bytes\n", (unsigned long long)bytes_added));

  memwipe(buf, sizeof(buf));
  return ENTROPY_CHUNK;
}

