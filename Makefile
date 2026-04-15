CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -g
CPPFLAGS ?= -Iinclude

CORE_SRC := src/bptree.c src/db.c src/error.c src/executor.c src/input.c src/parser.c src/printer.c src/storage.c src/transaction.c
MAIN_SRC := src/main.c

TEST_BINS := build/test_bptree build/test_parser build/test_storage build/test_executor

.PHONY: all clean test acceptance fixtures asan perf

all: bin/minisql build/make_fixture build/perf

bin/minisql: $(CORE_SRC) $(MAIN_SRC) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_SRC) $(MAIN_SRC) -o $@

build/test_bptree: tests/test_bptree.c src/bptree.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_bptree.c src/bptree.c -o $@

build/test_parser: tests/test_parser.c src/parser.c src/error.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_parser.c src/parser.c src/error.c -o $@

build/test_storage: tests/test_storage.c src/bptree.c src/db.c src/error.c src/storage.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_storage.c src/bptree.c src/db.c src/error.c src/storage.c -o $@

build/test_executor: tests/test_executor.c $(CORE_SRC) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_executor.c $(CORE_SRC) -o $@

build/make_fixture: tools/make_fixture.c src/bptree.c src/db.c src/error.c src/storage.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/make_fixture.c src/bptree.c src/db.c src/error.c src/storage.c -o $@

build/perf: tools/perf.c src/bptree.c src/db.c src/error.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/perf.c src/bptree.c src/db.c src/error.c -o $@

bin build data:
	mkdir -p $@

fixtures: build/make_fixture | data
	./build/make_fixture data/default.msqldb

test: $(TEST_BINS)
	./build/test_bptree
	./build/test_parser
	./build/test_storage
	./build/test_executor

acceptance: all
	sh tests/acceptance.sh

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer" test acceptance

perf: build/perf
	./build/perf 1000000

clean:
	rm -rf bin build data/default.msqldb
