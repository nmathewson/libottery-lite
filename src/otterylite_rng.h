/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#define CHACHA_BLOCKSIZE 64
#define CHACHA_ROUNDS 20
#define CHACHA_KEYLEN 32
#define CHACHA_IVLEN 8

#define OTTERY_KEYLEN (CHACHA_KEYLEN + CHACHA_IVLEN)
/* subtract CHACHA_BLOCKSIZE so we have some extra space to fit the whole
 * structure in a single mmaped page. */
#define OTTERY_BUFLEN (4096 - CHACHA_BLOCKSIZE)
#define OTTERY_N_BLOCKS (OTTERY_BUFLEN / CHACHA_BLOCKSIZE)

struct ottery_rng {
  unsigned magic;
  unsigned idx;
  unsigned count;
  unsigned char buf[OTTERY_BUFLEN];
};

#define ROTL32(x, n)                            \
  (((x) << (n)) | ((x) >> (32 - (n))))

#define CHACHA_QUARTER_ROUND(a, b, c, d)        \
  do {                                          \
    a += b;                                     \
    d = ROTL32(d ^ a, 16);                      \
    c += d;                                      \
    b = ROTL32(b ^ c, 12);                       \
    a += b;                                      \
    d = ROTL32(d ^ a, 8);                        \
    c += d;                                      \
    b = ROTL32(b ^ c, 7);                        \
  } while (0)


static void
chacha20_blocks(const unsigned char key[OTTERY_KEYLEN], int n_blocks,
                unsigned char *const output)
{
  uint32_t x[16];
  uint32_t y[16];
  int i, j;
  unsigned char *outp = output;

  x[0] = 1634760805u;
  x[1] = 857760878u;
  x[2] = 2036477234u;
  x[3] = 1797285236u;
  memcpy(&x[4], key, 8 * sizeof(uint32_t));
  x[12] = 0;
  x[13] = 0;
  memcpy(&x[14], key + 32, 2 * sizeof(uint32_t));

  for (i = 0; i < n_blocks; ++i)
    {
      x[12] = i;
      memcpy(y, x, sizeof(x));

      for (j = 0; j < (CHACHA_ROUNDS / 2); ++j)
        {
          CHACHA_QUARTER_ROUND(y[0], y[4], y[8],  y[12]);
          CHACHA_QUARTER_ROUND(y[1], y[5], y[9],  y[13]);
          CHACHA_QUARTER_ROUND(y[2], y[6], y[10], y[14]);
          CHACHA_QUARTER_ROUND(y[3], y[7], y[11], y[15]);
          CHACHA_QUARTER_ROUND(y[0], y[5], y[10], y[15]);
          CHACHA_QUARTER_ROUND(y[1], y[6], y[11], y[12]);
          CHACHA_QUARTER_ROUND(y[2], y[7], y[8],  y[13]);
          CHACHA_QUARTER_ROUND(y[3], y[4], y[9],  y[14]);
        }

      for (j = 0; j < 16; ++j)
        {
          y[j] += x[j];
        }

      memcpy(outp, y, sizeof(y));
      outp += sizeof(y);
    }

  memwipe(x, sizeof(x));
  memwipe(y, sizeof(y));
}

#undef ROTATE
#undef CHACHA_QUARTER_ROUND

static void
ottery_bytes_slow(struct ottery_rng *st, u8 *out, size_t n, size_t available_bytes)
{
  memcpy(out, st->buf + st->idx, available_bytes);
  out += available_bytes;
  n -= available_bytes;

  while (n > OTTERY_BUFLEN - OTTERY_KEYLEN)
    {
      ++st->count;
      chacha20_blocks(st->buf + OTTERY_BUFLEN - OTTERY_KEYLEN, OTTERY_N_BLOCKS, st->buf);
      memcpy(out, st->buf, OTTERY_BUFLEN - OTTERY_KEYLEN);
      out += (OTTERY_BUFLEN - OTTERY_KEYLEN);
      n -= (OTTERY_BUFLEN - OTTERY_KEYLEN);
    }

  ++st->count;
  chacha20_blocks(st->buf + OTTERY_BUFLEN - OTTERY_KEYLEN, OTTERY_N_BLOCKS, st->buf);
  memcpy(out, st->buf, n);
  memset(st->buf, 0, n);
  st->idx = n;
}

static inline void
ottery_bytes(struct ottery_rng *st, void * const output, size_t n)
{
  size_t available_bytes = OTTERY_BUFLEN - OTTERY_KEYLEN - st->idx;
  u8 *out = output;

  if (LIKELY(n <= available_bytes))
    {
      /* Can do in one go */
      memcpy(out, st->buf + st->idx, n);
      memset(st->buf + st->idx, 0, n);
      st->idx += n;
    }
  else
    {
      ottery_bytes_slow(st, output, n, available_bytes);
    }
}


static void
ottery_setkey(struct ottery_rng *st, const unsigned char key[OTTERY_KEYLEN])
{
  chacha20_blocks(key, OTTERY_N_BLOCKS, st->buf);
  st->idx = 0;
  st->count = 0;
}

