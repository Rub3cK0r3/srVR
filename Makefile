# Simple but professional Makefile for srVR

CC      := gcc
TARGET  := srVR
SRCS    := src/main.c src/server.c src/http.c src/router.c src/ev.c
OBJS    := $(SRCS:.c=.o)
INCLUDES:= -Iinclude

# Debug flags: extra warnings and debug symbols.
CFLAGS_DEBUG   := -std=c11 -Wall -Wextra -pedantic -g $(INCLUDES)
# Release flags: optimised build with assertions disabled.
CFLAGS_RELEASE := -std=c11 -Wall -Wextra -pedantic -O2 -DNDEBUG $(INCLUDES)

LDFLAGS := -lpthread

TEST_TARGET := test_epoll
TEST_SRCS   := tests/test_epoll.c src/server.c src/http.c src/router.c src/ev.c
TEST_OBJS   := $(TEST_SRCS:.c=.o)

.PHONY: all debug release clean test test-epoll

# By default build a debug binary; this is the most useful
# configuration during development and when exploring the code.
all: debug

# Debug build keeps symbols and disables optimisations, which
# makes it friendly for debuggers and learning.
debug: CFLAGS := $(CFLAGS_DEBUG)
debug: $(TARGET)

# Release build enables optimisations and defines NDEBUG so
# that expensive assertions can be compiled out.
release: CFLAGS := $(CFLAGS_RELEASE)
release: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Remove all build artefacts so we can start from a clean tree.
clean:
	$(RM) $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)

# Build and run tests
test: CFLAGS := $(CFLAGS_DEBUG)
test: test-epoll

test-epoll: CFLAGS := $(CFLAGS_DEBUG)
test-epoll: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

