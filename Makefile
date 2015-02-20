
all: tests benchmarks src/otterylite.o src/otterylite_st.o

HEADERS = \
	src/otterylite_rng.h \
	src/otterylite_digest.h \
	src/otterylite.h \
	src/otterylite_wipe.h \
	src/otterylite_entropy.h \
	src/otterylite_fallback.h \
	src/otterylite_fallback_unix.h \
	src/otterylite_fallback_win32.h \
	src/otterylite-impl.h \
	src/otterylite_locking.h \
	src/otterylite_alloc.h

TEST_PROGRAMS = \
	test/test \
        test/test_st \
	test/test_streamgen

BENCH_PROGRAMS = \
	bench/bench

COMMON_CFLAGS = $(EXTRA_CFLAGS) -I ./src -Wall -Wextra -Werror -pthread
EXTRA_CFLAGS = -W -Wfloat-equal -Wundef -Wpointer-arith -Wmissing-prototypes -Wwrite-strings -Wredundant-decls -Wchar-subscripts -Wcomment -Wformat=2 -Wwrite-strings -Wmissing-declarations -Wredundant-decls -Wnested-externs -Wbad-function-cast -Wswitch-enum -Werror -Winit-self -Wmissing-field-initializers -Wold-style-definition -Waddress -Wmissing-noreturn -Wstrict-overflow=1 -Wdeclaration-after-statement
# -Wshorten-64-to-32 -Wstrict-prototypes

CFLAGS = $(COMMON_CFLAGS) -O3

TEST_CFLAGS = $(COMMON_CFLAGS) --coverage -g -I test/tinytest
# XXXX figure out how to make coverage work here too.
TEST_CFLAGS2 = $(COMMON_CFLAGS) -g -I test/tinytest

CC=gcc

src/otterylite.o: src/otterylite.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

src/otterylite_st.o: src/otterylite.c $(HEADERS)
	$(CC) $(CFLAGS) -c -DOTTERY_STRUCT $< -o $@

tests: $(TEST_PROGRAMS)

benchmarks: $(BENCH_PROGRAMS)

test/tinytest/tinytest.o: test/tinytest/tinytest.c
	$(CC) $(TEST_CFLAGS) -c $< -o $@

TEST_DEPS = test/test_main.c test/test_blake2.c test/test_chacha.c test/test_egd.c test/test_entropy.c test/test_fork.c test/test_rng_core.c test/test_shallow.c $(HEADERS) src/otterylite.c test/tinytest/tinytest.o

test/test: $(TEST_DEPS)
	$(CC) $(TEST_CFLAGS) test/tinytest/tinytest.o $< -o $@

test/test_st: $(TEST_DEPS)
	$(CC) $(TEST_CFLAGS2) -DOTTERY_STRUCT test/tinytest/tinytest.c $< -o $@

test/test_streamgen: test/test_streamgen.c $(HEADERS) src/otterylite.o
	$(CC) $(CFLAGS) $< src/otterylite.o -o $@

bench/bench: bench/bench.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

wanted_output: ./test/make_test_vectors.py
	python ./test/make_test_vectors.py > wanted_output

check: all wanted_output
	rm -f test_main.gcda
	./test/test
	./test/test --quiet chacha_dump/make_chacha_testvectors +chacha_dump/make_chacha_testvectors > received_output
	cmp received_output wanted_output

coverage:
	gcov -o . test/test_main.c

dieharder: ./test/test_streamgen
	./test/test_streamgen --yes-really | dieharder -g 200 -a -Y1

clean:
	rm -f *.o */*.o */*/*.o $(TEST_PROGRAMS) $(BENCH_PROGRAMS) wanted_output received_output

