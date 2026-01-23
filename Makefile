# VOS Makefile

# Tools
AS = nasm
AR = $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-ar

# Cross toolchain (built by tools/build_newlib_toolchain.sh)
CROSS_PREFIX ?= $(CURDIR)/toolchain/opt/cross
CROSS_TARGET ?= i686-elf
CROSS_CC := $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-gcc
CROSS_LD := $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-ld

ifeq ($(wildcard $(CROSS_CC)),)
$(error Missing cross toolchain at $(CROSS_CC). Run `./tools/build_newlib_toolchain.sh`)
endif

CC = $(CROSS_CC)
LD = $(CROSS_LD)

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
INCLUDE_DIR = include
THIRD_PARTY_DIR = third_party
BUILD_DIR = build
ISO_DIR = iso
TOOLS_DIR = tools
TOOLS_BUILD_DIR = $(BUILD_DIR)/tools
FONTS_DIR = $(THIRD_PARTY_DIR)/fonts
FONTS_BUILD_DIR = $(BUILD_DIR)/fonts

# Flags
ASFLAGS = -f elf32
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -nostdlib \
         -Wall -Wextra -I$(INCLUDE_DIR) -I$(THIRD_PARTY_DIR)/microrl -O2 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Source files
ASM_SOURCES = $(BOOT_DIR)/boot.asm
C_SOURCES = $(wildcard $(KERNEL_DIR)/*.c)

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
MICRORL_OBJ = $(BUILD_DIR)/microrl.o

# Additional PSF2 fonts (embedded as binary objects).
EXTRA_FONT_PSF = \
	$(FONTS_DIR)/spleen/spleen-12x24.psf \
	$(FONTS_DIR)/spleen/spleen-16x32.psf \
	$(FONTS_DIR)/spleen/spleen-32x64.psf \
	$(FONTS_DIR)/terminus/Uni3-Terminus28x14.psf \
	$(FONTS_DIR)/terminus/Uni3-TerminusBold32x16.psf

EXTRA_FONT_OBJS = $(patsubst $(FONTS_DIR)/%.psf,$(FONTS_BUILD_DIR)/%.o,$(EXTRA_FONT_PSF))

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS) $(MICRORL_OBJ) $(EXTRA_FONT_OBJS)

# Output
KERNEL = $(BUILD_DIR)/kernel.bin
ISO = vos.iso

# Initramfs (multiboot module)
INITRAMFS_DIR = initramfs
INITRAMFS_TAR = $(ISO_DIR)/boot/initramfs.tar
INITRAMFS_ROOT = $(BUILD_DIR)/initramfs_root
INITRAMFS_FILES := $(shell find $(INITRAMFS_DIR) -type f 2>/dev/null)
INITRAMFS_DIRS := $(shell find $(INITRAMFS_DIR) -type d 2>/dev/null)

# Simple userland (ELF32 + newlib)
USER_DIR = user
USER_BUILD_DIR = $(BUILD_DIR)/user
USER_ASM_SOURCES = $(USER_DIR)/crt0.asm
USER_ASM_OBJECTS = $(USER_BUILD_DIR)/crt0.o
USER_CRTI_OBJ = $(USER_BUILD_DIR)/crti.o
USER_CRTN_OBJ = $(USER_BUILD_DIR)/crtn.o
USER_RUNTIME_C_SOURCES = $(USER_DIR)/newlib_syscalls.c $(USER_DIR)/env.c
USER_RUNTIME_C_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_RUNTIME_C_SOURCES))
USER_RUNTIME_OBJECTS = $(USER_ASM_OBJECTS) $(USER_RUNTIME_C_OBJECTS)
USER_RUNTIME_LIBS = $(USER_BUILD_DIR)/libvosposix.a

USER_LINK_CMD = $(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $(filter-out $(USER_RUNTIME_LIBS),$^) $(USER_RUNTIME_LIBS) -lc -lgcc
USER_LINK_CMD_MATH = $(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $(filter-out $(USER_RUNTIME_LIBS),$^) $(USER_RUNTIME_LIBS) -lc -lgcc -lm
USER_INIT_OBJ = $(USER_BUILD_DIR)/init.o
USER_ELIZA_OBJ = $(USER_BUILD_DIR)/eliza.o
USER_LSH_OBJ = $(USER_BUILD_DIR)/lsh.o
USER_SH_OBJ = $(USER_BUILD_DIR)/sh.o
USER_UPTIME_OBJ = $(USER_BUILD_DIR)/uptime.o
USER_DATE_OBJ = $(USER_BUILD_DIR)/date.o
USER_SETDATE_OBJ = $(USER_BUILD_DIR)/setdate.o
USER_PS_OBJ = $(USER_BUILD_DIR)/ps.o
USER_TOP_OBJ = $(USER_BUILD_DIR)/top.o
USER_NEOFETCH_OBJ = $(USER_BUILD_DIR)/neofetch.o
USER_FONT_OBJ = $(USER_BUILD_DIR)/font.o
USER_JSON_OBJ = $(USER_BUILD_DIR)/json.o
USER_IMG_OBJ = $(USER_BUILD_DIR)/img.o
USER_LOGIN_OBJ = $(USER_BUILD_DIR)/login.o
USER_VED_OBJ = $(USER_BUILD_DIR)/ved.o
USER_RAYCUBE_OBJ = $(USER_BUILD_DIR)/raycube.o
USER_INIT = $(USER_BUILD_DIR)/init.elf
USER_ELIZA = $(USER_BUILD_DIR)/eliza.elf
USER_LSH = $(USER_BUILD_DIR)/lsh.elf
USER_SH = $(USER_BUILD_DIR)/sh.elf
USER_UPTIME = $(USER_BUILD_DIR)/uptime.elf
USER_DATE = $(USER_BUILD_DIR)/date.elf
USER_SETDATE = $(USER_BUILD_DIR)/setdate.elf
USER_PS = $(USER_BUILD_DIR)/ps.elf
USER_TOP = $(USER_BUILD_DIR)/top.elf
USER_NEOFETCH = $(USER_BUILD_DIR)/neofetch.elf
USER_FONT = $(USER_BUILD_DIR)/font.elf
USER_JSON = $(USER_BUILD_DIR)/json.elf
USER_IMG = $(USER_BUILD_DIR)/img.elf
USER_LOGIN = $(USER_BUILD_DIR)/login.elf
USER_VED = $(USER_BUILD_DIR)/ved.elf
USER_RAYCUBE = $(USER_BUILD_DIR)/raycube.elf
# ne editor (userland)
USER_NE = $(USER_BUILD_DIR)/ne.elf
# Zork I (userland)
USER_ZORK_DIR = $(USER_DIR)/zork1c
USER_ZORK_C_SOURCES = $(USER_ZORK_DIR)/_parser.c $(USER_ZORK_DIR)/_game.c $(USER_ZORK_DIR)/_villains.c \
                      $(USER_ZORK_DIR)/_data.c $(USER_ZORK_DIR)/tables.c $(USER_ZORK_DIR)/mt.c $(USER_ZORK_DIR)/compress.c
USER_ZORK_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_ZORK_C_SOURCES))
USER_ZORK = $(USER_BUILD_DIR)/zork.elf

USER_LINENOISE_OBJ = $(USER_BUILD_DIR)/linenoise.o

# Minimal raylib-like API (VOS framebuffer backend)
RAYLIB_DIR = $(THIRD_PARTY_DIR)/raylib
RAYLIB_BUILD_DIR = $(USER_BUILD_DIR)/raylib
RAYLIB_OBJ = $(RAYLIB_BUILD_DIR)/raylib.o
USER_RAYLIB = $(USER_BUILD_DIR)/libraylib.a

# Tiny software 3D helper (wireframe cube)
SMALL3D_DIR = $(THIRD_PARTY_DIR)/small3dlib
SMALL3D_BUILD_DIR = $(USER_BUILD_DIR)/small3dlib
SMALL3D_OBJ = $(SMALL3D_BUILD_DIR)/small3d.o
USER_SMALL3D = $(USER_BUILD_DIR)/libsmall3d.a

# TinyCC (native compiler inside VOS)
TCC_DIR = $(THIRD_PARTY_DIR)/tcc
TCC_BUILD_DIR = $(USER_BUILD_DIR)/tcc

# Note: tcctools.c is #included by tcc.c upstream; do not compile it separately.
TCC_C_SOURCES = $(TCC_DIR)/tcc.c $(TCC_DIR)/libtcc.c $(TCC_DIR)/tccpp.c $(TCC_DIR)/tccgen.c $(TCC_DIR)/tccelf.c $(TCC_DIR)/tccasm.c $(TCC_DIR)/tccrun.c \
                $(TCC_DIR)/i386-gen.c $(TCC_DIR)/i386-link.c $(TCC_DIR)/i386-asm.c
TCC_OBJECTS = $(patsubst $(TCC_DIR)/%.c,$(TCC_BUILD_DIR)/%.o,$(TCC_C_SOURCES))
USER_TCC = $(USER_BUILD_DIR)/tcc.elf

TCC_RUNTIME_BUILD_DIR = $(USER_BUILD_DIR)/tcc_runtime
TCC_LIBTCC1_OBJ = $(TCC_RUNTIME_BUILD_DIR)/libtcc1.o
TCC_ALLOCA86_OBJ = $(TCC_RUNTIME_BUILD_DIR)/alloca86.o
TCC_ALLOCA86_BT_OBJ = $(TCC_RUNTIME_BUILD_DIR)/alloca86-bt.o
TCC_LIBTCC1_OBJS = $(TCC_LIBTCC1_OBJ) $(TCC_ALLOCA86_OBJ) $(TCC_ALLOCA86_BT_OBJ)
TCC_LIBTCC1 = $(USER_BUILD_DIR)/libtcc1.a

# ne (nice editor) - ANSI/termcap mode (no curses/terminfo)
NE_DIR = $(THIRD_PARTY_DIR)/ne
NE_SRC_DIR = $(NE_DIR)/src
NE_BUILD_DIR = $(USER_BUILD_DIR)/ne

# Build with the built-in ANSI termcap and without wide-char dependencies.
NE_C_SOURCES = \
	$(NE_SRC_DIR)/actions.c \
	$(NE_SRC_DIR)/ansi.c \
	$(NE_SRC_DIR)/autocomp.c \
	$(NE_SRC_DIR)/buffer.c \
	$(NE_SRC_DIR)/clips.c \
	$(NE_SRC_DIR)/cm.c \
	$(NE_SRC_DIR)/command.c \
	$(NE_SRC_DIR)/display.c \
	$(NE_SRC_DIR)/edit.c \
	$(NE_SRC_DIR)/errors.c \
	$(NE_SRC_DIR)/exec.c \
	$(NE_SRC_DIR)/ext.c \
	$(NE_SRC_DIR)/hash.c \
	$(NE_SRC_DIR)/help.c \
	$(NE_SRC_DIR)/input.c \
	$(NE_SRC_DIR)/inputclass.c \
	$(NE_SRC_DIR)/keys.c \
	$(NE_SRC_DIR)/menu.c \
	$(NE_SRC_DIR)/names.c \
	$(NE_SRC_DIR)/navigation.c \
	$(NE_SRC_DIR)/ne.c \
	$(NE_SRC_DIR)/prefs.c \
	$(NE_SRC_DIR)/regex.c \
	$(NE_SRC_DIR)/request.c \
	$(NE_SRC_DIR)/search.c \
	$(NE_SRC_DIR)/signals.c \
	$(NE_SRC_DIR)/streams.c \
	$(NE_SRC_DIR)/support.c \
	$(NE_SRC_DIR)/syn_hash.c \
	$(NE_SRC_DIR)/syn_regex.c \
	$(NE_SRC_DIR)/syn_utf8.c \
	$(NE_SRC_DIR)/syn_utils.c \
	$(NE_SRC_DIR)/syntax.c \
	$(NE_SRC_DIR)/term.c \
	$(NE_SRC_DIR)/termcap.c \
	$(NE_SRC_DIR)/tparam.c \
	$(NE_SRC_DIR)/info2cap.c \
	$(NE_SRC_DIR)/undo.c \
	$(NE_SRC_DIR)/utf8.c

NE_OBJECTS = $(patsubst $(NE_SRC_DIR)/%.c,$(NE_BUILD_DIR)/%.o,$(NE_C_SOURCES))

# sbase (portable Unix userland tools) - minimal subset
SBASE_DIR = $(THIRD_PARTY_DIR)/sbase
SBASE_BUILD_DIR = $(USER_BUILD_DIR)/sbase

SBASE_EPRINTF_OBJ = $(SBASE_BUILD_DIR)/libutil/eprintf.o
SBASE_EALLOC_OBJ = $(SBASE_BUILD_DIR)/libutil/ealloc.o
SBASE_FSHUT_OBJ = $(SBASE_BUILD_DIR)/libutil/fshut.o
SBASE_PUTWORD_OBJ = $(SBASE_BUILD_DIR)/libutil/putword.o
SBASE_WRITEALL_OBJ = $(SBASE_BUILD_DIR)/libutil/writeall.o
SBASE_CONCAT_OBJ = $(SBASE_BUILD_DIR)/libutil/concat.o
SBASE_STRTONUM_OBJ = $(SBASE_BUILD_DIR)/libutil/strtonum.o
SBASE_STRCASESTR_OBJ = $(SBASE_BUILD_DIR)/libutil/strcasestr.o
SBASE_EREGCOMP_OBJ = $(SBASE_BUILD_DIR)/libutil/eregcomp.o

SBASE_RUNE_OBJ = $(SBASE_BUILD_DIR)/libutf/rune.o
SBASE_UTF_OBJ = $(SBASE_BUILD_DIR)/libutf/utf.o
SBASE_RUNETYPE_OBJ = $(SBASE_BUILD_DIR)/libutf/runetype.o
SBASE_ISSPACERUNE_OBJ = $(SBASE_BUILD_DIR)/libutf/isspacerune.o
SBASE_FGETRUNE_OBJ = $(SBASE_BUILD_DIR)/libutf/fgetrune.o

SBASE_CAT_OBJ = $(SBASE_BUILD_DIR)/cat.o
SBASE_ECHO_OBJ = $(SBASE_BUILD_DIR)/echo.o
SBASE_BASENAME_OBJ = $(SBASE_BUILD_DIR)/basename.o
SBASE_DIRNAME_OBJ = $(SBASE_BUILD_DIR)/dirname.o
SBASE_HEAD_OBJ = $(SBASE_BUILD_DIR)/head.o
SBASE_WC_OBJ = $(SBASE_BUILD_DIR)/wc.o
SBASE_GREP_OBJ = $(SBASE_BUILD_DIR)/grep.o
SBASE_YES_OBJ = $(SBASE_BUILD_DIR)/yes.o
SBASE_TRUE_OBJ = $(SBASE_BUILD_DIR)/true.o
SBASE_FALSE_OBJ = $(SBASE_BUILD_DIR)/false.o

SBASE_CAT = $(USER_BUILD_DIR)/cat.elf
SBASE_ECHO = $(USER_BUILD_DIR)/echo.elf
SBASE_BASENAME = $(USER_BUILD_DIR)/basename.elf
SBASE_DIRNAME = $(USER_BUILD_DIR)/dirname.elf
SBASE_HEAD = $(USER_BUILD_DIR)/head.elf
SBASE_WC = $(USER_BUILD_DIR)/wc.elf
SBASE_GREP = $(USER_BUILD_DIR)/grep.elf
SBASE_YES = $(USER_BUILD_DIR)/yes.elf
SBASE_TRUE = $(USER_BUILD_DIR)/true.elf
SBASE_FALSE = $(USER_BUILD_DIR)/false.elf

# newlib regex (POSIX) sources (used by sbase grep)
NEWLIB_POSIX_DIR = toolchain/src/newlib-cygwin/newlib/libc/posix
NEWLIB_POSIX_BUILD_DIR = $(USER_BUILD_DIR)/newlib_posix

NEWLIB_REGEX_REGCOMP_OBJ = $(NEWLIB_POSIX_BUILD_DIR)/regcomp.o
NEWLIB_REGEX_REGEXEC_OBJ = $(NEWLIB_POSIX_BUILD_DIR)/regexec.o
NEWLIB_REGEX_REGERROR_OBJ = $(NEWLIB_POSIX_BUILD_DIR)/regerror.o
NEWLIB_REGEX_REGFREE_OBJ = $(NEWLIB_POSIX_BUILD_DIR)/regfree.o
NEWLIB_REGEX_COLLCMP_OBJ = $(NEWLIB_POSIX_BUILD_DIR)/collcmp.o

NEWLIB_REGEX_OBJS = $(NEWLIB_REGEX_REGCOMP_OBJ) $(NEWLIB_REGEX_REGEXEC_OBJ) $(NEWLIB_REGEX_REGERROR_OBJ) $(NEWLIB_REGEX_REGFREE_OBJ) $(NEWLIB_REGEX_COLLCMP_OBJ)

# BASIC (userland)
USER_BASIC_DIR = $(USER_DIR)/basic
USER_BASIC_C_SOURCES = $(USER_BASIC_DIR)/basic.c $(USER_BASIC_DIR)/ubasic.c $(USER_BASIC_DIR)/tokenizer.c $(USER_BASIC_DIR)/basic_programs.c
USER_BASIC_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_BASIC_C_SOURCES))
USER_BASIC = $(USER_BUILD_DIR)/basic.elf

USER_BINS = $(USER_INIT) $(USER_ELIZA) $(USER_LSH) $(USER_SH) $(USER_UPTIME) $(USER_DATE) $(USER_SETDATE) $(USER_PS) $(USER_TOP) $(USER_NEOFETCH) $(USER_FONT) $(USER_JSON) $(USER_IMG) $(USER_LOGIN) $(USER_VED) $(USER_RAYCUBE) $(USER_NE) $(USER_BASIC) $(USER_ZORK) $(USER_TCC) \
            $(SBASE_CAT) $(SBASE_ECHO) $(SBASE_BASENAME) $(SBASE_DIRNAME) $(SBASE_HEAD) $(SBASE_WC) $(SBASE_GREP) $(SBASE_YES) $(SBASE_TRUE) $(SBASE_FALSE)

# QEMU defaults
QEMU_XRES ?= 1920
QEMU_YRES ?= 1080

# Persistent FAT16 disk image (mounted at /disk in VOS).
DISK_IMG ?= vos-disk.img
DISK_SIZE_MB ?= 256

# Host helper to install a sysroot into $(DISK_IMG) under /usr.
SYSROOT_SCRIPT = $(TOOLS_DIR)/install_sysroot.sh

# Optional extra module: a small FAT12 image (for FAT12/16 support demo)
MKFAT = $(TOOLS_BUILD_DIR)/mkfat
FAT_IMG = $(ISO_DIR)/boot/fat.img

# Default target
all: $(ISO)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create user build directory
$(USER_BUILD_DIR): | $(BUILD_DIR)
	mkdir -p $(USER_BUILD_DIR)

# Create tools build directory
$(TOOLS_BUILD_DIR): | $(BUILD_DIR)
	mkdir -p $(TOOLS_BUILD_DIR)

# Create fonts build directory
$(FONTS_BUILD_DIR): | $(BUILD_DIR)
	mkdir -p $(FONTS_BUILD_DIR)

# Embed PSF2 fonts as binary objects
$(FONTS_BUILD_DIR)/%.o: $(FONTS_DIR)/%.psf | $(FONTS_BUILD_DIR)
	mkdir -p $(dir $@)
	$(LD) -r -b binary $< -o $@

# Build host tool to generate a FAT image
$(MKFAT): $(TOOLS_DIR)/mkfat.c | $(TOOLS_BUILD_DIR)
	gcc -O2 $< -o $@

# Generate the FAT image module
$(FAT_IMG): $(MKFAT)
	mkdir -p $(ISO_DIR)/boot
	$(MKFAT) $@

# Compile assembly
$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile userland assembly
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.asm | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile userland C
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -I$(THIRD_PARTY_DIR)/jsmn -I$(THIRD_PARTY_DIR)/sheredom_json -I$(THIRD_PARTY_DIR)/stb -I$(RAYLIB_DIR) -I$(SMALL3D_DIR) -c $< -o $@

# Vendored linenoise (userland line editing)
$(USER_LINENOISE_OBJ): $(THIRD_PARTY_DIR)/linenoise/linenoise.c | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -I$(THIRD_PARTY_DIR)/jsmn -I$(THIRD_PARTY_DIR)/sheredom_json -I$(THIRD_PARTY_DIR)/stb -c $< -o $@

# Minimal raylib backend (static library installed into /usr/lib for TCC).
$(RAYLIB_OBJ): $(RAYLIB_DIR)/raylib.c $(RAYLIB_DIR)/raylib.h | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(RAYLIB_DIR) -c $< -o $@

$(USER_RAYLIB): $(RAYLIB_OBJ) | $(USER_BUILD_DIR)
	rm -f $@
	$(AR) rcs $@ $^

# Tiny software 3D helper (static library installed into /usr/lib for TCC).
$(SMALL3D_OBJ): $(SMALL3D_DIR)/small3d.c $(SMALL3D_DIR)/small3d.h | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(SMALL3D_DIR) -c $< -o $@

$(USER_SMALL3D): $(SMALL3D_OBJ) | $(USER_BUILD_DIR)
	rm -f $@
	$(AR) rcs $@ $^

# Vendored tcc sources (userland native compiler)
$(TCC_BUILD_DIR)/%.o: $(TCC_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -DONE_SOURCE=0 -DTCC_TARGET_I386 -DCONFIG_TCC_STATIC -DCONFIG_TCCBOOT -DTCC_ON_VOS=1 -I$(USER_DIR) -I$(TCC_DIR) -c $< -o $@

$(TCC_RUNTIME_BUILD_DIR)/%.o: $(TCC_DIR)/lib/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -c $< -o $@

$(TCC_RUNTIME_BUILD_DIR)/%.o: $(TCC_DIR)/lib/%.S | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -c $< -o $@

$(TCC_LIBTCC1): $(TCC_LIBTCC1_OBJS) | $(USER_BUILD_DIR)
	rm -f $@
	$(AR) rcs $@ $^

# Build a small POSIX compatibility archive (regex, etc.) for userland ports.
$(USER_RUNTIME_LIBS): $(NEWLIB_REGEX_OBJS) $(USER_RUNTIME_C_OBJECTS) | $(USER_BUILD_DIR)
	rm -f $@
	$(AR) rcs $@ $(NEWLIB_REGEX_OBJS) $(USER_RUNTIME_C_OBJECTS)

# Link userland init (static, freestanding)
$(USER_INIT): $(USER_RUNTIME_OBJECTS) $(USER_INIT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland eliza (static, freestanding)
$(USER_ELIZA): $(USER_RUNTIME_OBJECTS) $(USER_ELIZA_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland linenoise demo
$(USER_LSH): $(USER_RUNTIME_OBJECTS) $(USER_LINENOISE_OBJ) $(USER_LSH_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland shell (linenoise)
$(USER_SH): $(USER_RUNTIME_OBJECTS) $(USER_LINENOISE_OBJ) $(USER_SH_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland login (auth + session)
$(USER_LOGIN): $(USER_RUNTIME_OBJECTS) $(USER_LOGIN_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland full-screen editor (VED)
$(USER_VED): $(USER_RUNTIME_OBJECTS) $(USER_VED_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile ne sources
$(NE_BUILD_DIR)/%.o: $(NE_SRC_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -std=c99 -fno-strict-aliasing \
		-D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -D_REGEX_LARGE_OFFSETS -DSTDC_HEADERS -DHAVE_SNPRINTF \
		-DNE_TERMCAP -DNE_ANSI -DNOWCHAR -DGLOBALDIR=\"/usr/share/ne\" \
		-I$(USER_DIR) -I$(NE_SRC_DIR) -c $< -o $@

# Link ne editor
$(USER_NE): $(USER_RUNTIME_OBJECTS) $(NE_OBJECTS) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD_MATH)

# Link userland uptime/date/setdate/ps/top (static, freestanding)
$(USER_UPTIME): $(USER_RUNTIME_OBJECTS) $(USER_UPTIME_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_DATE): $(USER_RUNTIME_OBJECTS) $(USER_DATE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_SETDATE): $(USER_RUNTIME_OBJECTS) $(USER_SETDATE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_PS): $(USER_RUNTIME_OBJECTS) $(USER_PS_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_TOP): $(USER_RUNTIME_OBJECTS) $(USER_TOP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland neofetch (static, freestanding)
$(USER_NEOFETCH): $(USER_RUNTIME_OBJECTS) $(USER_NEOFETCH_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland font selector
$(USER_FONT): $(USER_RUNTIME_OBJECTS) $(USER_FONT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland json tool (jsmn)
$(USER_JSON): $(USER_RUNTIME_OBJECTS) $(USER_JSON_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland image viewer (stb_image)
$(USER_IMG): $(USER_RUNTIME_OBJECTS) $(USER_IMG_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile sbase sources
$(SBASE_BUILD_DIR)/%.o: $(SBASE_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -c $< -o $@

# Compile newlib POSIX regex sources (for sbase grep)
$(NEWLIB_POSIX_BUILD_DIR)/%.o: $(NEWLIB_POSIX_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX2_RE_DUP_MAX=255 -I$(NEWLIB_POSIX_DIR) -c $< -o $@

# Link sbase tools (minimal subset)
$(SBASE_CAT): $(USER_RUNTIME_OBJECTS) $(SBASE_CAT_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_CONCAT_OBJ) $(SBASE_WRITEALL_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_ECHO): $(USER_RUNTIME_OBJECTS) $(SBASE_ECHO_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_PUTWORD_OBJ) $(SBASE_FSHUT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_BASENAME): $(USER_RUNTIME_OBJECTS) $(SBASE_BASENAME_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_FSHUT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_DIRNAME): $(USER_RUNTIME_OBJECTS) $(SBASE_DIRNAME_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_FSHUT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_HEAD): $(USER_RUNTIME_OBJECTS) $(SBASE_HEAD_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_FSHUT_OBJ) $(SBASE_STRTONUM_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_WC): $(USER_RUNTIME_OBJECTS) $(SBASE_WC_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_FSHUT_OBJ) $(SBASE_RUNE_OBJ) $(SBASE_UTF_OBJ) $(SBASE_RUNETYPE_OBJ) $(SBASE_ISSPACERUNE_OBJ) $(SBASE_FGETRUNE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_GREP): $(USER_RUNTIME_OBJECTS) $(SBASE_GREP_OBJ) $(SBASE_EPRINTF_OBJ) $(SBASE_FSHUT_OBJ) $(SBASE_EALLOC_OBJ) $(SBASE_STRCASESTR_OBJ) $(SBASE_EREGCOMP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_YES): $(USER_RUNTIME_OBJECTS) $(SBASE_YES_OBJ) $(SBASE_EPRINTF_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_TRUE): $(USER_RUNTIME_OBJECTS) $(SBASE_TRUE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(SBASE_FALSE): $(USER_RUNTIME_OBJECTS) $(SBASE_FALSE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland BASIC interpreter
$(USER_BASIC): $(USER_RUNTIME_OBJECTS) $(USER_BASIC_OBJECTS) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland zork (static, freestanding)
$(USER_ZORK): $(USER_RUNTIME_OBJECTS) $(USER_ZORK_OBJECTS) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland tcc (native compiler)
$(USER_TCC): $(USER_RUNTIME_OBJECTS) $(TCC_OBJECTS) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link raycube demo (raylib + small3d)
$(USER_RAYCUBE): $(USER_RUNTIME_OBJECTS) $(USER_RAYCUBE_OBJ) $(USER_RAYLIB) $(USER_SMALL3D) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD_MATH)

# Compile C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Vendored microrl (readline-style line editing)
$(MICRORL_OBJ): $(THIRD_PARTY_DIR)/microrl/microrl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link kernel
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# Create bootable ISO
$(ISO): $(KERNEL) $(USER_BINS) $(FAT_IMG) $(INITRAMFS_FILES) $(INITRAMFS_DIRS)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)
	if [ -d "$(INITRAMFS_DIR)" ]; then cp -r $(INITRAMFS_DIR)/* $(INITRAMFS_ROOT)/ 2>/dev/null || true ; fi
	mkdir -p $(INITRAMFS_ROOT)/bin
	cp $(USER_INIT) $(INITRAMFS_ROOT)/bin/init
	cp $(USER_LOGIN) $(INITRAMFS_ROOT)/bin/login
	cp $(USER_ELIZA) $(INITRAMFS_ROOT)/bin/eliza
	cp $(USER_LSH) $(INITRAMFS_ROOT)/bin/lsh
	cp $(USER_SH) $(INITRAMFS_ROOT)/bin/sh
	cp $(USER_VED) $(INITRAMFS_ROOT)/bin/ved
	cp $(USER_NE) $(INITRAMFS_ROOT)/bin/ne
	cp $(USER_UPTIME) $(INITRAMFS_ROOT)/bin/uptime
	cp $(USER_DATE) $(INITRAMFS_ROOT)/bin/date
	cp $(USER_SETDATE) $(INITRAMFS_ROOT)/bin/setdate
	cp $(USER_PS) $(INITRAMFS_ROOT)/bin/ps
	cp $(USER_TOP) $(INITRAMFS_ROOT)/bin/top
	cp $(USER_NEOFETCH) $(INITRAMFS_ROOT)/bin/neofetch
	cp $(USER_FONT) $(INITRAMFS_ROOT)/bin/font
	cp $(USER_JSON) $(INITRAMFS_ROOT)/bin/json
	cp $(USER_IMG) $(INITRAMFS_ROOT)/bin/img
	cp $(USER_RAYCUBE) $(INITRAMFS_ROOT)/bin/raycube
	cp $(USER_BASIC) $(INITRAMFS_ROOT)/bin/basic
	cp $(USER_ZORK) $(INITRAMFS_ROOT)/bin/zork
	cp $(USER_TCC) $(INITRAMFS_ROOT)/bin/tcc
	cp $(SBASE_CAT) $(INITRAMFS_ROOT)/bin/cat
	cp $(SBASE_ECHO) $(INITRAMFS_ROOT)/bin/echo
	cp $(SBASE_BASENAME) $(INITRAMFS_ROOT)/bin/basename
	cp $(SBASE_DIRNAME) $(INITRAMFS_ROOT)/bin/dirname
	cp $(SBASE_HEAD) $(INITRAMFS_ROOT)/bin/head
	cp $(SBASE_WC) $(INITRAMFS_ROOT)/bin/wc
	cp $(SBASE_GREP) $(INITRAMFS_ROOT)/bin/grep
	cp $(SBASE_YES) $(INITRAMFS_ROOT)/bin/yes
	cp $(SBASE_TRUE) $(INITRAMFS_ROOT)/bin/true
	cp $(SBASE_FALSE) $(INITRAMFS_ROOT)/bin/false
	tar -C $(INITRAMFS_ROOT) -cf $(INITRAMFS_TAR) .
	echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod all_video' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod gfxterm' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod font' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'loadfont /boot/grub/fonts/unicode.pf2' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "set gfxmode=$(QEMU_XRES)x$(QEMU_YRES),1920x1080,1600x900,1366x768,1280x800,1280x720,1024x768,800x600,640x480,auto" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set gfxpayload=keep' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'terminal_output gfxterm' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "VOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    module /boot/initramfs.tar' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    module /boot/fat.img' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

# Create a persistent FAT16 disk image for /disk (only when missing).
$(DISK_IMG):
	truncate -s $(DISK_SIZE_MB)M $@
	mkfs.fat -F 16 -n VOSDISK $@

# Install a sysroot onto $(DISK_IMG) so /usr is populated inside VOS.
sysroot: $(USER_RUNTIME_OBJECTS) $(USER_RUNTIME_LIBS) $(USER_CRTI_OBJ) $(USER_CRTN_OBJ) $(TCC_LIBTCC1) $(USER_TCC) $(USER_RAYLIB) $(USER_SMALL3D) $(DISK_IMG)
	bash $(SYSROOT_SCRIPT) $(DISK_IMG)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_TAR)
	rm -rf $(ISO_DIR)/boot/grub/grub.cfg
	rm -rf $(FAT_IMG)
	rm -f $(ISO)

# Run in QEMU (for quick testing)
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES)

# Run in QEMU with debug output
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES) -d int -no-reboot

.PHONY: all clean run debug sysroot
