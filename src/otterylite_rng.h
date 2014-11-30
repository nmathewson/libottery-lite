/* otterylite_rng.h -- implement the core RNG construction for
   libottery-lite. */

/*
  To the extent possible under law, Nick Mathewson has waived all copyright and
  related or neighboring rights to libottery-lite, using the creative commons
  "cc0" public domain dedication.  See doc/cc0.txt or
  <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
*/

/* The first half of this file defines the ChaCha20 stream cipher.

   For more information on ChaCha, see http://cr.yp.to/chacha.html
 */

#define CHACHA_BLOCKSIZE 64
#define CHACHA_ROUNDS 20
#define CHACHA_KEYLEN 32
#define CHACHA_IVLEN 8

#define CHACHA_QUARTER_ROUND(a, b, c, d)         \
  do {                                           \
    a += b;                                      \
    d = ROTL32(d ^ a, 16);                       \
    c += d;                                      \
    b = ROTL32(b ^ c, 12);                       \
    a += b;                                      \
    d = ROTL32(d ^ a, 8);                        \
    c += d;                                      \
    b = ROTL32(b ^ c, 7);                        \
  } while (0)

static void
chacha20_blocks(const unsigned char key[CHACHA_KEYLEN+CHACHA_IVLEN],
                size_t n_blocks,
                unsigned char *const output)
{
  uint32_t x[16];
  uint32_t y[16];
  size_t i;
  int j;
  unsigned char *outp = output;

  x[0] = 1634760805u;
  x[1] = 857760878u;
  x[2] = 2036477234u;
  x[3] = 1797285236u;
  read_u32_le(&x[4], key, 8);
  x[12] = 0;
  x[13] = 0;
  read_u32_le(&x[14], key + 32, 2);

  for (i = 0; i < n_blocks; ++i)
    {
      x[12] = (i & 0xffffffffu);
#if SIZE_MAX > 0xffffffff
      x[13] = (i >> 32);
#else
      x[13] = 0;
#endif
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

      write_u32_le(outp, y, 16);
      outp += sizeof(y);
    }

  memwipe(x, sizeof(x));
  memwipe(y, sizeof(y));
}

#undef CHACHA_QUARTER_ROUND

/* The amount of secret material that we use to fill an ottery buffer.

   (Even though we're using 320 bits here, I do not claim 320-bit security.)
 */
#define OTTERY_KEYLEN (CHACHA_KEYLEN + CHACHA_IVLEN)

/* How much space should we allocate for ChaCha output?  (We subtract
 * CHACHA_BLOCKSIZE so we have some extra space to fit the whole struct
 * ottery_rng in a single 4K mmaped page.) */
#define OTTERY_BUFLEN (4096 - CHACHA_BLOCKSIZE)

/* How many ChaCha blocks should we make at a time */
#define OTTERY_N_BLOCKS (OTTERY_BUFLEN / CHACHA_BLOCKSIZE)

/* This is the inner RNG structure that we use to generate output. */
struct ottery_rng {
  /* This is set to RNG_MAGIC.  If we notice that it's been cleared to
     zero on an INHERIT_ZERO system, we know that we've forked */
  unsigned magic;
  /* This is an index into buf.  It must be no more than BUFLEN - KEYLEN.
   */
  unsigned idx;
  /*
    How many times have we regenerated buf?  If this gets large, we rekey.
   */
  unsigned count;
  /*
     For all 0 <= j < idx, buf[j] contains 0.

     For all j >= idx, buf[j] contains a random byte generated with
     the previous key material.

     The last KEYLEN bytes of buf will be the key material for the next
     buffer.
   */
  unsigned char buf[OTTERY_BUFLEN];
};

/*
  Helper function to implement the slow-path of ottery_bytes.

  We don't have enough buffered bytes to fulfil the request straight from
  the buffer, so we will need to call the ChaCha core.

  We're going to use the rng 'st' to fill 'out' with 'n' bytes.  We already
  counted how many bytes we have queued, and found 'available_bytes'.

 */
static void
ottery_bytes_slow(struct ottery_rng *st, u8 *out, size_t n,
                  size_t available_bytes)
{
  /* First, give them the bytes that we have. */
  memcpy(out, st->buf + st->idx, available_bytes);
  out += available_bytes;
  n -= available_bytes;

  /* Then, so long as we're giving them a whole buffer at once, generate
     more buffers and copy them out.

     The original libottery had an optimization for this case, where it
     write data straight into the output buffer.  But doing it this way
     simplifies the code a lot.
  */
  while (n > OTTERY_BUFLEN - OTTERY_KEYLEN)
    {
      ++st->count;
      chacha20_blocks(st->buf + OTTERY_BUFLEN - OTTERY_KEYLEN, OTTERY_N_BLOCKS, st->buf);
      memcpy(out, st->buf, OTTERY_BUFLEN - OTTERY_KEYLEN);
      out += (OTTERY_BUFLEN - OTTERY_KEYLEN);
      n -= (OTTERY_BUFLEN - OTTERY_KEYLEN);
    }

  /* Now we're going to generate one more fresh block, and only give part
     of it out. (We might give it all, but no more.) */
  ++st->count;
  chacha20_blocks(st->buf + OTTERY_BUFLEN - OTTERY_KEYLEN, OTTERY_N_BLOCKS, st->buf);
  memcpy(out, st->buf, n);
  memset(st->buf, 0, n);
  st->idx = n;
}

/*
  Core RNG implementation: write 'n' bytes into 'output' using 'st'.
 */
static inline void
ottery_bytes(struct ottery_rng *st, void * const output, size_t n)
{
  size_t available_bytes = OTTERY_BUFLEN - OTTERY_KEYLEN - st->idx;
  u8 *out = output;

  if (LIKELY(n <= available_bytes))
    {
      /* Fast path: we don't need to generate more bytes; we can fulfil
         this from our buffer.
       */
      memcpy(out, st->buf + st->idx, n);
      memset(st->buf + st->idx, 0, n);
      st->idx += n;
    }
  else
    {
      ottery_bytes_slow(st, output, n, available_bytes);
    }
}


/*
  Replace the existing material in 'st' with material generated using 'key'
 */
static void
ottery_setkey(struct ottery_rng *st, const unsigned char key[OTTERY_KEYLEN])
{
  chacha20_blocks(key, OTTERY_N_BLOCKS, st->buf);
  st->idx = 0;
  st->count = 0;
}

