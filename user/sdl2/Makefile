# VOS SDL2 Shim Makefile
# Builds libvos-sdl2.a static library

# Cross toolchain - same as main VOS Makefile
CROSS_PREFIX ?= $(CURDIR)/../../toolchain/opt/cross
CROSS_TARGET ?= i686-elf
CC = $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-gcc
AR = $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-ar

# User programs need newlib
NEWLIB_PREFIX = $(CURDIR)/../../toolchain/opt/newlib
NEWLIB_INC = $(NEWLIB_PREFIX)/$(CROSS_TARGET)/include
NEWLIB_LIB = $(NEWLIB_PREFIX)/$(CROSS_TARGET)/lib

CFLAGS = -O2 -Wall -Wextra -ffreestanding -nostdlib \
         -I./include -I.. -I$(NEWLIB_INC)

SRCS = src/SDL_main.c \
       src/SDL_video.c \
       src/SDL_audio.c \
       src/SDL_events.c \
       src/SDL_timer.c

OBJS = $(SRCS:.c=.o)
LIB = libvos-sdl2.a

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB) test_sdl.o

# Test program (requires full VOS build environment)
# This is just for reference - actual building should be done via main VOS Makefile
test: $(LIB)
	@echo "To build test_sdl, add it to the main VOS Makefile"
	@echo "See user/sdl2/test_sdl.c for example code"

.PHONY: all clean test
