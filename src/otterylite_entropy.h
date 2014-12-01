/* otterylite_entropy.h -- entropy extraction for lottery-lite */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

/*
  See the README for a discussion of approaches and ideas here.

  The first part of this file provides a wide variety of methods for getting
  entropy from the OS and hardware.  Fallback methods are implemented in other
  files.

  The second part of this file chooses and combines entropy methods.
 */

#ifndef OTTERYLITE_ENTROPY_H_INCLUDED
#define OTTERYLITE_ENTROPY_H_INCLUDED

/* Every method is supposed to produce this much data.  If you need to produce
   more, use blake2b to compact it. */
#define ENTROPY_CHUNK 32

/*
  We consider ourselves unseeded if we have less than this much output from
  all our entropy producers.
 */
#define OTTERY_ENTROPY_MINLEN 32

/*
  Define some flags to 0 when they don't exist, so we can write the code
  as if they did.
 */
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
/*
  RDRAND -- get entropy from recent x86 chips.

  Have sinister forces backdoored this friendly instruction?  Let's hope not.
  But also, let's not use it last, or use it alone.
 */

/* Giving RDRAND too many chances to fail seems risky to me. */
#define RDRAND_MAXATTEMPTS 16

/* Low-level rdrand implementation */
#ifdef _MSC_VER
#define rdrand_ll_ _rdrand32_step
#else
static int
rdrand_ll_(uint32_t *therand)
{
  unsigned char status;
  __asm volatile (".byte 0x0F, 0xC7, 0xF0 ; setc %1"
                  : "=a" (*therand), "=qm" (status));

  return (status) == 1 ? 0 : -1;
}
#endif

/* Call rdrand until it succeeds or we give up. */
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

/* CPUID implementation to see whether we even have RDRAND */
#ifdef _MSC_VER
#define cpuid_(i,result) __cpuid((result), (i))
#else
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
#endif

static int
cpuid_says_rdrand_supported_(void)
{
  unsigned result[4];

  cpuid_(1, result);
  return 0 != (result[2] & (1u << 30));
}

/* Entropy source using rdrand. */
static int
ottery_getentropy_rdrand(unsigned char *output)
{
  int i;

  if (!cpuid_says_rdrand_supported_())
    return -2; /* RDRAND not supported. */
  for (i = 0; i < ENTROPY_CHUNK / 4; ++i, output += 4)
    {
      if (rdrand_((uint32_t*)output) < 0) {
        /*
          A tricky point -- if rdrand stops working partway through, do
          we use what it gave us before?  I don't think so; let's allow
          minimum space for shenanigans.
        */
        return -1;
      }
    }
  return ENTROPY_CHUNK;
}

#else
#define ottery_getentropy_rdrand NULL
#endif



#if ((defined(__OpenBSD__) && OpenBSD >= 201411 /* 5.6 */))
/*
  Ah, a good entropy source!  getentropy is about as simple as you
  could ask for.
 */
static int
ottery_getentropy_getentropy(unsigned char *out)
{
  return getentropy(out, ENTROPY_CHUNK) == 0 ? ENTROPY_CHUNK : -1;
}
#else
#define ottery_getentropy_getentropy NULL
#endif


#if defined(__linux__) && defined(__NR_getrandom)
/*
  getrandom tries its best, but supplies too many options, and doesn't
  make it too easy to just do the right thing.  Still, it's so much better
  than the situation before that I really don't want to complain.
 */

/* Define a getrandom, since glibc doesn't wrap it as of this writing. */
static int
ottery_getrandom_ll_(void *out, size_t n, unsigned flags)
{
  return syscall(__NR_getrandom, out, n, flags);
}
/*
  Wrap getrandom to make it try harder.
 */
static int
ottery_getrandom_(void *out, size_t n, unsigned flags)
{
  int r;

  do
    {
      r = ottery_getrandom_ll_(out, n, flags);
      if (r == (int)n)
        return n;
    } while (r == -EINTR); /* We have to try again on EINTR. Ouch! */
  if (r == -ENOSYS)
    return -2; /* Not supported */
  return -1;
}
/*
  Entropy source using getrandom.
 */
static int
ottery_getentropy_getrandom(unsigned char *out)
{
  return ottery_getrandom_(out, ENTROPY_CHUNK, 0);
}
#else
#define ottery_getentropy_getrandom NULL
#endif

#if defined(_WIN32)
/*
  On Windows, everybody uses CryptGenRandom.  The wikipedia page at
      https://en.wikipedia.org/wiki/CryptGenRandom
  has a nice collection of references.  Also see MSDN.
*/
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
/* Everything besides windows has device files */

/*
  Try to read 'len' bytes from the device file named 'fname', storing them
  into 'out'.  Return the actual number of bytes read, or -1 on error.  If
  need_mode_flags, fail if the st_mode field of the file does not have all the
  bits in need_mode_flags set.
 */
static int
ottery_getentropy_device_(unsigned char *out, int len,
                          const char *fname,
                          unsigned need_mode_flags)
{
  int fd;
  int r, output = -1, remain = len;
  struct stat st;

  /*
    Open with O_NOFOLLOW -- this stuff should not be a symlink.
   */
  fd = open(fname, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return -1;
  if (fstat(fd, &st))
    goto out; /* Can't fstat?  That's an error */
  if ((st.st_mode & need_mode_flags) != need_mode_flags)
    goto out; /* If it's not a device, and we asked for one, that's a bad
               * sign. */
  /* FFFF Check the actual device numbers */

  /* Read until we hit EOF, an error, or the number of bytes we wanted */
  output = 0;
  while (remain)
    {
      r = (int) read(fd, out, remain);
      if (r == -1)
        {
          if (errno == EINTR || errno == EAGAIN)
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

/*
  Try to read from the most urandom-like file available.
 */
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
  /*
    According to the libressl-portable people, this is where you have to look
    if you're doing O_NOFOLLOW and trying to find a urandom device on sunos.
   */
  TRY("/devices/pseudo/random@0:urandom");
#endif
#ifdef __OpenBSD__
  /*
    OpenBSD puts its RNG in srandom.  ????? Is this so?
   */
  TRY("/dev/srandom");
#endif
  TRY("/dev/urandom");
  TRY("/dev/random");
  return -1;
}

/* Try a /dev/hw{_,}random, if it exists */
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
#define LINUX_UUID_LEN 37
/*
  So, if you don't have getrandom(), and you don't have a working /dev, but
  you still have /proc, you can still get the kernel to give you entropy.
  Just hash a couple of UUIDs together!
 */
static int
ottery_getentropy_proc_uuid(unsigned char *out)
{
  /* ???? Verify that this actually uses urandom. */
  int n = 0, r, i;
  u8 buf[LINUX_UUID_LEN * 3], *cp = buf;

  memset(buf, 0, sizeof(buf));
  /* Each call yields LINUX_UUID_LEN bytes, containing 16 actual bytes of
   * entropy. Make an extra call just in case */
  for (i = 0; i < 3; ++i)
    {
      r = ottery_getentropy_device_(cp, LINUX_UUID_LEN, "/proc/sys/kernel/random/uuid", 0);
      if (r < 0)
        return -1;
      n += r;
      cp += r;
    }
  blake2(out, ENTROPY_CHUNK, buf, n, 909090, 1010101);
  memwipe(buf, sizeof(buf));
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_proc_uuid NULL
#endif

#ifndef OTTERY_DISABLE_EGD
/*
  EGD is a venerable replacement for having a kernel that actually knows how
  to treat entropy.

  The protocol is documented in the EGD distribution
 */
static struct sockaddr_storage ottery_egd_sockaddr;
static int ottery_egd_socklen = -1;

#ifndef _WIN32
/* Windows messed up its Berkeley sockets API; this is what you need to
 * call things */
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
    return -2; /* socket not configured */

  sock = socket(ottery_egd_sockaddr.ss_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sock == INVALID_SOCKET)
    return -1;

  if (connect(sock,
              (struct sockaddr*)&ottery_egd_sockaddr, ottery_egd_socklen) < 0)
    goto out;

  msg[0] = 1;             /* "read, nonblocking" */
  msg[1] = ENTROPY_CHUNK; /* request size */
  if (send(sock, msg, 2, 0) != 2)
    goto out;

  while (n_read < ENTROPY_CHUNK)
    {
      int r = (int) recv(sock, (void*)out, ENTROPY_CHUNK - n_read, 0);
      if (r < 0) {
        if (errno == EAGAIN || errno == EINTR)
          continue;
        goto out;
      } else if (r == 0) {
        break;
      }

      n_read += r;
      out += r;
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
/*
  Let's say that you're on a horrible Linux with no getrandom(), no /proc, and
  no /dev.  Well, maybe it's a horrible _old_ Linux!  If it is, it might have
  the sysctl() syscall, and you might be able to get random UUIDs this way.

  ("Come back here and take what's coming to you! I'll randomize your legs
   off!")

  This won't work on more modern Linuxes, since they don't have sysctl any
  more.
 */
static int
ottery_getentropy_linux_sysctl(unsigned char *out)
{
  int mib[] = { CTL_KERN, KERN_RANDOM, RANDOM_UUID };
  int n_read = 0, i;
  char buf[LINUX_UUID_LEN * 3];

  memset(buf, 0, 74);
  for (i = 0; i < 3; ++i)
    {
      size_t n = LINUX_UUID_LEN;
      int r = sysctl(mib, 3, buf, &n, NULL, 0);
      if (r < 0 && errno == ENOSYS)
        return -2;
      else if (r < 0 || n > LINUX_UUID_LEN)
        return -1;
      n_read += n;
    }
  blake2(out, ENTROPY_CHUNK, (u8*)buf, n_read, 444, 1234567);
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_linux_sysctl NULL
#endif

#if defined(CTL_KERN) && defined(KERN_ARND)
/*
  Some of the BSDs provide a different sysctl().  That's worth trying too.
 */
static int
ottery_getentropy_bsd_sysctl(unsigned char *out)
{
  int i;
  int mib[] = { CTL_KERN, KERN_ARND };

  /* I hear that some BSDs don't like returning anything but sizeof(unsigned)
   * bytes at a time from this one. */
  for (i = 0; i < ENTROPY_CHUNK; i += sizeof(unsigned))
    {
      size_t n = sizeof(unsigned);
      if (sysctl(mib, 2, out, &n, NULL, 0) == -1 || n <= 0)
        return -1;
      out += n;
    }
  return ENTROPY_CHUNK;
}
#else
#define ottery_getentropy_bsd_sysctl NULL
#endif

#if !defined(OTTERY_DISABLE_FALLBACK_RNG)
/*
  And last of all, we can define a hairy mess of junk.  Let's stick that in
  another file so it doesn't fighten the livestock.
 */
#include "otterylite_fallback.h"
#else
#define ottery_getentropy_fallback_kludge NULL
#endif /* OTTERY_DISABLE_FALLBACK_RNG */

/*
  Now turn we to selecting where to get our entropy.  Every function for
  getting entropy comes with a unique ID, and belongs to a Group.

  A function can be Weak, or Avoided.  Any non-weak function is considered
  Strong.

  Here are the rules:

     * If you've already gotten a complete ENTROPY_CHUNK from some
       function in a Group, don't consider any other function from that
       Group.

     * If you've already gotten a complete ENTROPY_CHUNK from a Strong
       function, don't consider any Avoided function.
 */


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
#define GROUP_CPU     (1u << 1)
#define GROUP_SYSCALL (1u << 2)
#define GROUP_DEVICE  (1u << 3)
#define GROUP_EGD     (1u << 4)
#define GROUP_KLUDGE  (1u << 5)

#define FLAG_WEAK   (1u << 0)
#define FLAG_AVOID  (1u << 1)

#define SOURCE(name, id, group, flags)          \
  { #name, ottery_getentropy_ ## name, (id), (group), (flags) }
static const struct entropy_source {
  const char *name;
  int (*getentropy_fn)(unsigned char *out);
  unsigned id;
  unsigned group;
  unsigned flags;
} entropy_sources[] = {
  SOURCE(rdrand, ID_RDRAND, GROUP_CPU, FLAG_WEAK),
  SOURCE(getrandom, ID_GETRANDOM, GROUP_SYSCALL, 0),
  SOURCE(getentropy, ID_GETENTROPY, GROUP_SYSCALL, 0),
  SOURCE(cryptgenrandom, ID_CRYPTGENRANDOM, GROUP_SYSCALL, 0),
  SOURCE(dev_urandom, ID_DEV_URANDOM, GROUP_DEVICE, 0),
  SOURCE(dev_hwrandom, ID_DEV_HWRANDOM, GROUP_HW, 0),
  SOURCE(egd, ID_EGD, GROUP_EGD, 0),
  SOURCE(proc_uuid, ID_PROC_UUID, GROUP_DEVICE, FLAG_AVOID),
  SOURCE(linux_sysctl, ID_LINUX_SYSCTL, GROUP_SYSCALL, FLAG_AVOID),
  SOURCE(bsd_sysctl, ID_BSD_SYSCTL, GROUP_SYSCALL, 0),
  SOURCE(fallback_kludge, ID_FALLBACK_KLUDGE, GROUP_KLUDGE, FLAG_AVOID|FLAG_WEAK)
};

#define N_ENTROPY_SOURCES (sizeof(entropy_sources) / sizeof(entropy_sources[0]))

/*
  The largest possible output from ottery_getentropy().
 */
#define OTTERY_ENTROPY_MAXLEN (ENTROPY_CHUNK * N_ENTROPY_SOURCES)

/*
  Helper: As ottery_getentropy, but consider the 'n_sources' entropy
  sources in 'sources'.

  This is done as a different function so that we can test it.
 */
static int
ottery_getentropy_impl(unsigned char *out, int *status_out,
                       const struct entropy_source * const sources,
                       int n_sources)
{
  int i, n;
  /* Pointer to the next place in 'out' where we should write. */
  unsigned char *outp = out;
  /* boolean: set to true if we have gotten an ENTROPY_CHUNK from any
     non-weak source. */
  int have_strong = 0;
  /* boolean: set to true if we have gotten an ENTROPY_CHUNK from any
     single source. */
  int have_a_full_output = 0;
  /* bitmasks: which groups and which IDs have given us full CHUNKs? */
  unsigned have_groups = 0, have_sources = 0;

  /*
    Start by filling the output with 0s, so that we can just hash
    the whole thing later on.
  */
  memset(out, 0, ENTROPY_CHUNK * n_sources);

  for (i = 0; i < n_sources; ++i)
    {
      if (NULL == sources[i].getentropy_fn)
        continue; /* Not implemented; skip */
      /* assert(outp - out < OTTERY_ENTROPY_MAXLEN - ENTROPY_CHUNK); */
      if (have_strong && (sources[i].flags & FLAG_AVOID))
        continue; /* We already have strong entropy; avoid this one */
      if ((have_groups & sources[i].group) == sources[i].group)
        continue; /* We already got entropy from this group */

      /* Try calling the function that implements this source. */
      n = sources[i].getentropy_fn(outp);

      if (n < 0)
        continue; /* Failed or not implemented */

      outp += n; /* Remember any data it gave us. */

      if (n < ENTROPY_CHUNK)
        continue; /* Too little output, so don't record this as having
                     succeeded */

      have_a_full_output = 1;
      if (0 == (sources[i].flags & FLAG_WEAK))
        have_strong = 1;
      have_groups |= sources[i].group;
      have_sources |= sources[i].id;
      TRACE(("source %s gave us %d\n", sources[i].name, n));
    }
  (void) have_sources; /* Eventually, expose this. FFFFF */

  if (outp - out < ENTROPY_CHUNK)
    *status_out = -1; /* Not enough output altogether */
  else if (! have_a_full_output)
    *status_out = 0;  /* Nobody gave us complete output */
  else if (!have_strong)
    *status_out = 1;  /* No source that I really trust much */
  else
    *status_out = 2;  /* We have at least one good source. */

  return (int) ( outp - out );
}

/*
  Fill 'out' with up to OTTERY_ENTROPY_MAXLEN bytes of entropy.  Return
  the number of bytes we added.

  Set *'status_out' to -1 or 0 if we're doing quite badly, 1 if we have
  entropy but not from good sources, and 2 if we're doing as well
  as we're likely to do.
 */
static int
ottery_getentropy(unsigned char *out, int *status_out)
{
  return ottery_getentropy_impl(out, status_out,
                                entropy_sources, (int)N_ENTROPY_SOURCES);
}

#endif /* OTTERYLITE_ENTROPY_H_INCLUDED */
