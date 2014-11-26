/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

#define u32 uint32_t
#define KEYLEN 40
#define BLOCKSIZE 64
#define ROUNDS 20
#define BUFLEN 4096
#define N_BLOCKS (BUFLEN / BLOCKSIZE)

struct ottery_rng {
  unsigned idx;
  unsigned count;
  unsigned char buf[BUFLEN];
};

#define ROTATE(x, n)                            \
  (((x)<<(n)) | ((x)>>(32-(n))))

#define QUARTER_ROUND(a,b,c,d) \
  do {                         \
    a += b;                    \
    d = ROTATE(d ^ a, 16);     \
    c += d;                    \
    b = ROTATE(b ^ c, 12);     \
    a += b;                    \
    d = ROTATE(d ^ a, 8);      \
    c += d;                    \
    b = ROTATE(b ^ c, 7);      \
  } while (0)


static void
chacha20_blocks(const unsigned char key[KEYLEN], int n_blocks,
                unsigned char *const output)
{
  u32 x[16];
  u32 y[16];
  int i, j;
  unsigned char *outp = output;

  x[0] = 1634760805u;
  x[1] = 857760878u;
  x[2] = 2036477234u;
  x[3] = 1797285236u;
  memcpy(&x[4], key, 8*sizeof(u32));
  x[12] = 0;
  x[13] = 0;
  memcpy(&x[14], key+32, 2*sizeof(u32));

  for (i = 0; i < n_blocks; ++i) {
    x[12] = i;
    memcpy(y, x, sizeof(x));

    for (j = 0; j < (ROUNDS / 2); ++j) {
      QUARTER_ROUND( y[0], y[4], y[8],y[12]);
      QUARTER_ROUND( y[1], y[5], y[9],y[13]);
      QUARTER_ROUND( y[2], y[6],y[10],y[14]);
      QUARTER_ROUND( y[3], y[7],y[11],y[15]);
      QUARTER_ROUND( y[0], y[5],y[10],y[15]);
      QUARTER_ROUND( y[1], y[6],y[11],y[12]);
      QUARTER_ROUND( y[2], y[7], y[8],y[13]);
      QUARTER_ROUND( y[3], y[4], y[9],y[14]);
    }

    for (j = 0; j < 16; ++j) {
      y[j] += x[j];
    }

    memcpy(outp, y, sizeof(y));
    outp += sizeof(y);
  }

  memwipe(x, sizeof(x));
  memwipe(y, sizeof(y));
}

#undef ROTATE
#undef QUARTER_ROUND

static void
ottery_bytes(struct ottery_rng *st, void * const output, size_t n)
{
  size_t available_bytes = BUFLEN - KEYLEN - st->idx;
  char *out = output;

  if (n <= available_bytes) {
    /* Can do in one go */
    memcpy(out, st->buf + st->idx, n);
    memset(st->buf + st->idx, 0, n);
    st->idx += n;
    return;
  }

  memcpy(out, st->buf + st->idx, available_bytes);
  out += available_bytes;
  n -= available_bytes;

  while (n > BUFLEN - KEYLEN) {
    ++st->count;
    chacha20_blocks(st->buf+BUFLEN-KEYLEN, N_BLOCKS, st->buf);
    memcpy(out, st->buf, BUFLEN - KEYLEN);
    out += (BUFLEN - KEYLEN);
    n -= (BUFLEN - KEYLEN);
  }

  ++st->count;
  chacha20_blocks(st->buf+BUFLEN-KEYLEN, N_BLOCKS, st->buf);
  memcpy(out, st->buf, n);
  memset(st->buf, 0, n);
  st->idx = n;
}

static void
ottery_setkey(struct ottery_rng *st, const unsigned char key[KEYLEN])
{
  chacha20_blocks(key, N_BLOCKS, st->buf);
  st->idx = 0;
}

