/* otterylite.h -- public APIs and declarations for libottery-lite */
/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/
#ifndef OTTERYLITE_H_INCLUDED_
#define OTTERYLITE_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

/*
  The following macros, if defined, override aspects of some of the
  definitions in this file, and corresponding definitions in Libottery-lite.
*/

/* Declare the APIs as taking a "struct ottery_state" structure, for
   state isolation.

   #define OTTERY_STRUCT
*/

/*
  Override the prefix we use for non-static functions.  By default, the prefix
  is "ottery_", or "ottery_st_" if OTTERY_STRUCT is defined.

  #define OTTERY_FUNC_PREFIX foo_
 */

/*
  Don't build with support for EGD (Entropy Gathering Daemon).  This
  option also disables the EGD API in this header.

  #define OTTERY_DISABLE_EGD
*/

/*
  Declare out interfaces to match arc4random_'s .  This is the same as
  setting OTTERY_FUNC_PREFIX to "arc4random_", except that it prevents
  APIs from having silly names like "arc4random_random()".

  #define OTTERY_BE_ARC4RANDOM
*/


/* Now we set OTTERY_FUNC_PREFIX if it's not already declared. */
#ifdef OTTERY_BE_ARC4RANDOM
#define OTTERY_FUNC_PREFIX arc4
#endif

#ifndef OTTERY_FUNC_PREFIX
#ifdef OTTERY_STRUCT
#define OTTERY_FUNC_PREFIX ottery_st_
#else
#define OTTERY_FUNC_PREFIX ottery_
#endif
#endif

/* These macros are used for declaring the first (or only) argument for our
   functions.  If we're using a static state, we don't need any extra arguments
   here.  But if we're putting our state in a structure, we need a separate
   argument for it.
 */
#ifdef OTTERY_STRUCT
#define OTTERY_STATE_ARG_ONLY struct ottery_state *state
#define OTTERY_STATE_ARG_FIRST OTTERY_STATE_ARG_ONLY,
struct ottery_state;
#else
#define OTTERY_STATE_ARG_ONLY void
#define OTTERY_STATE_ARG_FIRST
#endif

/* MSC is the only relevant compiler left without <stdint.h>.  We use uint64_t
 * internally, but here we define our own ottery_u64_t to avoid polluting the
 * namespace. */
#ifdef _MSC_VER
#define ottery_u64_t unsigned __int64
#else
#include <stdint.h>
#define ottery_u64_t uint64_t
#endif

#include <sys/types.h>

/* Standard identifier-pasting tricks to declare functions with different
 * names depending on how we're configured. */
#define OTTERY_PASTE__(a, b) a ## b
#define OTTERY_PASTE2__(a, b) OTTERY_PASTE__(a, b)
#define OTTERY_PUBLIC_FN(name) OTTERY_PASTE2__(OTTERY_FUNC_PREFIX, name)
#ifdef OTTERY_BE_ARC4RANDOM
#define OTTERY_PUBLIC_FN2(name) OTTERY_PUBLIC_FN(random_ ## name)
#else
#define OTTERY_PUBLIC_FN2(name) OTTERY_PUBLIC_FN(name)
#endif

/*
  And here's the API!  See the README for documentation on what all these
  functions do.

 */
#ifdef OTTERY_STRUCT
size_t OTTERY_PUBLIC_FN2 (state_size)(void);
void OTTERY_PUBLIC_FN2 (init)(OTTERY_STATE_ARG_ONLY);
#endif

void OTTERY_PUBLIC_FN2 (teardown)(OTTERY_STATE_ARG_ONLY);
void OTTERY_PUBLIC_FN2 (need_reseed)(OTTERY_STATE_ARG_ONLY);
void OTTERY_PUBLIC_FN2 (addrandom)(OTTERY_STATE_ARG_FIRST const unsigned char *inp, int n);
int OTTERY_PUBLIC_FN2 (status)(OTTERY_STATE_ARG_ONLY);
unsigned OTTERY_PUBLIC_FN (random)(OTTERY_STATE_ARG_ONLY);
ottery_u64_t OTTERY_PUBLIC_FN (random64)(OTTERY_STATE_ARG_ONLY);
unsigned OTTERY_PUBLIC_FN (random_uniform)(OTTERY_STATE_ARG_FIRST unsigned limit);
ottery_u64_t OTTERY_PUBLIC_FN (random_uniform64)(OTTERY_STATE_ARG_FIRST ottery_u64_t limit);
void OTTERY_PUBLIC_FN (random_buf)(OTTERY_STATE_ARG_FIRST void *out, size_t n);

#ifdef OTTERY_BE_ARC4RANDOM
#define arc4random_stir() ((void)0)
#endif

#ifndef OTTERY_DISABLE_EGD
struct sockaddr;
int OTTERY_PUBLIC_FN (set_egd_address)(const struct sockaddr *sa, int socklen);
#endif

#ifdef __cplusplus
}
#endif
#endif /* OTTERYLITE_H_INCLUDED_ */
