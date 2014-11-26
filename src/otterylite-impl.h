/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#ifndef OTTERYLITE_IMPL_H_INCLUDED
#define OTTERYLITE_IMPL_H_INCLUDED

#define TRACE(x) printf x
/* #define TRACE(x) */

#ifdef OTTERY_STRUCT
#define OTTERY_STATE_ARG_OUT state
#define COMMA ,
#define RNG (&(state)->rng)
#define STATE_FIELD(fld) (state->fld)
#define FUNC_PREFIX ottery_st_
#else
#define OTTERY_STATE_ARG_OUT
#define COMMA
#define RNG (&ottery_rng)
#define STATE_FIELD(fld) (ottery_ ## fld)
#endif

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

#ifdef OTTERY_ENABLE_EGD
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

#ifdef __APPLE__
#include <netinet/in.h>
#include <sys/socketvar.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <mach/mach_time.h>
#else
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

#ifdef __GNUC__
#define UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#define LIKELY(expr) (__builtin_expect(!!(expr), 1))
#else
#define UNLIKELY(expr) (expr)
#define LIKELY(expr) (expr)
#endif

typedef unsigned char u8;
#endif
