
static void
test_rng_core_construction_short(void *arg)
{
  u8 key[KEYLEN] = "test written on I-95 on 26 Nov 2015fnord";
  u8 stream[BUFLEN * 2];
  struct ottery_rng rng;
  u8 tmp[128];
  u8 *streamp = stream;
  int i;
  uint64_t ones = 0, zeros = ~(uint64_t)0, t;

  ottery_setkey(&rng, key);
  chacha20_blocks(key, BUFLEN / 64, stream);
  chacha20_blocks(stream + BUFLEN - KEYLEN, BUFLEN / 64, stream + BUFLEN - KEYLEN);

  tt_assert(! iszero(rng.buf, 8008));

  for (i = 0; i < 8000; i += 13) {
    ottery_bytes(&rng, tmp, 1);
    tt_mem_op(tmp, ==, streamp, 1);
    streamp++;
    ottery_bytes(&rng, tmp, 3);
    tt_mem_op(tmp, ==, streamp, 3);
    streamp += 3;
    ottery_bytes(&rng, &t, 8);
    tt_mem_op(&t, ==, streamp, 8);
    ones |= t;
    zeros &= t;
    streamp += 8;
    ottery_bytes(&rng, tmp, 1);
    tt_mem_op(tmp, ==, streamp, 1);
    streamp += 1;
    if (i < BUFLEN - KEYLEN) {
      tt_assert(iszero(rng.buf, i));
    } else {
      tt_assert(iszero(rng.buf, i - (BUFLEN - KEYLEN)));
    }
  }

  tt_int_op(i, ==, 8008);

  /* Make sure every bit is sometimes 1 and sometimes 0. */
  tt_assert(zeros == 0);
  tt_assert(ones == ~(uint64_t)0);

 end:
  ;
}

static void
test_rng_core_construction_long(void *arg)
{
  const int nbufs = 30;
  u8 *stream = malloc((BUFLEN-KEYLEN) * nbufs + KEYLEN), *streamp;
  u8 key[KEYLEN] = "ottery:!THE NEW DANCE REMIX!1!ha1l3r15.", *keyp;
  u8 *tmp = malloc(5003);
  int i;
  struct ottery_rng rng;
  int streamlen;

  ottery_setkey(&rng, key);

  tt_int_op(nbufs, <, RESEED_AFTER_BLOCKS);

  tt_assert(stream);
  tt_assert(tmp);

  streamp = stream;
  for (i = 0; i < nbufs; ++i) {
    chacha20_blocks(key, BUFLEN / 64, streamp);
    tt_assert(!iszero(streamp + BUFLEN - 16, 16));
    keyp = streamp + BUFLEN - KEYLEN;
    memcpy(key, keyp, KEYLEN);
    memset(keyp, 0, KEYLEN);
    streamp = keyp;
  }

  streamlen = streamp - stream;
  tt_int_op(streamlen, ==, (BUFLEN - KEYLEN) * nbufs);

  streamp = stream;

  for (i = 0; i < streamlen-5003; i += 5003) {
    /* CHECK COUNT */
    ottery_bytes(&rng, tmp, 5003);
    tt_mem_op(tmp, ==, streamp, 5003);
    tt_assert(iszero(rng.buf, rng.idx));
    streamp += 5003;
  }

 end:
  if (stream)
    free(stream);
  if (tmp)
    free(tmp);
}

static struct testcase_t rng_core_tests[] = {
  { "short_requests", test_rng_core_construction_short, 0, NULL, NULL },
  { "long_requests", test_rng_core_construction_long, 0, NULL, NULL },
  END_OF_TESTCASES
};
