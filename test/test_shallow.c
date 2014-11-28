

#ifdef OTTERY_STRUCT
static void
test_shallow_sizeof(void *arg)
{
  (void)arg;

  tt_uint_op(sizeof(struct ottery_state), <=, ottery_st_state_size());
 end:
  ;
}
#endif

static void
test_shallow_unsigned(void *arg)
{
  unsigned acc = 0, dec = ~acc;
  uint64_t acc64 = 0, dec64 = ~acc;
  unsigned acc16 = 0, dec16 = 0xffff;
  int i;
  DECLARE_STATE();
  INIT_STATE();

  for (i = 0; i < 100; ++i) {
    unsigned u = OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);
    uint64_t u64 = OTTERY_PUBLIC_FN(random64)(OTTERY_STATE_ARG_OUT);
    unsigned u16 = OTTERY_PUBLIC_FN(random_uniform)(OTTERY_STATE_ARG_OUT COMMA 65536);
    acc |= u;
    acc64 |= u64;
    acc16 |= u16;
    dec &= u;
    dec64 &= u64;
    dec16 &= u16;
  }
  tt_want(dec == 0);
  tt_want(dec64 == 0);
  tt_want(dec16 == 0);
  tt_want(acc == ~dec);
  tt_want(acc64 == ~dec64);
  tt_want(acc16 == 0xffff);

  RELEASE_STATE();
}

static void
test_shallow_uniform(void *arg)
{
  int i;
  int count[5];
  const uint64_t quite_big = ((uint64_t)1)<<50;
  const uint64_t bigger_still = (((uint64_t)1)<<51) - (quite_big / 5);
  int got_a_big_one = 0;
  int got_a_big_small_one = 0;
  DECLARE_STATE();

  memset(count, 0, sizeof(count));
  INIT_STATE();
  for (i = 0; i < 1000; ++i) {
    unsigned u = OTTERY_PUBLIC_FN(random_uniform)(OTTERY_STATE_ARG_OUT COMMA 5);
    uint64_t u64 = OTTERY_PUBLIC_FN(random_uniform64)(OTTERY_STATE_ARG_OUT COMMA bigger_still);
    tt_int_op(u, <, 5);
    tt_assert(u64 < bigger_still);
    count[u]++;
    tt_int_op(OTTERY_PUBLIC_FN(random_uniform)(OTTERY_STATE_ARG_OUT COMMA 10), <, 10);
    tt_assert(OTTERY_PUBLIC_FN(random_uniform64)(OTTERY_STATE_ARG_OUT COMMA 10) < 10);
    if (OTTERY_PUBLIC_FN(random_uniform)(OTTERY_STATE_ARG_OUT COMMA 3000000000U) > 2000000000U)
      ++got_a_big_small_one;

    if (u64 > quite_big)
      ++got_a_big_one;
  }
  for (i = 0; i < 5; ++i) {
    tt_int_op(0, !=, count[i]);
  }
  tt_assert(got_a_big_one);
  tt_assert(got_a_big_small_one);

 end:
  RELEASE_STATE();
}

static void
test_shallow_buf(void *arg)
{
  uint32_t acc1[5], acc2[5];
  uint32_t dec1[5], dec2[5];
  uint32_t buf1[6];
  uint32_t buf2[6];
  int i, j;
  DECLARE_STATE();
  INIT_STATE();
  (void)arg;

  for (i = 0; i < 5; ++i) {
    acc1[i] = 0;
    dec1[i] = ~0;
    acc2[i] = 0;
    dec2[i] = ~0;
  }

  memset(buf1, 0xcc, sizeof(buf1));
  memset(buf2, 0xdd, sizeof(buf2));

  for (j = 0; j < 100; ++j) {

    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf1, 19);
    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf2, 20);
    tt_int_op(buf1[5], ==, 0xcccccccc);
    tt_int_op(buf2[5], ==, 0xdddddddd);
    
    for (i = 0; i < 5; ++i) {
      acc1[i] |= buf1[i];
      acc2[i] |= buf2[i];
      dec1[i] &= buf1[i];
      dec2[i] &= buf2[i];
    }
  }

  for (i = 0; i < 4; ++i) {
    tt_want(acc1[i] == 0xffffffff);
    tt_want(acc2[i] == 0xffffffff);
    tt_want(dec1[i] == 0);
    tt_want(dec2[i] == 0);
  }

  tt_want(acc2[4] == 0xffffffff);
  tt_want(dec2[4] == 0);

  tt_mem_op(&acc1[4], ==, "\xff\xff\xff\xcc", 4);
  tt_mem_op(&dec1[4], ==, "\x00\x00\x00\xcc", 4);

 end:
  RELEASE_STATE();
}

static void
test_manual_reseed(void *arg)
{
  u8 buf[2500];
  DECLARE_STATE();
  (void)arg;
  INIT_STATE();

  OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf, sizeof(buf));
  tt_int_op(STATE_FIELD(init_counter), ==, 1);

  OTTERY_PUBLIC_FN2(need_reseed)(OTTERY_STATE_ARG_OUT);
  tt_int_op(STATE_FIELD(init_counter), ==, 1);
  OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);
  tt_int_op(STATE_FIELD(init_counter), ==, 2);
  tt_int_op(RNG->idx, ==, sizeof(unsigned));

 end:
  RELEASE_STATE();
}

static void
test_auto_reseed(void *arg)
{
  int i = 0;
  u8 buf[2500];
  const unsigned count = (BUFLEN - KEYLEN) * RESEED_AFTER_BLOCKS;
  const unsigned blocks = (count+sizeof(buf)-1) / sizeof(buf);
  DECLARE_STATE();
  INIT_STATE();

  for (i = 0; i < blocks+50; ++i) {
    OTTERY_PUBLIC_FN(random_buf)(OTTERY_STATE_ARG_OUT COMMA buf, sizeof(buf));
    /* printf("i=%d, count=%d\n", i, (int) RNG->count); */
    if (i <= blocks) {
      tt_int_op(STATE_FIELD(init_counter), ==, 1);
    } else {
      tt_int_op(STATE_FIELD(init_counter), ==, 2);
    }
  }

 end:
  RELEASE_STATE();
}

static void
test_shallow_status_1(void *arg)
{
  int status;
  DECLARE_STATE();
  (void)arg;

#ifdef OTTERY_STRUCT
  state = calloc(1, sizeof(struct ottery_state));
#endif
  tt_int_op(STATE_FIELD(init_counter), ==, 0);

  status = OTTERY_PUBLIC_FN2(status)(OTTERY_STATE_ARG_OUT);

  tt_int_op(status, >=, 1);
  if (status != 2) {
    TT_DECLARE("WARN", ("RNG initialization seems to have failed: %d\n",
                        status));
  }

  tt_int_op(STATE_FIELD(init_counter), ==, 1);

 end:
  RELEASE_STATE();
}

static void
test_shallow_status_2(void *arg)
{
  int status;
  DECLARE_STATE();
  (void)arg;

  INIT_STATE();

  /* This time, check the status after it's initialized */
  OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);

  tt_int_op(STATE_FIELD(init_counter), ==, 1);
  status = OTTERY_PUBLIC_FN2(status)(OTTERY_STATE_ARG_OUT);

  tt_int_op(status, >=, 1);
  if (status != 2) {
    TT_DECLARE("WARN", ("RNG initialization seems to have failed: %d\n",
                        status));
  }

  tt_int_op(STATE_FIELD(init_counter), ==, 1);

 end:
  RELEASE_STATE();
}

static void
test_shallow_addrandom(void *arg)
{
  u8 buf[600] = "708901345660";
  u8 b2[OTTERY_DIGEST_LEN*2];
  u8 newkey[OTTERY_DIGEST_LEN];
  u8 next[BLOCKSIZE];

  unsigned u;

  DECLARE_STATE();

  (void)arg;

  INIT_STATE();

  OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);
  tt_int_op(RNG->idx, ==, sizeof(unsigned));
  OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);
  tt_int_op(RNG->idx, ==, sizeof(unsigned)*2);

  /* Reconstruct the key we'll see */
  memcpy(b2, RNG->buf + RNG->idx, OTTERY_DIGEST_LEN);
  ottery_digest(b2+OTTERY_DIGEST_LEN, buf, sizeof(buf));
  ottery_digest(newkey, b2, sizeof(b2));

  chacha20_blocks(newkey, 1, next);

  OTTERY_PUBLIC_FN2(addrandom)(OTTERY_STATE_ARG_OUT COMMA buf, sizeof(buf));
  tt_int_op(RNG->idx, ==, 0);

  u = OTTERY_PUBLIC_FN(random)(OTTERY_STATE_ARG_OUT);

  tt_mem_op(&u, ==, next, 4);

  /* And let's make sure this is a no-op */
  tt_int_op(RNG->idx, ==, sizeof(unsigned));
  OTTERY_PUBLIC_FN2(addrandom)(OTTERY_STATE_ARG_OUT COMMA buf, -1);
  tt_int_op(RNG->idx, ==, sizeof(unsigned));

 end:
  RELEASE_STATE();
}

static struct testcase_t shallow_tests[] = {

  { "unsigned", test_shallow_unsigned, TT_FORK, NULL, NULL },
  { "range", test_shallow_uniform, TT_FORK, NULL, NULL },
  { "buf", test_shallow_buf, TT_FORK, NULL, NULL },
  { "reseed_manually", test_manual_reseed, TT_FORK, NULL, NULL },
  { "reseed_after_data", test_auto_reseed, TT_FORK, NULL, NULL },
  { "status_1", test_shallow_status_1, TT_FORK, NULL, NULL },
  { "status_2", test_shallow_status_2, TT_FORK, NULL, NULL },
  { "addrandom", test_shallow_addrandom, TT_FORK, NULL, NULL },

#ifdef OTTERY_STRUCT
  { "sizeof", test_shallow_sizeof, TT_FORK, NULL, NULL },
#endif
  END_OF_TESTCASES
};
