CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude
SANFLAGS ?= -fsanitize=address,undefined -g

COMMON = src/util.c src/batch.c src/lex.c src/parse.c src/bpt.c src/store.c src/exec.c src/cli.c
APP_SRC = $(COMMON) src/main.c src/gen_perf.c
GEN_SRC = $(COMMON) src/gen_perf.c
UNIT_SRC = tests/test_unit.c $(COMMON) src/gen_perf.c
FUNC_SRC = tests/test_func.c $(COMMON) src/gen_perf.c

APP = build/sql2_books
UNIT = build/test_unit
FUNC = build/test_func
GEN = build/gen_perf

.PHONY: all clean test san perf

all: $(APP) $(UNIT) $(FUNC) $(GEN)

build:
	mkdir -p build

$(APP): build $(APP_SRC)
	$(CC) $(CFLAGS) $(APP_SRC) -o $(APP)

$(UNIT): build $(UNIT_SRC)
	$(CC) $(CFLAGS) -DNO_MAIN $(UNIT_SRC) -o $(UNIT)

$(FUNC): build $(FUNC_SRC)
	$(CC) $(CFLAGS) -DNO_MAIN $(FUNC_SRC) -o $(FUNC)

$(GEN): build $(GEN_SRC)
	$(CC) $(CFLAGS) -DPERF_MAIN $(GEN_SRC) -o $(GEN)

test: $(UNIT) $(FUNC)
	./$(UNIT)
	./$(FUNC)

san: build
	$(CC) $(CFLAGS) $(SANFLAGS) $(APP_SRC) -o $(APP)-san
	$(CC) $(CFLAGS) $(SANFLAGS) -DNO_MAIN $(UNIT_SRC) -o $(UNIT)-san
	$(CC) $(CFLAGS) $(SANFLAGS) -DNO_MAIN $(FUNC_SRC) -o $(FUNC)-san
	./$(UNIT)-san
	./$(FUNC)-san

perf: $(APP) $(GEN)
	./$(GEN) data/perf_books.bin 1000000
	./$(APP) --mode cli --data data/perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id = 1000000;"
	./$(APP) --mode cli --data data/perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id BETWEEN 999001 AND 1000000;"
	./$(APP) --mode cli --data data/perf_books.bin --summary-only --batch "SELECT * FROM books WHERE author = 'Author 999';"
	./$(APP) --mode cli --data data/perf_books.bin --summary-only --batch "SELECT * FROM books WHERE genre = 'Genre 7';"

clean:
	rm -rf build

