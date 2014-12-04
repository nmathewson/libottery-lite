/*
   To the extent possible under law, Nick Mathewson has waived all copyright and
   related or neighboring rights to libottery-lite, using the creative commons
   "cc0" public domain dedication.  See doc/cc0.txt or
   <http://creativecommons.org/publicdomain/zero/1.0/> for full details.
 */

#define MAXSKIP 8192

static void
dumphex(const char *label, const u8 *bytes, int n)
{
  int m = 0;

  if (label)
    printf("%s: ", label);
  while (n--)
    {
      printf("%02x", *bytes++);
      if (0 == (++m % 32) && n)
        printf("\n");
    }
  puts("");
}

static void
experiment(const u8 *key, const u8 *nonce, unsigned skip)
{
#define OUTPUT 512
  u8 buf[MAXSKIP + OUTPUT];
  u8 k[OTTERY_KEYLEN];
  unsigned n = skip + OUTPUT;

  tt_int_op(n, <=, sizeof(buf));

  memset(k, 0, sizeof(k));
  memcpy(k, key, 32);
  memcpy(k + 32, nonce, 8);

  chacha20_blocks(k, n / CHACHA_BLOCKSIZE, buf);

  puts("================================================================");
  dumphex("   key", key, 32);
  dumphex(" nonce", nonce, 8);
  printf("offset: %d\n", skip);

  dumphex(NULL, buf + skip, OUTPUT);
end:
  ;
#undef OUTPUT
}

#define X(key, nonce, skip)                                     \
  do {                                                          \
      experiment((const u8*)(key), (const u8*)(nonce), (skip));   \
    } while (0)


static void
dump_chacha20_test_vectors(void *arg)
{
  (void)arg;
  X("helloworld!helloworld!helloworld", "!hellowo", 0);
  X("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0", 0);
  X("helloworld!helloworld!helloworld", "!hellowo", 8192);
  X("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0", 8192);
  X("Zombie ipsum reversus ab viral i", "nferno, ", 128);
  X("nam rick grimes malum cerebro. D", "e carne ", 512);
  X("lumbering animata corpora quaeri", "tis. Sum", 640);
  X("mus brains sit, morbo vel malefi", "cia? De ", 704);
}

static struct testcase_t chacha_testvectors_tests[] = {
  { "make_chacha_testvectors", dump_chacha20_test_vectors,
    TT_OFF_BY_DEFAULT, NULL, 0 },
  END_OF_TESTCASES
};
