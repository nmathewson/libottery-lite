/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#define WORDBITS 32

typedef unsigned char u8;

#if WORDBITS == 64
typedef uint64_t word_t;
#define U64(n) n ## ull
#define BLAKE2_IV0 U64(0x6a09e667f3bcc908)
#define BLAKE2_IV1 U64(0xbb67ae8584caa73b)
#define BLAKE2_IV2 U64(0x3c6ef372fe94f82b)
#define BLAKE2_IV3 U64(0xa54ff53a5f1d36f1)
#define BLAKE2_IV4 U64(0x510e527fade682d1)
#define BLAKE2_IV5 U64(0x9b05688c2b3e6c1f)
#define BLAKE2_IV6 U64(0x1f83d9abfb41bd6b)
#define BLAKE2_IV7 U64(0x5be0cd19137e2179)
#define ROT(x, n)  (((x) >> (n)) | ((x) << (64 - n)))
#define OTTERY_PERSONALIZATION_1 U64(0x4f74746572792042)
#define OTTERY_PERSONALIZATION_2 U64(0x6c616b6532622121)
#define BLAKE2_BLOCKSIZE 128
#define BLAKE2_ROUNDS 12
#define ROT1 32
#define ROT2 24
#define ROT3 16
#define ROT4 63
#else
typedef uint32_t word_t;
#define BLAKE2_IV0 0x6a09e667
#define BLAKE2_IV1 0xbb67ae85
#define BLAKE2_IV2 0x3c6ef372
#define BLAKE2_IV3 0xa54ff53a
#define BLAKE2_IV4 0x510e527f
#define BLAKE2_IV5 0x9b05688c
#define BLAKE2_IV6 0x1f83d9ab
#define BLAKE2_IV7 0x5be0cd19
#define ROT(x, n)  (((x) >> (n)) | ((x) << (32 - n)))
#define OTTERY_PERSONALIZATION_1 0x6c6f6c62
#define OTTERY_PERSONALIZATION_2 0x6c6b3273
#define BLAKE2_BLOCKSIZE 64
#define BLAKE2_ROUNDS 10
#define ROT1 16
#define ROT2 12
#define ROT3 8
#define ROT4 7
#endif

#define BLAKE2_MAX_OUTPUT (BLAKE2_BLOCKSIZE / 2)

/* This is not quite Blake2. We assume we're on a little endian platform. */

#define BLAKE2_G(a, b, c, d, round, idx)               \
  do {                                          \
    a += b + m[blake2_sigma[round][2*idx]];     \
    d = ROT(d ^ a, ROT1);                       \
    c += d;                                     \
    b = ROT(b ^ c, ROT2);                        \
    a += b + m[blake2_sigma[round][2*idx+1]];    \
    d = ROT(d ^ a, ROT3);                        \
    c += d;                                      \
    b = ROT(b ^ c, ROT4);                        \
  } while(0)

#define BLAKE2_ROUND(round)                               \
  do {                                                    \
    BLAKE2_G(v[0] , v[4] , v[8] , v[12], round, 0);       \
    BLAKE2_G(v[1] , v[5] , v[9] , v[13], round, 1);       \
    BLAKE2_G(v[2] , v[6] , v[10], v[14], round, 2);       \
    BLAKE2_G(v[3] , v[7] , v[11], v[15], round, 3);       \
    BLAKE2_G(v[0] , v[5] , v[10], v[15], round, 4);       \
    BLAKE2_G(v[1] , v[6] , v[11], v[12], round, 5);       \
    BLAKE2_G(v[2] , v[7] , v[8] , v[13], round, 6);       \
    BLAKE2_G(v[3] , v[4] , v[9] , v[14], round, 7);       \
  } while(0)

static const u8 blake2_sigma[12][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
  { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
  { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
  { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
  { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
  { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
  { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
  { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
  { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
};

/* This is not quite blake2, since it doesn't do endianness conversion. */
static int
blake2_noendian(u8 *output, int output_len, const u8 *input, word_t input_len,
                word_t personalization_0, word_t personalization_1)
{
  word_t h[8];
  word_t counter; /* Too short for standard usage; fine for us. */
  word_t m[16], v[16];

  if (output_len > BLAKE2_MAX_OUTPUT || output_len <= 0)
    return -1;

  counter = 0;
  h[0] = BLAKE2_IV0;
  /* these parameters include: the digest length, the key length (0),
   * the fanout (1), and the depth (1). */
  h[0] ^= ( (word_t) 0x00000101 | (output_len << 24) ) << (WORDBITS - 32);
  h[1] = BLAKE2_IV1; /* only used by blake2p */
  h[2] = BLAKE2_IV2; /* only used by blake2p */
  h[3] = BLAKE2_IV3; /* only used by blake2p  */
  h[4] = BLAKE2_IV4; /* salt would go here*/
  h[5] = BLAKE2_IV5; /* salt would go here */
  h[6] = BLAKE2_IV6 ^ personalization_0;
  h[7] = BLAKE2_IV6 ^ personalization_1;

  /* We would add the key as the first block, if we supported keys. */

  while (input_len) {
    word_t f0;
    word_t inc;
    int i;

    if (input_len > sizeof(m)) {
      memcpy(m, input, sizeof(m));
      f0 = 0;
      inc = sizeof(m);
    } else {
      memset(m, 0, sizeof(m));
      memcpy(m, input, input_len);
      f0 = ~(word_t)0;
      inc = input_len;
    }

    memcpy(v, h, sizeof(h));
    v[8] = BLAKE2_IV0;
    v[9] = BLAKE2_IV1;
    v[10] = BLAKE2_IV2;
    v[11] = BLAKE2_IV3;
    v[12] = BLAKE2_IV4; /* naughty; should be the high word of the counter */
    v[13] = BLAKE2_IV5 ^ counter;
    v[14] = BLAKE2_IV6 ^ f0;
    v[15] = BLAKE2_IV7;

    input += inc;
    counter += inc;
    input_len -= inc;

    for (i = 0; i < BLAKE2_ROUNDS; ++i) {
      BLAKE2_ROUND(i);
    }

    for (i = 0; i < 8; ++i) {
      h[i] ^= v[i] ^ v[i+8];
    }
  }

  memcpy(output, h, output_len);

  memwipe(h, sizeof(h));
  memwipe(v, sizeof(v));

  return (int) output_len;
}

#define OTTERY_DIGEST_LEN BLAKE2_MAX_OUTPUT
#define OTTERY_DIGEST(out, inp, inplen)                                 \
  do {                                                                  \
    int blake_output;                                                   \
    blake_output = blake2_noendian((out), BLAKE2_MAX_OUTPUT,            \
                                   (inp), (inplen),                     \
                                   OTTERY_PERSONALIZATION_1,            \
                                   OTTERY_PERSONALIZATION_2);           \
    if (blake_output != BLAKE2_MAX_OUTPUT)                              \
      abort();                                                          \
  } while(0)

