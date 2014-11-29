/* otterylite-impl.h -- preliminary includes and definitions for the
   libottery-lite PRNG */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#ifndef OTTERYLITE_IMPL_H_INCLUDED_
#define OTTERYLITE_IMPL_H_INCLUDED_

/*
  Here are more macros you can define to override Libottery-lite's default
  behavior.  Unlike the ones in otterylite.h, these ones don't change the
  declarations of the APIs.
 */

/*
  Don't lock the PRNG if this option is set.

  Only for use in embedded single-threaded applications.  And really, just
  don't.  It isn't worth it.  You'll regret it.

  #define OTTERY_DISABLE_LOCKING
*/

/*
  Don't try to mmap the ottery RNG state into its own separate page.

  By default, we use mlock and minherit tricks to keep things from going wrong
  with the private parts of the RNG state.  Setting this option disables that.

  #define OTTERY_RNG_NO_MMAP
*/

/*
  Don't allocate the RNG on the heap.

  If used together with NO_MMAP, this option makes the RNG get stored inside
  the regular ottery state.  Not recommended.

   #define OTTERY_RNG_NO_HEAP
*/

/*
  If we can't get any entropy from a real entropy source, don't try to gather
  it from kludging around the OS.

  #define OTTERY_DISABLE_FALLBACK_RNG
 */

/* Define this for a little debugging output.
   #define TRACE(x) printf x
*/
#define TRACE(x)

/*
  Here are the internal parts of the OTTERY_STRUCT implementation.  If we're
  putting our state in a structure, then our local pointer to it is in
  a variable called "state", and we need to insert a comma after that sometimes.
  We'll find members at state->membername.

  On the other hand, if we're storing things statically, we don't pass
 */
#ifdef OTTERY_STRUCT
#define OTTERY_STATE_ARG_OUT state
#define COMMA ,
#define STATE_FIELD(fld) (state->fld)
#else
#define OTTERY_STATE_ARG_OUT
#define COMMA
#define STATE_FIELD(fld) (ottery_ ## fld)
#endif

/* We need an extra "&" if we're not keeping the RNG separately. */
#if defined(OTTERY_RNG_NO_HEAP) && defined(OTTERY_RNG_NO_MMAP)
#define RNG_PTR (&(STATE_FIELD(rng)))
#else
#define RNG_PTR STATE_FIELD(rng)
#endif

/*
  Now find out a little bit about what kind of CPU we're building for.  Define
  OTTERY_X86 if it's an x86 or x86_64, and define OTTERY_X86_64 if it's an
  X86_64.
 */
#if defined(i386) ||                            \
  defined(__i386) ||                            \
  defined(__x86_64) ||                          \
  defined(__M_IX86) ||                          \
  defined(_M_IX86) ||                           \
  defined(_M_AMD64) ||                          \
  defined(__INTEL_COMPILER)

#define OTTERY_X86

#if defined(__x86_64) ||                        \
  defined(_M_AMD64)
#define OTTERY_X86_64
#endif
#endif

/* If we're pretending to be arc4random(), then suppress any declarations of
   arc4random_foo in the system headers. */
#ifdef OTTERY_BE_ARC4RANDOM
#undef arc4random_stir
#define arc4random x_system__arc4random
#define arc4random_uniform x_system__arc4random_uniform
#define arc4random_bytes x_system__arc4random_buf
#define arc4random_stir x_system__arc4random_stir
#define arc4random_addrandom x_system__arc4random_addrandom
#endif

/* And here we go with a big pile of includes. */

#include <sys/types.h>
#include <sys/stat.h>
#if defined(__OpenBSD__)
#include <param.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/random.h>
#endif

#ifndef OTTERY_DISABLE_EGD
#ifdef _WIN32
#include <winsock2.h>
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#endif

#if defined(_WIN32) && !defined(OTTERY_DISABLE_FALLBACK_RNG)
#include <tchar.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <lm.h>
#endif

#if !defined(_WIN32) && !defined(OTTERY_RNG_NO_MMAP)
#include <sys/mman.h>
#endif

#ifdef __APPLE__
#include <netinet/in.h>
#include <sys/socketvar.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <mach/mach_time.h>
#elif !defined(WIN32)
#include <ucontext.h>
#endif

#ifdef _MSC_VER
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
#define uint64_t unsigned __int64
#define inline __inline
#else
#include <stdint.h>
#endif

/* Branch prediction with GCC */
#ifdef __GNUC__
#define UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#define LIKELY(expr) (__builtin_expect(!!(expr), 1))
#else
#define UNLIKELY(expr) (expr)
#define LIKELY(expr) (expr)
#endif

#ifdef OTTERY_BE_ARC4RANDOM
/* We suppressed any declarations of arc4random_foo in the system headers,
 * but now we can undo that. */
#undef arc4random
#undef arc4random_uniform
#undef arc4random_bytes
#undef arc4random_stir
#undef arc4random_addrandom
#endif

/* On OSX, minherit wants these flags to have the VM_ prefix */
#if !defined(INHERIT_NONE) && defined(VM_INHERIT_NONE)
#define INHERIT_NONE VM_INHERIT_NONE
#endif
#if !defined(INHERIT_ZERO) && defined(VM_INHERIT_ZERO)
#define INHERIT_ZERO VM_INHERIT_ZERO
#endif

/* We're going to be saying "unsigned char" a _lot_ */
typedef unsigned char u8;

/* Our crypto uses these kinds of bit rotation.  Smart compilers can
 * optimize these to a single instruction. */
#define ROTL32(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR64(x, n)  (((x) >> (n)) | ((x) << (64 - (n))))
#define ROTR32(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))


#endif /* OTTERYLITE_H_IMPL_INCLUDED_ */
