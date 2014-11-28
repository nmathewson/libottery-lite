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
    return -2; /* RDRAND not supported. */
  for (i = 0; i < ENTROPY_CHUNK / 4; ++i, output += 4)
    {
      if (rdrand_((uint32_t*)output) < 0)
        return -1;
    }
  return ENTROPY_CHUNK;
}

#else
#define ottery_getentropy_rdrand NULL
#endif

#if ((defined(__OpenBSD__) && OpenBSD >= 201411 /* 5.6 */))
static int
ottery_getentropy_getentropy(unsigned char *out)
{
  return getentropy(out, ENTROPY_CHUNK) == 0 ? ENTROPY_CHUNK : -1;
}
#else
#define ottery_getentropy_getentropy NULL
#endif

#if defined(__linux__) && defined(__NR_getrandom)
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
  if (r == -ENOSYS)
    return -2;
  return -1;
}
static int
ottery_getentropy_getrandom(unsigned char *out)
{
  return ottery_getrandom_(out, ENTROPY_CHUNK, 0);
}
#else
#define ottery_getentropy_getrandom NULL
#endif

#if defined(_WIN32)
static int
ottery_getentropy_cryptgenrandom(unsigned char *out)
{
  int n = -1;
  HCRYPTPROV h = 0;

  if (!CryptAcquireContext(&h, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    return -1;

  if (!CryptGenRandom(h, ENTROPY_CHUNK, out))
    goto out;

  n = ENTROPY_CHUNK;

 out:
  CryptReleaseContext(h, 0);
  return n;
}
#else
#define ottery_getentropy_cryptgenrandom NULL
#endif

#ifndef _WIN32
static int
ottery_getentropy_device_(unsigned char *out, int len,
                          const char *fname,
                          unsigned need_mode_flags)
{
  int fd;
  int r, output = -1, remain = len;
  struct stat st;

  fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return -1;
  if (fstat(fd, &st))
    goto out;
  if ((st.st_mode & need_mode_flags) != need_mode_flags)
    goto out;
  output = 0;
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
  TRY("/dev/hw_random");
  return -1;
}
#undef TRY
#else
#define ottery_getentropy_dev_urandom NULL
#define ottery_getentropy_dev_hwrandom NULL
#endif

#ifdef __linux__
static int
ottery_getentropy_proc_uuid(unsigned char *out)
{
  int n = 0, r, i;
  u8 buf[37 * 2], *cp = buf;

  memset(buf, 0, sizeof(buf));
  /* Each call yields 37 bytes, containing 16 actual bytes of entropy. Make
   * two calls to get 32 bytes of entropy. */
  for (i = 0; i < 2; ++i)
    {
      r = ottery_getentropy_device_(cp, 37, "/proc/sys/kernel/random/uuid", 0);
      if (r < 0)
        return -1;
      n += r;
      cp += r;
    }
  blake2_noendian(out, ENTROPY_CHUNK, buf, n, 909090, 1010101);
  memwipe(buf, sizeof(buf));
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_proc_uuid NULL
#endif

#ifndef OTTERY_DISABLE_EGD
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
    return -2;

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
#endif

#if defined(__linux__)
static int
ottery_getentropy_linux_sysctl(unsigned char *out)
{
  int mib[] = { CTL_KERN, KERN_RANDOM, RANDOM_UUID };
  int n_read = 0, i;
  char buf[74];

  memset(buf, 0, 74);
  for (i = 0; i < 2; ++i)
    {
      size_t n = 37;
      int r = sysctl(mib, 3, buf, &n, NULL, 0);
      if (r < 0 && errno == ENOSYS)
        return -2;
      else if (r < 0 || n > 37)
        return -1;
      n_read += n;
    }
  blake2_noendian(out, ENTROPY_CHUNK, (u8*)buf, n_read, 444, 1234567);
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_linux_sysctl NULL
#endif

#if defined(CTL_KERN) && defined(KERN_ARND)
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
#endif

#if !defined(OTTERY_DISABLE_FALLBACK_RNG)
#include "otterylite_fallback.h"
#else
#define ottery_getentropy_fallback_kludge NULL
#endif /* OTTERY_DISABLE_FALLBACK_RNG */

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
      (id), (group), (flags) }
static const struct entropy_source {
  const char *name;
  int (*getentropy_fn)(unsigned char *out);
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
  SOURCE(fallback_kludge, ID_FALLBACK_KLUDGE, GROUP_KLUDGE, FLAG_AVOID|FLAG_WEAK)
};

#define N_ENTROPY_SOURCES (sizeof(entropy_sources) / sizeof(entropy_sources[0]))

#define OTTERY_ENTROPY_MAXLEN (ENTROPY_CHUNK * N_ENTROPY_SOURCES)

static int
ottery_getentropy_impl(unsigned char *out, int *status_out,
                       const struct entropy_source * const sources,
                       int n_sources)
{
  int i, n;
  unsigned char *outp = out;
  int have_strong = 0;
  unsigned have_groups = 0, have_sources = 0, have_a_full_output = 0;

  memset(out, 0, ENTROPY_CHUNK * n_sources);

  for (i = 0; i < n_sources; ++i)
    {
      if (NULL == sources[i].getentropy_fn)
        continue;
      /* assert(outp - out < OTTERY_ENTROPY_MAXLEN - ENTROPY_CHUNK); */
      if (have_strong && (sources[i].flags & FLAG_AVOID))
        continue;
      if ((have_groups & sources[i].group) == sources[i].group)
        continue;
      n = sources[i].getentropy_fn(outp);
      if (n < 0)
        continue;
      outp += n;
      if (n != ENTROPY_CHUNK)
        continue;
      have_a_full_output = 1;
      if (0 == (sources[i].flags & FLAG_WEAK))
        have_strong = 1;
      have_groups |= sources[i].group;
      have_sources |= sources[i].id;
      TRACE(("source %s gave us %d\n", sources[i].name, n));
    }

  if (! have_a_full_output)
    *status_out = 0;
  else if (!have_strong)
    *status_out = 1;
  else
    *status_out = 2;

  return outp - out;
}

static int
ottery_getentropy(unsigned char *out, int *status_out)
{
  return ottery_getentropy_impl(out, status_out,
                                entropy_sources, (int)N_ENTROPY_SOURCES);
}
