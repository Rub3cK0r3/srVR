# Simple but professional Makefile for srVR

CC      := gcc
TARGET  := srVR
SRCS    := src/main.c src/server.c src/http.c src/router.c
OBJS    := $(SRCS:.c=.o)
INCLUDES:= -Iinclude

# Debug flags: extra warnings and debug symbols.
CFLAGS_DEBUG   := -std=c11 -Wall -Wextra -pedantic -g $(INCLUDES)
# Release flags: optimised build with assertions disabled.
CFLAGS_RELEASE := -std=c11 -Wall -Wextra -pedantic -O2 -DNDEBUG $(INCLUDES)

LDFLAGS := -lpthread

.PHONY: all debug release clean

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
	$(RM) $(OBJS) $(TARGET)

