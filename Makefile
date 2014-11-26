
all: tests benchmarks src/otterylite.o src/otterylite_st.o

HEADERS = \
	src/otterylite_rng.h \
	src/otterylite_digest.h \
	src/otterylite.h \
	src/otterylite_wipe.h \
	src/otterylite_entropy.h \
	src/otterylite-impl.h

TEST_PROGRAMS = \
	test/test_blake2 \
	test/test_chacha \
	test/test_streamgen

BENCH_PROGRAMS = \
	bench/bench

CFLAGS = -I ./src -Wall -O3 -g

CC=gcc

src/otterylite.o: src/otterylite.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

src/otterylite_st.o: src/otterylite.c $(HEADERS)
	$(CC) $(CFLAGS) -c -DOTTERY_STRUCT $< -o $@

tests: $(TEST_PROGRAMS)

benchmarks: $(BENCH_PROGRAMS)

test/test_blake2: test/test_blake2.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

test/test_chacha: test/test_chacha.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

test/test_streamgen: test/test_streamgen.c $(HEADERS) src/otterylite.o
	$(CC) $(CFLAGS) $< src/otterylite.o -o $@

bench/bench: bench/bench.c $(HEADERS) src/otterylite.o
	$(CC) $(CFLAGS) $< src/otterylite.o -o $@

check: all
	./test/test_blake2
	python ./test/make_test_vectors.py > wanted_output
	./test/test_chacha > received_output
	cmp received_output wanted_output

dieharder: ./test/test_streamgen
	./test/test_streamgen --yes-really | dieharder -g 200 -a
