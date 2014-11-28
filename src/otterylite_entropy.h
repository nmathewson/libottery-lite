/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#define ENTROPY_CHUNK 32
#define OTTERY_ENTROPY_MINLEN 32

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#if defined(OTTERY_X86)
#define ottery_getentropy_rdrand_OUTLEN ENTROPY_CHUNK
#define RDRAND_MAXATTEMPTS 32
static int
rdrand_ll_(uint32_t *therand)
{
  unsigned char status;
  __asm volatile (".byte 0x0F, 0xC7, 0xF0 ; setc %1"
                  : "=a" (*therand), "=qm" (status));

  return (status) == 1 ? 0 : -1;
}
static int
rdrand_(uint32_t *therand)
{
  int i;

  for (i = 0; i < RDRAND_MAXATTEMPTS; ++i)
    {
      if (rdrand_ll_(therand) == 0)
        return 0;
    }
  return -1;
}
static void
cpuid_(int index, unsigned result[4])
{
  unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

#ifdef OTTERY_X86_64
  __asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (index));
#else
  __asm volatile (
                  "xchgl %%ebx, %1; cpuid; xchgl %%ebx, %1"
                  : "=a" (eax), "=r" (ebx), "=c" (ecx), "=d" (edx)
                  : "0" (index)
                  : "cc");
#endif
  result[0] = eax;
  result[1] = ebx;
  result[2] = ecx;
  result[3] = edx;
}

static int
cpuid_says_rdrand_supported_(void)
{
  unsigned result[4];

  cpuid_(1, result);
  return 0 != (result[2] & (1u << 30));
}
static int
ottery_getentropy_rdrand(unsigned char *output)
{
  int i;

  if (!cpuid_says_rdrand_supported_())
    return -1; /* RDRAND not supported. */
  for (i = 0; i < ENTROPY_CHUNK / 4; ++i, output += 4)
    {
      if (rdrand_((uint32_t*)output) < 0)
        return -1;
    }
  return ENTROPY_CHUNK;
}

#else
#define ottery_getentropy_rdrand NULL
#define ottery_getentropy_rdrand_OUTLEN 0
#endif

#if ((defined(__OpenBSD__) && OpenBSD >= 201411 /* 5.6 */))
#define ottery_getentropy_getentropy_OUTLEN ENTROPY_CHUNK
static int
ottery_getentropy_getentropy(unsigned char *out)
{
  return getentropy(out, ENTROPY_CHUNK) == 0 ? ENTROPY_CHUNK : -1;
}
#else
#define ottery_getentropy_getentropy NULL
#define ottery_getentropy_getentropy_OUTLEN 0
#endif

#if defined(__linux__) && defined(__NR_getrandom)
#define ottery_getentropy_getrandom_OUTLEN ENTROPY_CHUNK
static int
ottery_getrandom_ll_(void *out, size_t n, unsigned flags)
{
  return syscall(__NR_getrandom, out, n, flags);
}
static int
ottery_getrandom_(void *out, size_t n, unsigned flags)
{
  int r;

  do
    {
      r = ottery_getrandom_ll_(out, n, flags);
      if (r == (int)n)
        return n;
    } while (r == -EINTR);
  return -1;
}
static int
ottery_getentropy_getrandom(unsigned char *out)
{
  if (ottery_getrandom_(out, ENTROPY_CHUNK, 0) != ENTROPY_CHUNK)
    return -1;
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_getrandom NULL
#define ottery_getentropy_getrandom_OUTLEN 0
#endif

#if defined(_WIN32)
#define ottery_getentropy_cryptgenrandom_OUTLEN ENTROPY_CHUNK
static int
ottery_getentropy_cryptgenrandom(unsigned char *out)
{
  int n = -1;
  HCRYPTPROV h = 0;

  if (!CryptAcquireContext(&h, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    return -1;

  if (!CryptGenRandom(h, ottery_getentropy_cryptgenrandom_OUTLEN, out))
    goto out;

  n = ottery_getentropy_cryptgenrandom_OUTLEN;

 out:
  CryptReleaseContext(h, 0);
  return n;
}
#else
#define ottery_getentropy_cryptgenrandom NULL
#define ottery_getentropy_cryptgenrandom_OUTLEN 0
#endif

#ifndef _WIN32
#define ottery_getentropy_dev_urandom_OUTLEN ENTROPY_CHUNK
#define ottery_getentropy_dev_hwrandom_OUTLEN ENTROPY_CHUNK
static int
ottery_getentropy_device_(unsigned char *out, int len,
                          const char *fname,
                          unsigned need_mode_flags)
{
  int fd;
  int r, output = 0, remain = len;
  struct stat st;

  fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return -1;
  if (fstat(fd, &st))
    goto out;
  if ((st.st_mode & need_mode_flags) != need_mode_flags)
    goto out;
  while (remain)
    {
      r = read(fd, out, remain);
      if (r == -1)
        {
          if (errno == EINTR)
            continue;
          output = -1;
          goto out;
        }
      else if (r == 0)   /* EOF */
        {
          break;
        }
      out += r;
      output += r;
      remain -= r;
    }

 out:
  close(fd);
  return output;
}
static int
ottery_getentropy_dev_urandom(unsigned char *out)
{
#define TRY(fname)                                                      \
  do {                                                                  \
    r = ottery_getentropy_device_(out, ENTROPY_CHUNK, fname, S_IFCHR);  \
    if (r == ENTROPY_CHUNK)                                             \
      return r;                                                         \
  } while (0)
  int r;
#if defined(__sun) || defined(sun)
  TRY("/devices/pseudo/random@0:urandom");
#endif
#ifdef __OpenBSD__
  TRY("/dev/srandom");
#else
  TRY("/dev/urandom");
#endif
  TRY("/dev/random");
  return -1;
}
static int
ottery_getentropy_dev_hwrandom(unsigned char *out)
{
  int r;

  TRY("/dev/hwrandom");
  return -1;
}
#undef TRY
#else
#define ottery_getentropy_dev_urandom NULL
#define ottery_getentropy_dev_hwrandom NULL
#define ottery_getentropy_dev_urandom_OUTLEN 0
#define ottery_getentropy_dev_hwrandom_OUTLEN 0
#endif

#ifdef __linux__
#define ottery_getentropy_proc_uuid_OUTLEN (37 * 2)
static int
ottery_getentropy_proc_uuid(unsigned char *out)
{
  int n = 0, r, i;

  /* Each call yields 37 bytes, containing 16 actual bytes of entropy. Make
   * two calls to get 32 bytes of entropy. */
  for (i = 0; i < 2; ++i)
    {
      r = ottery_getentropy_device_(out, 37, "/proc/sys/kernel/random/uuid", 0);
      if (r < 0)
        return -1;
      n += r;
    }
  return n;
}
#else
#define ottery_getentropy_proc_uuid NULL
#define ottery_getentropy_proc_uuid_OUTLEN 0
#endif

#ifndef OTTERY_DISABLE_EGD
#define ottery_getentropy_egd_OUTLEN ENTROPY_CHUNK
static struct sockaddr_storage ottery_egd_sockaddr;
static int ottery_egd_socklen = -1;
#ifndef _WIN32
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket(s) close(s)
#endif
static int
ottery_getentropy_egd(unsigned char *out)
{
  SOCKET sock;
  char msg[2];
  int result = -1, n_read = 0;

  if (ottery_egd_socklen < 0)
    return -1;

  sock = socket(ottery_egd_sockaddr.ss_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sock == INVALID_SOCKET)
    return -1;

  if (connect(sock, (struct sockaddr*)&ottery_egd_sockaddr, ottery_egd_socklen) < 0)
    goto out;

  msg[0] = 1; /* read, nonblocking */
  msg[1] = ENTROPY_CHUNK; /* request size */
  if (send(sock, msg, 2, 0) != 2)
    goto out;

  while (n_read < ENTROPY_CHUNK)
    {
      int r = recv(sock, (void*)out, ENTROPY_CHUNK - n_read, 0);
      if (r < 0)
        goto out;
      else if (r == 0)
        break;

      n_read += r;
    }

  result = n_read;

 out:
  closesocket(sock);
  return result;
}
#undef SOCKET
#else
#define ottery_getentropy_egd NULL
#define ottery_getentropy_egd_OUTLEN 0
#endif

#if defined(__linux__)
#define ottery_getentropy_linux_sysctl_OUTLEN (37 * 2)
static int
ottery_getentropy_linux_sysctl(unsigned char *out)
{
  int mib[] = { CTL_KERN, KERN_RANDOM, RANDOM_UUID };
  int n_read = 0, i;

  memset(out, 0, 74);
  for (i = 0; i < 2; ++i)
    {
      size_t n = 37;
      int r = sysctl(mib, 3, out, &n, NULL, 0);
      if (r < 0 || n > 37)
        return -1;
      n_read += n;
    }
  return n_read;
}
#else
#define ottery_getentropy_linux_sysctl NULL
#define ottery_getentropy_linux_sysctl_OUTLEN 0
#endif

#if defined(CTL_KERN) && defined(KERN_ARND)
#define ottery_getentropy_bsd_sysctl_OUTLEN ENTROPY_CHUNK
static int
ottery_getentropy_bsd_sysctl(unsigned char *out)
{
  int mib[] = { CTL_KERN, CTL_ARND };

  /* I hear that some BSDs don't like returning anything but sizeof(unsigned)
   * bytes at a time from this one. */
  for (i = 0; i < ENTROPY_CHUNK; i += sizeof(unsigned))
    {
      int n = sizeof(unsigned);
      if (sysctl(mib, 2, out, &n, NULL, 0) == -1 || n <= 0)
        return -1;
      out += n;
    }
}
#else
#define ottery_getentropy_bsd_sysctl NULL
#define ottery_getentropy_bsd_sysctl_OUTLEN 0
#endif

#if !defined(_WIN32) && !defined(OTTERY_DISABLE_FALLBACK_RNG)
#define ottery_getentropy_fallback_kludge_OUTLEN OTTERY_DIGEST_LEN

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
  unsigned char buf[4096], *cp;
  int iter, i;
  uint64_t bytes_added = 0;

  memset(buf, 0, sizeof(buf));

#define ADD_CHUNK(chunk, len)                   \
  do {                                          \
    if (cp - buf + OTTERY_DIGEST_LEN > 4096) {  \
      ottery_digest(buf, buf, sizeof(buf));     \
      cp = buf + OTTERY_DIGEST_LEN;             \
    }                                           \
    bytes_added += (len);                       \
    ottery_digest(cp, (chunk), (len));          \
    cp += OTTERY_DIGEST_LEN;                    \
  } while (0)
#define ADD(object)                             \
  do {                                          \
    if (cp - buf + sizeof(object) > 4096) {     \
      ottery_digest(buf, buf, sizeof(buf));     \
      cp = buf + OTTERY_DIGEST_LEN;             \
    }                                           \
    bytes_added += sizeof(object);              \
    memcpy(buf, &object, sizeof(object));       \
    cp += sizeof(object);                       \
  } while (0)
#define ADD_ADDR(ptr)                           \
  do {                                          \
    void *p = (void*)ptr;                       \
    ADD(p);                                     \
  } while (0)
#define ADD_FN_ADDR(ptr)                        \
  do {                                          \
    uint64_t p = (uint64_t)&ptr;                \
    ADD(p);                                     \
  } while (0)
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

  cp = buf;

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

  ottery_digest(out, buf, sizeof(buf));

TRACE(("I looked at %llu bytes\n", (unsigned long long)bytes_added));

memwipe(buf, sizeof(buf));
return OTTERY_DIGEST_LEN;
}
#else
#define ottery_getentropy_fallback_kludge_OUTLEN 0
#define ottery_getentropy_fallback_kludge NULL
#endif

#define SOURCE_LEN(name) (ottery_getentropy_ ## name ## _OUTLEN)
#define OTTERY_ENTROPY_MAXLEN                   \
  (SOURCE_LEN(rdrand) +                         \
   SOURCE_LEN(getrandom) +                      \
   SOURCE_LEN(getentropy) +                     \
   SOURCE_LEN(cryptgenrandom) +                 \
   SOURCE_LEN(dev_urandom) +                    \
   SOURCE_LEN(dev_hwrandom) +                   \
   SOURCE_LEN(egd) +                            \
   SOURCE_LEN(proc_uuid) +                      \
   SOURCE_LEN(linux_sysctl) +                   \
   SOURCE_LEN(bsd_sysctl) +                     \
   SOURCE_LEN(fallback_kludge))

#define ID_RDRAND          (1u << 0)
#define ID_GETRANDOM       (1u << 1)
#define ID_GETENTROPY      (1u << 2)
#define ID_CRYPTGENRANDOM  (1u << 3)
#define ID_DEV_URANDOM     (1u << 4)
#define ID_DEV_HWRANDOM    (1u << 5)
#define ID_EGD             (1u << 6)
#define ID_PROC_UUID       (1u << 7)
#define ID_LINUX_SYSCTL    (1u << 8)
#define ID_BSD_SYSCTL      (1u << 9)
#define ID_FALLBACK_KLUDGE (1u << 10)

#define GROUP_HW      (1u << 0)
#define GROUP_SYSCALL (1u << 1)
#define GROUP_DEVICE  (1u << 2)
#define GROUP_EGD     (1u << 3)
#define GROUP_KLUDGE  (1u << 4)

#define FLAG_WEAK   (1u << 0)
#define FLAG_AVOID  (1u << 1)

#define SOURCE(name, id, group, flags)          \
  { #name, ottery_getentropy_ ## name,          \
      ottery_getentropy_ ## name ## _OUTLEN,    \
      (id), (group), (flags) }
static const struct {
  const char *name;
  int (*getentropy_fn)(unsigned char *out);
  int output_len;
  unsigned id;
  unsigned group;
  unsigned flags;
} entropy_sources[] = {
  SOURCE(rdrand, ID_RDRAND, GROUP_HW, FLAG_WEAK),
  SOURCE(getrandom, ID_GETRANDOM, GROUP_SYSCALL, 0),
  SOURCE(getentropy, ID_GETENTROPY, GROUP_SYSCALL, 0),
  SOURCE(cryptgenrandom, ID_CRYPTGENRANDOM, GROUP_SYSCALL, 0),
  SOURCE(dev_urandom, ID_DEV_URANDOM, GROUP_DEVICE, 0),
  SOURCE(dev_hwrandom, ID_DEV_HWRANDOM, GROUP_DEVICE, 0),
  SOURCE(egd, ID_EGD, GROUP_EGD, 0),
  SOURCE(proc_uuid, ID_PROC_UUID, GROUP_DEVICE, FLAG_AVOID),
  SOURCE(linux_sysctl, ID_LINUX_SYSCTL, GROUP_SYSCALL, FLAG_AVOID),
  SOURCE(bsd_sysctl, ID_BSD_SYSCTL, GROUP_SYSCALL, 0),
  SOURCE(fallback_kludge, ID_FALLBACK_KLUDGE, GROUP_KLUDGE, FLAG_AVOID)
};

#define N_ENTROPY_SOURCES (sizeof(entropy_sources) / sizeof(entropy_sources[0]))

static int
ottery_getentropy(unsigned char *out)
{
  int i, n;
  unsigned char *outp = out;
  int have_strong = 0;
  unsigned have_groups = 0, have_sources = 0;

  memset(out, 0, OTTERY_ENTROPY_MAXLEN);

  for (i = 0; i < (int)N_ENTROPY_SOURCES; ++i)
    {
      if (NULL == entropy_sources[i].getentropy_fn)
        continue;
      if (have_strong && (entropy_sources[i].flags & FLAG_AVOID))
        continue;
      n = entropy_sources[i].getentropy_fn(outp);
      if (n < 0)
        continue;
      outp += n;
      if (n >= ENTROPY_CHUNK && 0 == (entropy_sources[i].flags & FLAG_WEAK))
        have_strong = 1;
      have_groups |= entropy_sources[i].group;
      have_sources |= entropy_sources[i].id;
      TRACE(("source %s gave us %d\n", entropy_sources[i].name, n));
    }

  return outp - out;
}
