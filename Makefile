# Makefile for llquery - Fast query Parser

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O3 -std=c99 -funroll-loops
DEBUG_CFLAGS = -Wall -Wextra -g -std=c99 -fsanitize=address

# Library
LIB_SRC = llquery.c
LIB_OBJ = llquery.o
LIB_STATIC = libllquery.a
LIB_SHARED = libllquery.so

# Test
TEST_SRC = test_llquery.c
TEST_BIN = test_llquery

# Example
EXAMPLE_SRC = example.c
EXAMPLE_BIN = example

# Benchmark
BENCH_SRC = benchmark.c
BENCH_BIN = benchmark
BENCH_CFLAGS = $(CFLAGS) -D_POSIX_C_SOURCE=199309L

.PHONY: all clean test example run-example benchmark run-benchmark

all: $(LIB_STATIC) $(LIB_SHARED) benchmark

# Static library
$(LIB_STATIC): $(LIB_OBJ)
	ar rcs $@ $^

# Shared library
$(LIB_SHARED): $(LIB_SRC)
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^

# Object file
$(LIB_OBJ): $(LIB_SRC) llquery.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Test binary
$(TEST_BIN): $(TEST_SRC) $(LIB_STATIC)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_STATIC)

# Example binary
$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(LIB_STATIC)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_STATIC)

# Benchmark binary
$(BENCH_BIN): $(BENCH_SRC) $(LIB_STATIC)
	$(CC) $(BENCH_CFLAGS) -o $@ $< $(LIB_STATIC)

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Build example
example: $(EXAMPLE_BIN)

# Run example
run-example: $(EXAMPLE_BIN)
	./$(EXAMPLE_BIN)

# Build benchmark
benchmark: $(BENCH_BIN)

# Run benchmark
run-benchmark: $(BENCH_BIN)
	./$(BENCH_BIN)

# Debug build
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean $(TEST_BIN) $(BENCH_BIN)
	ASAN_OPTIONS=verbosity=2:abort_on_error=0 ./$(TEST_BIN)
	ASAN_OPTIONS=verbosity=2:abort_on_error=0 ./$(BENCH_BIN)

clean:
	rm -f $(LIB_OBJ) $(LIB_STATIC) $(LIB_SHARED) $(TEST_BIN) $(EXAMPLE_BIN) $(BENCH_BIN)

# Install (optional)
install: $(LIB_STATIC) $(LIB_SHARED)
	install -d /usr/local/lib
	install -m 644 $(LIB_STATIC) /usr/local/lib/
	install -m 755 $(LIB_SHARED) /usr/local/lib/
	install -d /usr/local/include
	install -m 644 llquery.h /usr/local/include/

uninstall:
	rm -f /usr/local/lib/$(LIB_STATIC)
	rm -f /usr/local/lib/$(LIB_SHARED)
	rm -f /usr/local/include/llquery.h
