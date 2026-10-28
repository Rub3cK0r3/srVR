# Simple but professional Makefile for srVR
CC      := gcc
TARGET  := bin/srVR
SRCS    := src/main.c src/server.c src/http.c src/router.c src/ev.c
INCLUDES:= -Iinclude

# Debug flags: extra warnings and debug symbols.
CFLAGS_DEBUG   := -std=c11 -Wall -Wextra -pedantic -g $(INCLUDES)
# Release flags: optimised build with assertions disabled.
CFLAGS_RELEASE := -std=c11 -Wall -Wextra -pedantic -O2 -DNDEBUG $(INCLUDES)

# Default to debug flags for pattern matching rules
CFLAGS := $(CFLAGS_DEBUG)
LDFLAGS := -lpthread

# Map src/%.c directly to build/%.o (¡Aislamiento!)
OBJS    := $(patsubst src/%.c, build/%.o, $(SRCS))

# Test configurations
TEST_TARGET := bin/test_epoll
TEST_SRCS   := tests/test_epoll.c src/server.c src/http.c src/router.c src/ev.c
# Convierte todas las fuentes de test a la carpeta build/ para proteger src/ y tests/
TEST_OBJS   := $(patsubst src/%.c, build/%.o, $(patsubst tests/%.c, build/tests/%.o, $(TEST_SRCS)))

.PHONY: all debug release clean test test-epoll test-server

# By default build a debug binary
all: debug

# Target-specific variables to safely swap flags
debug: CFLAGS := $(CFLAGS_DEBUG)
debug: $(TARGET)

release: CFLAGS := $(CFLAGS_RELEASE)
release: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for core source files (Outputs to build/)
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for test files (Outputs to build/tests/)
build/tests/%.o: tests/%.c
	@mkdir -p build/tests
	$(CC) $(CFLAGS) -c $< -o $@

# TEST TARGETS
test: CFLAGS := $(CFLAGS_DEBUG)
test: test-epoll test-server

test-epoll: CFLAGS := $(CFLAGS_DEBUG)
test-epoll: $(TEST_OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_OBJS) $(LDFLAGS)
	./$(TEST_TARGET)
test-server:
	./tests/test_server.sh
clean:
	rm -rf build bin

