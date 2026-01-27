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
	$(FONTS_DIR)/terminus/Uni3-TerminusBold32x16.psf \
	$(FONTS_DIR)/tamzen/Tamzen10x20.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v16b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v18b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v20b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v20n.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v22b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v24b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v28b.psf \
	$(FONTS_DIR)/terminus-powerline/ter-powerline-v32b.psf \
	$(FONTS_DIR)/gohufont/gohufont-uni-11.psf \
	$(FONTS_DIR)/gohufont/gohufont-uni-11b.psf \
	$(FONTS_DIR)/unifont/Uni1-VGA28x16.psf \
	$(FONTS_DIR)/unifont/Uni1-VGA32x16.psf \
	$(FONTS_DIR)/unifont/Uni2-Terminus20x10.psf \
	$(FONTS_DIR)/unifont/Uni2-Terminus24x12.psf \
	$(FONTS_DIR)/unifont/Uni2-Terminus28x14.psf \
	$(FONTS_DIR)/unifont/Uni2-Terminus32x16.psf \
	$(FONTS_DIR)/unifont/Uni2-TerminusBold20x10.psf \
	$(FONTS_DIR)/unifont/Uni2-TerminusBold24x12.psf \
	$(FONTS_DIR)/unifont/Uni2-TerminusBold28x14.psf \
	$(FONTS_DIR)/unifont/Uni2-TerminusBold32x16.psf \
	$(FONTS_DIR)/unifont/Uni2-VGA28x16.psf \
	$(FONTS_DIR)/unifont/Uni2-VGA32x16.psf \
	$(FONTS_DIR)/unifont/Uni3-Terminus20x10.psf \
	$(FONTS_DIR)/unifont/Uni3-Terminus24x12.psf \
	$(FONTS_DIR)/unifont/Uni3-Terminus28x14.psf \
	$(FONTS_DIR)/unifont/Uni3-Terminus32x16.psf \
	$(FONTS_DIR)/unifont/Uni3-TerminusBold20x10.psf \
	$(FONTS_DIR)/unifont/Uni3-TerminusBold24x12.psf \
	$(FONTS_DIR)/unifont/Uni3-TerminusBold28x14.psf \
	$(FONTS_DIR)/unifont/Uni3-TerminusBold32x16.psf \
	$(FONTS_DIR)/cozette/cozette.psf \
	$(FONTS_DIR)/cozette/cozette_hidpi.psf

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
USER_DF_OBJ = $(USER_BUILD_DIR)/df.o
USER_TREE_OBJ = $(USER_BUILD_DIR)/tree.o
USER_UPTIME_OBJ = $(USER_BUILD_DIR)/uptime.o
USER_SETDATE_OBJ = $(USER_BUILD_DIR)/setdate.o
USER_PS_OBJ = $(USER_BUILD_DIR)/ps.o
USER_TOP_OBJ = $(USER_BUILD_DIR)/top.o
USER_SYSVIEW_OBJ = $(USER_BUILD_DIR)/sysview.o
USER_NEOFETCH_OBJ = $(USER_BUILD_DIR)/neofetch.o
USER_FONT_OBJ = $(USER_BUILD_DIR)/font.o
USER_THEME_OBJ = $(USER_BUILD_DIR)/theme.o
USER_LS_OBJ = $(USER_BUILD_DIR)/ls.o
USER_JSON_OBJ = $(USER_BUILD_DIR)/json.o
USER_IMG_OBJ = $(USER_BUILD_DIR)/img.o
USER_LOGIN_OBJ = $(USER_BUILD_DIR)/login.o
USER_S3LCUBE_OBJ = $(USER_BUILD_DIR)/s3lcube.o
USER_S3LFLY_OBJ = $(USER_BUILD_DIR)/s3lfly.o
USER_OLIVEDEMO_OBJ = $(USER_BUILD_DIR)/olivedemo.o
USER_NEXTVI_OBJ = $(USER_BUILD_DIR)/nextvi/vi.o
USER_MD4C_OBJ = $(USER_BUILD_DIR)/md4c.o
USER_MDVIEW_OBJ = $(USER_BUILD_DIR)/mdview.o
USER_ZIP_OBJ = $(USER_BUILD_DIR)/zip.o
USER_UNZIP_OBJ = $(USER_BUILD_DIR)/unzip.o
USER_GZIP_OBJ = $(USER_BUILD_DIR)/gzip.o
USER_BEEP_OBJ = $(USER_BUILD_DIR)/beep.o
USER_MODPLAY_OBJ = $(USER_BUILD_DIR)/modplay.o
USER_MIDIPLAY_OBJ = $(USER_BUILD_DIR)/midiplay.o
USER_CHOWN_OBJ = $(USER_BUILD_DIR)/chown.o
USER_USERADD_OBJ = $(USER_BUILD_DIR)/useradd.o
USER_USERDEL_OBJ = $(USER_BUILD_DIR)/userdel.o
USER_GROUPADD_OBJ = $(USER_BUILD_DIR)/groupadd.o
USER_GROUPDEL_OBJ = $(USER_BUILD_DIR)/groupdel.o
USER_INIT = $(USER_BUILD_DIR)/init.elf
USER_ELIZA = $(USER_BUILD_DIR)/eliza.elf
USER_DF = $(USER_BUILD_DIR)/df.elf
USER_TREE = $(USER_BUILD_DIR)/tree.elf
USER_UPTIME = $(USER_BUILD_DIR)/uptime.elf
USER_SETDATE = $(USER_BUILD_DIR)/setdate.elf
USER_PS = $(USER_BUILD_DIR)/ps.elf
USER_TOP = $(USER_BUILD_DIR)/top.elf
USER_SYSVIEW = $(USER_BUILD_DIR)/sysview.elf
USER_NEOFETCH = $(USER_BUILD_DIR)/neofetch.elf
USER_FONT = $(USER_BUILD_DIR)/font.elf
USER_THEME = $(USER_BUILD_DIR)/theme.elf
USER_LS = $(USER_BUILD_DIR)/ls.elf
USER_JSON = $(USER_BUILD_DIR)/json.elf
USER_IMG = $(USER_BUILD_DIR)/img.elf
USER_LOGIN = $(USER_BUILD_DIR)/login.elf
USER_S3LCUBE = $(USER_BUILD_DIR)/s3lcube.elf
USER_S3LFLY = $(USER_BUILD_DIR)/s3lfly.elf
USER_OLIVEDEMO = $(USER_BUILD_DIR)/olivedemo.elf
USER_NEXTVI = $(USER_BUILD_DIR)/nextvi.elf
USER_MDVIEW = $(USER_BUILD_DIR)/mdview.elf
USER_ZIP = $(USER_BUILD_DIR)/zip.elf
USER_UNZIP = $(USER_BUILD_DIR)/unzip.elf
USER_GZIP = $(USER_BUILD_DIR)/gzip.elf
USER_BEEP = $(USER_BUILD_DIR)/beep.elf
USER_MODPLAY = $(USER_BUILD_DIR)/modplay.elf
USER_MIDIPLAY = $(USER_BUILD_DIR)/midiplay.elf
USER_CHOWN = $(USER_BUILD_DIR)/chown.elf
USER_USERADD = $(USER_BUILD_DIR)/useradd.elf
USER_USERDEL = $(USER_BUILD_DIR)/userdel.elf
USER_GROUPADD = $(USER_BUILD_DIR)/groupadd.elf
USER_GROUPDEL = $(USER_BUILD_DIR)/groupdel.elf
# pocketmod and TinySoundFont directories
POCKETMOD_DIR = $(THIRD_PARTY_DIR)/pocketmod
TSF_DIR = $(THIRD_PARTY_DIR)/tsf

# SDL2 shim library and test program
USER_SDL2_DIR = $(USER_DIR)/sdl2
USER_SDL2_LIB = $(USER_SDL2_DIR)/libvos-sdl2.a
USER_SDLTEST_OBJ = $(USER_BUILD_DIR)/sdltest.o
USER_SDLTEST = $(USER_BUILD_DIR)/sdltest.elf
# ne editor (userland)
# Zork I (userland)
USER_ZORK_DIR = $(USER_DIR)/zork1c
USER_ZORK_C_SOURCES = $(USER_ZORK_DIR)/_parser.c $(USER_ZORK_DIR)/_game.c $(USER_ZORK_DIR)/_villains.c \
                      $(USER_ZORK_DIR)/_data.c $(USER_ZORK_DIR)/tables.c $(USER_ZORK_DIR)/mt.c $(USER_ZORK_DIR)/compress.c
USER_ZORK_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_ZORK_C_SOURCES))
USER_ZORK = $(USER_BUILD_DIR)/zork.elf

USER_LINENOISE_OBJ = $(USER_BUILD_DIR)/linenoise.o

# small3dlib (upstream single-header software 3D renderer)
SMALL3D_DIR = $(THIRD_PARTY_DIR)/small3dlib

# olive.c (single-file 2D software renderer; installed into /usr/include)
OLIVE_DIR = $(THIRD_PARTY_DIR)/olive
OLIVE_BUILD_DIR = $(USER_BUILD_DIR)/olive
OLIVE_OBJ = $(OLIVE_BUILD_DIR)/olive.o
USER_OLIVE = $(USER_BUILD_DIR)/libolive.a

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

# sbase (portable Unix userland tools)
SBASE_DIR = $(THIRD_PARTY_DIR)/sbase
SBASE_BUILD_DIR = $(USER_BUILD_DIR)/sbase
SBASE_BIN_DIR = $(USER_BUILD_DIR)/sbase_bin

SBASE_LIBUTIL_C_SOURCES = $(wildcard $(SBASE_DIR)/libutil/*.c)
SBASE_LIBUTIL_OBJECTS = $(patsubst $(SBASE_DIR)/%.c,$(SBASE_BUILD_DIR)/%.o,$(SBASE_LIBUTIL_C_SOURCES))
SBASE_LIBUTIL = $(SBASE_BUILD_DIR)/libsbaseutil.a

SBASE_LIBUTF_C_SOURCES = $(wildcard $(SBASE_DIR)/libutf/*.c)
SBASE_LIBUTF_OBJECTS = $(patsubst $(SBASE_DIR)/%.c,$(SBASE_BUILD_DIR)/%.o,$(SBASE_LIBUTF_C_SOURCES))
SBASE_LIBUTF = $(SBASE_BUILD_DIR)/libsbaseutf.a

SBASE_LIBS = $(SBASE_LIBUTIL) $(SBASE_LIBUTF)

# Keep this list focused on tools that don't require fork/exec.
SBASE_TOOLS = \
	cat echo basename dirname head wc grep yes true false \
	pwd cp mv rm mkdir rmdir tail cut tr sort uniq tee nl seq rev \
	strings od printf sed du chmod ln \
	cksum md5sum sha1sum sha256sum sha512sum \
	cmp comm cols dd printenv expr fold join paste split sponge \
	test tsort unexpand expand \
	cal date sleep uname whoami logname which kill \
	find xargs time env mktemp

SBASE_TOOL_OBJECTS = $(patsubst %,$(SBASE_BUILD_DIR)/%.o,$(SBASE_TOOLS))
SBASE_TOOL_BINS = $(patsubst %,$(SBASE_BIN_DIR)/%.elf,$(SBASE_TOOLS))

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

# Peanut-GB (Game Boy emulator - single header library)
PEANUT_GB_DIR = $(THIRD_PARTY_DIR)/peanut-gb
USER_GBEMU_OBJ = $(USER_BUILD_DIR)/gbemu.o
USER_GBEMU = $(USER_BUILD_DIR)/gbemu.elf

# Nofrendo (NES emulator)
NOFRENDO_DIR = $(THIRD_PARTY_DIR)/nofrendo
NOFRENDO_BUILD_DIR = $(USER_BUILD_DIR)/nofrendo
NOFRENDO_NES_C_SOURCES = $(NOFRENDO_DIR)/nes/apu.c $(NOFRENDO_DIR)/nes/cpu.c $(NOFRENDO_DIR)/nes/dis.c \
                         $(NOFRENDO_DIR)/nes/input.c $(NOFRENDO_DIR)/nes/mem.c $(NOFRENDO_DIR)/nes/mmc.c \
                         $(NOFRENDO_DIR)/nes/nes.c $(NOFRENDO_DIR)/nes/ppu.c $(NOFRENDO_DIR)/nes/rom.c \
                         $(NOFRENDO_DIR)/nes/state.c
NOFRENDO_MAPPER_C_SOURCES = $(wildcard $(NOFRENDO_DIR)/mappers/*.c)
NOFRENDO_C_SOURCES = $(NOFRENDO_NES_C_SOURCES) $(NOFRENDO_MAPPER_C_SOURCES)
NOFRENDO_OBJECTS = $(patsubst $(NOFRENDO_DIR)/%.c,$(NOFRENDO_BUILD_DIR)/%.o,$(NOFRENDO_C_SOURCES))
USER_NESEMU_OBJ = $(USER_BUILD_DIR)/nesemu.o
USER_NESEMU = $(USER_BUILD_DIR)/nesemu.elf

# dash (Debian Almquist Shell - POSIX shell)
DASH_DIR = $(THIRD_PARTY_DIR)/dash
DASH_BUILD_DIR = $(USER_BUILD_DIR)/dash

DASH_C_SOURCES = \
	$(DASH_DIR)/alias.c \
	$(DASH_DIR)/arith_yacc.c \
	$(DASH_DIR)/arith_yylex.c \
	$(DASH_DIR)/builtins.c \
	$(DASH_DIR)/cd.c \
	$(DASH_DIR)/error.c \
	$(DASH_DIR)/eval.c \
	$(DASH_DIR)/exec.c \
	$(DASH_DIR)/expand.c \
	$(DASH_DIR)/histedit.c \
	$(DASH_DIR)/init.c \
	$(DASH_DIR)/input.c \
	$(DASH_DIR)/jobs.c \
	$(DASH_DIR)/mail.c \
	$(DASH_DIR)/main.c \
	$(DASH_DIR)/memalloc.c \
	$(DASH_DIR)/miscbltin.c \
	$(DASH_DIR)/mystring.c \
	$(DASH_DIR)/nodes.c \
	$(DASH_DIR)/options.c \
	$(DASH_DIR)/output.c \
	$(DASH_DIR)/parser.c \
	$(DASH_DIR)/redir.c \
	$(DASH_DIR)/show.c \
	$(DASH_DIR)/signames.c \
	$(DASH_DIR)/syntax.c \
	$(DASH_DIR)/system.c \
	$(DASH_DIR)/trap.c \
	$(DASH_DIR)/var.c \
	$(DASH_DIR)/bltin/printf.c \
	$(DASH_DIR)/bltin/test.c \
	$(DASH_DIR)/bltin/times.c \
	$(DASH_DIR)/vos_editline.c

DASH_OBJECTS = $(patsubst $(DASH_DIR)/%.c,$(DASH_BUILD_DIR)/%.o,$(DASH_C_SOURCES))
USER_DASH = $(USER_BUILD_DIR)/dash.elf

# Klystrack chiptune tracker (built with separate Makefile.vos)
USER_KLYSTRACK_DIR = $(THIRD_PARTY_DIR)/klystrack
USER_KLYSTRACK = $(USER_KLYSTRACK_DIR)/bin.vos/klystrack

USER_BINS = $(USER_INIT) $(USER_ELIZA) $(USER_DF) $(USER_TREE) $(USER_UPTIME) $(USER_SETDATE) $(USER_PS) $(USER_TOP) $(USER_SYSVIEW) $(USER_NEOFETCH) $(USER_FONT) $(USER_THEME) $(USER_LS) $(USER_JSON) $(USER_IMG) $(USER_LOGIN) $(USER_S3LCUBE) $(USER_S3LFLY) $(USER_OLIVEDEMO) $(USER_NEXTVI) $(USER_MDVIEW) $(USER_BASIC) $(USER_ZORK) $(USER_TCC) $(USER_GBEMU) $(USER_NESEMU) $(USER_DASH) $(USER_ZIP) $(USER_UNZIP) $(USER_GZIP) $(USER_BEEP) $(USER_MODPLAY) $(USER_MIDIPLAY) $(USER_CHOWN) \
            $(USER_USERADD) $(USER_USERDEL) $(USER_GROUPADD) $(USER_GROUPDEL) \
            $(USER_SDLTEST) $(USER_KLYSTRACK) \
            $(SBASE_TOOL_BINS)

# QEMU defaults
QEMU_XRES ?= 1920
QEMU_YRES ?= 1080

# Persistent Minix disk image (mounted at /disk in VOS).
DISK_IMG ?= vos-disk.img
DISK_SIZE_MB ?= 4096

# Host helper to install a sysroot into $(DISK_IMG) under /usr.
SYSROOT_SCRIPT = $(TOOLS_DIR)/install_sysroot.sh

# Optional extra module: a small FAT16 image (disabled - using MinixFS for disk)
# To re-enable: uncomment these and add $(FAT_IMG) to $(ISO) dependencies
# MKFAT = $(TOOLS_BUILD_DIR)/mkfat
# FAT_IMG = $(ISO_DIR)/boot/fat.img

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

# Build host tool to generate a FAT image (disabled - using MinixFS)
# $(MKFAT): $(TOOLS_DIR)/mkfat.c | $(TOOLS_BUILD_DIR)
# 	gcc -O2 $< -o $@

# Generate the FAT image module (disabled - using MinixFS)
# $(FAT_IMG): $(MKFAT)
# 	mkdir -p $(ISO_DIR)/boot
# 	$(MKFAT) $@

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
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -I$(THIRD_PARTY_DIR)/jsmn -I$(THIRD_PARTY_DIR)/sheredom_json -I$(THIRD_PARTY_DIR)/stb -I$(SMALL3D_DIR) -I$(OLIVE_DIR) -c $< -o $@

# Vendored linenoise (userland line editing)
$(USER_LINENOISE_OBJ): $(THIRD_PARTY_DIR)/linenoise/linenoise.c | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -I$(THIRD_PARTY_DIR)/jsmn -I$(THIRD_PARTY_DIR)/sheredom_json -I$(THIRD_PARTY_DIR)/stb -c $< -o $@

# olive.c renderer (static library installed into /usr/lib for TCC).
$(OLIVE_OBJ): $(OLIVE_DIR)/olive.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -DOLIVEC_IMPLEMENTATION -DOLIVECDEF= -I$(OLIVE_DIR) -c $< -o $@

$(USER_OLIVE): $(OLIVE_OBJ) | $(USER_BUILD_DIR)
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

# Link userland df (filesystem stats)
$(USER_DF): $(USER_RUNTIME_OBJECTS) $(USER_DF_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland tree
$(USER_TREE): $(USER_RUNTIME_OBJECTS) $(USER_TREE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland login (auth + session)
$(USER_LOGIN): $(USER_RUNTIME_OBJECTS) $(USER_LOGIN_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile dash sources
$(DASH_BUILD_DIR)/%.o: $(DASH_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wno-unused-parameter -Wno-sign-compare -O2 \
		-DHAVE_CONFIG_H -DSHELL -include $(DASH_DIR)/config.h \
		-I$(USER_DIR) -I$(DASH_DIR) -I$(THIRD_PARTY_DIR)/linenoise -c $< -o $@

# Link dash shell (includes linenoise for history/editing)
$(USER_DASH): $(USER_RUNTIME_OBJECTS) $(DASH_OBJECTS) $(USER_LINENOISE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland uptime/setdate/ps/top (static, freestanding)
$(USER_UPTIME): $(USER_RUNTIME_OBJECTS) $(USER_UPTIME_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_SETDATE): $(USER_RUNTIME_OBJECTS) $(USER_SETDATE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_PS): $(USER_RUNTIME_OBJECTS) $(USER_PS_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_TOP): $(USER_RUNTIME_OBJECTS) $(USER_TOP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_SYSVIEW): $(USER_RUNTIME_OBJECTS) $(USER_SYSVIEW_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland neofetch (static, freestanding)
$(USER_NEOFETCH): $(USER_RUNTIME_OBJECTS) $(USER_NEOFETCH_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland font selector
$(USER_FONT): $(USER_RUNTIME_OBJECTS) $(USER_FONT_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland theme selector
$(USER_THEME): $(USER_RUNTIME_OBJECTS) $(USER_THEME_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland colorized ls
$(USER_LS): $(USER_RUNTIME_OBJECTS) $(USER_LS_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland json tool (jsmn)
$(USER_JSON): $(USER_RUNTIME_OBJECTS) $(USER_JSON_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland image viewer (stb_image)
$(USER_IMG): $(USER_RUNTIME_OBJECTS) $(USER_IMG_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland zip (miniz)
$(USER_ZIP): $(USER_RUNTIME_OBJECTS) $(USER_ZIP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland unzip (miniz)
$(USER_UNZIP): $(USER_RUNTIME_OBJECTS) $(USER_UNZIP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland gzip (miniz)
$(USER_GZIP): $(USER_RUNTIME_OBJECTS) $(USER_GZIP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link userland beep (PC speaker)
$(USER_BEEP): $(USER_RUNTIME_OBJECTS) $(USER_BEEP_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile modplay (uses pocketmod for MOD playback)
$(USER_MODPLAY_OBJ): $(USER_DIR)/modplay.c $(POCKETMOD_DIR)/pocketmod.h | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(USER_DIR) -I$(THIRD_PARTY_DIR) -c $< -o $@

# Link modplay (needs math library for pocketmod)
$(USER_MODPLAY): $(USER_RUNTIME_OBJECTS) $(USER_MODPLAY_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD_MATH)

# Compile midiplay (uses TinySoundFont for MIDI/SF2 playback)
$(USER_MIDIPLAY_OBJ): $(USER_DIR)/midiplay.c $(TSF_DIR)/tsf.h $(TSF_DIR)/tml.h | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(USER_DIR) -I$(THIRD_PARTY_DIR) -c $< -o $@

# Link midiplay (needs math library for TinySoundFont)
$(USER_MIDIPLAY): $(USER_RUNTIME_OBJECTS) $(USER_MIDIPLAY_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD_MATH)

# Link chown
$(USER_CHOWN): $(USER_RUNTIME_OBJECTS) $(USER_CHOWN_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link user management tools
$(USER_USERADD): $(USER_RUNTIME_OBJECTS) $(USER_USERADD_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_USERDEL): $(USER_RUNTIME_OBJECTS) $(USER_USERDEL_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_GROUPADD): $(USER_RUNTIME_OBJECTS) $(USER_GROUPADD_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(USER_GROUPDEL): $(USER_RUNTIME_OBJECTS) $(USER_GROUPDEL_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile sbase sources
$(SBASE_BUILD_DIR)/%.o: $(SBASE_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -DPATH_MAX=256 -DNAME_MAX=255 -DSSIZE_MAX=2147483647 -I$(USER_DIR) -c $< -o $@

# Compile newlib POSIX regex sources (for sbase grep)
$(NEWLIB_POSIX_BUILD_DIR)/%.o: $(NEWLIB_POSIX_DIR)/%.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX2_RE_DUP_MAX=255 -I$(NEWLIB_POSIX_DIR) -c $< -o $@

# Archive sbase support libraries (libutil + libutf).
$(SBASE_LIBUTIL): $(SBASE_LIBUTIL_OBJECTS) | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(SBASE_LIBUTF): $(SBASE_LIBUTF_OBJECTS) | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# Link sbase tools.
$(SBASE_TOOL_BINS): $(SBASE_BIN_DIR)/%.elf: $(USER_RUNTIME_OBJECTS) $(SBASE_BUILD_DIR)/%.o $(SBASE_LIBS) $(USER_RUNTIME_LIBS)
	mkdir -p $(dir $@)
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

# Link small3dlib cube demo (full upstream header)
$(USER_S3LCUBE): $(USER_RUNTIME_OBJECTS) $(USER_S3LCUBE_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link small3dlib flying game
$(USER_S3LFLY): $(USER_RUNTIME_OBJECTS) $(USER_S3LFLY_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Link olive.c demo (upstream single-file renderer as a static lib)
$(USER_OLIVEDEMO): $(USER_RUNTIME_OBJECTS) $(USER_OLIVEDEMO_OBJ) $(USER_OLIVE) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile nextvi (vi clone - unity build where vi.c #includes other .c files)
$(USER_NEXTVI_OBJ): $(USER_DIR)/nextvi/vi.c | $(USER_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(USER_DIR)/nextvi -c $< -o $@

# Link nextvi
$(USER_NEXTVI): $(USER_RUNTIME_OBJECTS) $(USER_NEXTVI_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile MD4C library (single-file markdown parser)
$(USER_MD4C_OBJ): $(THIRD_PARTY_DIR)/md4c/md4c.c $(THIRD_PARTY_DIR)/md4c/md4c.h | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -Wno-unused-parameter -O2 -I$(THIRD_PARTY_DIR)/md4c -c $< -o $@

# Compile mdview (terminal markdown viewer)
$(USER_MDVIEW_OBJ): $(USER_DIR)/mdview.c $(THIRD_PARTY_DIR)/md4c/md4c.h | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/md4c -c $< -o $@

# Link mdview
$(USER_MDVIEW): $(USER_RUNTIME_OBJECTS) $(USER_MD4C_OBJ) $(USER_MDVIEW_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile Game Boy emulator (uses Peanut-GB single-header library)
$(USER_GBEMU_OBJ): $(USER_DIR)/gbemu.c $(PEANUT_GB_DIR)/peanut_gb.h | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(THIRD_PARTY_DIR) -I$(USER_DIR) -c $< -o $@

# Link Game Boy emulator
$(USER_GBEMU): $(USER_RUNTIME_OBJECTS) $(USER_GBEMU_OBJ) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

# Compile nofrendo NES core files
$(NOFRENDO_BUILD_DIR)/nes/%.o: $(NOFRENDO_DIR)/nes/%.c | $(NOFRENDO_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -Wno-unused-parameter -O2 -I$(NOFRENDO_DIR) -c $< -o $@

# Compile nofrendo mappers
$(NOFRENDO_BUILD_DIR)/mappers/%.o: $(NOFRENDO_DIR)/mappers/%.c | $(NOFRENDO_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -Wno-unused-parameter -O2 -I$(NOFRENDO_DIR) -c $< -o $@

# Compile NES emulator frontend
$(USER_NESEMU_OBJ): $(USER_DIR)/nesemu.c | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -Wno-unused-parameter -O2 -I$(THIRD_PARTY_DIR) -I$(NOFRENDO_DIR) -I$(USER_DIR) -c $< -o $@

# Link NES emulator
$(USER_NESEMU): $(USER_RUNTIME_OBJECTS) $(USER_NESEMU_OBJ) $(NOFRENDO_OBJECTS) $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)

$(NOFRENDO_BUILD_DIR):
	mkdir -p $(NOFRENDO_BUILD_DIR)/nes $(NOFRENDO_BUILD_DIR)/mappers

# Build SDL2 shim library (call make in user/sdl2/)
$(USER_SDL2_LIB):
	$(MAKE) -C $(USER_SDL2_DIR)

# Build klystrack chiptune tracker (uses separate Makefile.vos)
$(USER_KLYSTRACK): $(USER_SDL2_LIB) $(USER_RUNTIME_LIBS) $(USER_RUNTIME_OBJECTS)
	$(MAKE) -C $(USER_KLYSTRACK_DIR) -f Makefile.vos

# Compile SDL2 test program
$(USER_SDLTEST_OBJ): $(USER_SDL2_DIR)/test_sdl.c $(USER_SDL2_LIB) | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(USER_DIR) -I$(USER_SDL2_DIR)/include -c $< -o $@

# Link SDL2 test program (SDL2 library must come before other libs)
$(USER_SDLTEST): $(USER_RUNTIME_OBJECTS) $(USER_SDLTEST_OBJ) $(USER_SDL2_LIB) $(USER_RUNTIME_LIBS)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $(USER_RUNTIME_OBJECTS) $(USER_SDLTEST_OBJ) $(USER_SDL2_LIB) $(USER_RUNTIME_LIBS) -lc -lgcc

# Compile C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Vendored microrl (readline-style line editing)
$(MICRORL_OBJ): $(THIRD_PARTY_DIR)/microrl/microrl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link kernel
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# Create bootable ISO (includes TCC sysroot in initramfs)
$(ISO): $(KERNEL) $(USER_BINS) $(INITRAMFS_FILES) $(INITRAMFS_DIRS) $(TCC_LIBTCC1) $(USER_CRTI_OBJ) $(USER_CRTN_OBJ)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)
	if [ -d "$(INITRAMFS_DIR)" ]; then cp -r $(INITRAMFS_DIR)/* $(INITRAMFS_ROOT)/ 2>/dev/null || true ; fi
	mkdir -p $(INITRAMFS_ROOT)/bin
	cp $(USER_INIT) $(INITRAMFS_ROOT)/bin/init
	cp $(USER_LOGIN) $(INITRAMFS_ROOT)/bin/login
	cp $(USER_ELIZA) $(INITRAMFS_ROOT)/bin/eliza
	cp $(USER_DF) $(INITRAMFS_ROOT)/bin/df
	cp $(USER_TREE) $(INITRAMFS_ROOT)/bin/tree
	cp $(USER_UPTIME) $(INITRAMFS_ROOT)/bin/uptime
	cp $(USER_SETDATE) $(INITRAMFS_ROOT)/bin/setdate
	cp $(USER_PS) $(INITRAMFS_ROOT)/bin/ps
	cp $(USER_TOP) $(INITRAMFS_ROOT)/bin/top
	cp $(USER_SYSVIEW) $(INITRAMFS_ROOT)/bin/sysview
	cp $(USER_NEOFETCH) $(INITRAMFS_ROOT)/bin/neofetch
	cp $(USER_FONT) $(INITRAMFS_ROOT)/bin/font
	cp $(USER_THEME) $(INITRAMFS_ROOT)/bin/theme
	cp $(USER_LS) $(INITRAMFS_ROOT)/bin/ls
	cp $(USER_JSON) $(INITRAMFS_ROOT)/bin/json
	cp $(USER_IMG) $(INITRAMFS_ROOT)/bin/img
	cp $(USER_S3LCUBE) $(INITRAMFS_ROOT)/bin/s3lcube
	cp $(USER_S3LFLY) $(INITRAMFS_ROOT)/bin/s3lfly
	cp $(USER_OLIVEDEMO) $(INITRAMFS_ROOT)/bin/olivedemo
	cp $(USER_NEXTVI) $(INITRAMFS_ROOT)/bin/vi
	cp $(USER_MDVIEW) $(INITRAMFS_ROOT)/bin/mdview
	cp $(USER_BASIC) $(INITRAMFS_ROOT)/bin/basic
	cp $(USER_ZORK) $(INITRAMFS_ROOT)/bin/zork
	cp $(USER_TCC) $(INITRAMFS_ROOT)/bin/tcc
	cp $(USER_GBEMU) $(INITRAMFS_ROOT)/bin/gbemu
	cp $(USER_NESEMU) $(INITRAMFS_ROOT)/bin/nesemu
	cp $(USER_DASH) $(INITRAMFS_ROOT)/bin/dash
	cp $(USER_DASH) $(INITRAMFS_ROOT)/bin/sh
	cp $(USER_ZIP) $(INITRAMFS_ROOT)/bin/zip
	cp $(USER_UNZIP) $(INITRAMFS_ROOT)/bin/unzip
	cp $(USER_GZIP) $(INITRAMFS_ROOT)/bin/gzip
	cp $(USER_GZIP) $(INITRAMFS_ROOT)/bin/gunzip
	cp $(USER_BEEP) $(INITRAMFS_ROOT)/bin/beep
	cp $(USER_MODPLAY) $(INITRAMFS_ROOT)/bin/modplay
	cp $(USER_MIDIPLAY) $(INITRAMFS_ROOT)/bin/midiplay
	cp $(USER_CHOWN) $(INITRAMFS_ROOT)/bin/chown
	cp $(USER_USERADD) $(INITRAMFS_ROOT)/bin/useradd
	cp $(USER_USERDEL) $(INITRAMFS_ROOT)/bin/userdel
	cp $(USER_GROUPADD) $(INITRAMFS_ROOT)/bin/groupadd
	cp $(USER_GROUPDEL) $(INITRAMFS_ROOT)/bin/groupdel
	cp $(USER_SDLTEST) $(INITRAMFS_ROOT)/bin/sdltest
	@# Klystrack chiptune tracker
	cp $(THIRD_PARTY_DIR)/klystrack/bin.vos/klystrack $(INITRAMFS_ROOT)/bin/klystrack
	mkdir -p $(INITRAMFS_ROOT)/res
	cp $(THIRD_PARTY_DIR)/klystrack/res/* $(INITRAMFS_ROOT)/res/
	mkdir -p $(INITRAMFS_ROOT)/key
	cp $(THIRD_PARTY_DIR)/klystrack/key/* $(INITRAMFS_ROOT)/key/
	for b in $(SBASE_TOOLS); do cp $(SBASE_BIN_DIR)/$$b.elf $(INITRAMFS_ROOT)/bin/$$b; done
	@# Include TCC sysroot in initramfs - will be copied to /disk on first boot
	mkdir -p $(INITRAMFS_ROOT)/sysroot/usr/lib/tcc/include
	mkdir -p $(INITRAMFS_ROOT)/sysroot/usr/include/sys
	cp $(TCC_LIBTCC1) $(INITRAMFS_ROOT)/sysroot/usr/lib/tcc/libtcc1.a
	cp -r third_party/tcc/include/* $(INITRAMFS_ROOT)/sysroot/usr/lib/tcc/include/
	cp $(USER_RUNTIME_LIBS) $(INITRAMFS_ROOT)/sysroot/usr/lib/libvosposix.a
	cp $(USER_ASM_OBJECTS) $(INITRAMFS_ROOT)/sysroot/usr/lib/crt0.o
	cp $(USER_ASM_OBJECTS) $(INITRAMFS_ROOT)/sysroot/usr/lib/crt1.o
	cp $(USER_CRTI_OBJ) $(INITRAMFS_ROOT)/sysroot/usr/lib/crti.o
	cp $(USER_CRTN_OBJ) $(INITRAMFS_ROOT)/sysroot/usr/lib/crtn.o
	@# Create patched libc.a without signal objects (they conflict with libvosposix.a)
	cp $(CROSS_PREFIX)/i686-elf/lib/libc.a $(USER_BUILD_DIR)/libc_patched.a
	$(CROSS_PREFIX)/bin/i686-elf-ar d $(USER_BUILD_DIR)/libc_patched.a libc_a-signal.o libc_a-signalr.o 2>/dev/null || true
	cp $(USER_BUILD_DIR)/libc_patched.a $(INITRAMFS_ROOT)/sysroot/usr/lib/libc.a
	cp $(CROSS_PREFIX)/i686-elf/lib/libm.a $(INITRAMFS_ROOT)/sysroot/usr/lib/libm.a 2>/dev/null || true
	cp -r $(CROSS_PREFIX)/i686-elf/include/* $(INITRAMFS_ROOT)/sysroot/usr/include/
	cp $(USER_DIR)/syscall.h $(INITRAMFS_ROOT)/sysroot/usr/include/syscall.h
	cp $(USER_DIR)/sys/termios.h $(INITRAMFS_ROOT)/sysroot/usr/include/sys/termios.h
	cp $(USER_DIR)/sys/ioctl.h $(INITRAMFS_ROOT)/sysroot/usr/include/sys/ioctl.h
	cp $(USER_DIR)/sys/stat_tcc.h $(INITRAMFS_ROOT)/sysroot/usr/include/sys/stat.h
	@# Game development resources (headers in /usr/dev/game/, examples in /usr/dev/game/examples/)
	mkdir -p $(INITRAMFS_ROOT)/sysroot/usr/dev/game/doc
	mkdir -p $(INITRAMFS_ROOT)/sysroot/usr/dev/game/examples
	find gameResources -maxdepth 1 -type f -exec cp {} $(INITRAMFS_ROOT)/sysroot/usr/dev/game/ \;
	cp -r gameResources/doc/* $(INITRAMFS_ROOT)/sysroot/usr/dev/game/doc/ 2>/dev/null || true
	cp -r gameResources/gameExamples/* $(INITRAMFS_ROOT)/sysroot/usr/dev/game/examples/ 2>/dev/null || true
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
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

# Create a persistent Minix disk image for /disk.
# Uses MBR with a single Minix partition for compatibility.
# NOTE: This only creates the partition table. Run 'make format-disk' to format.
$(DISK_IMG):
	@echo "Creating $(DISK_SIZE_MB)MB disk image with MBR partition table..."
	truncate -s $(DISK_SIZE_MB)M $@
	@# Create MBR partition table with single Minix partition (type 0x81)
	echo 'type=81' | sfdisk $@ >/dev/null 2>&1
	@echo "Partition table created. Run 'make format-disk' to format as Minix."
	@echo "(Requires sudo for loop device access)"

# Format the Minix partition (requires sudo)
# Partition starts at sector 2048, extends to end of disk
DISK_SECTORS = $(shell echo $$(($(DISK_SIZE_MB) * 2048)))
PART_SECTORS = $(shell echo $$(($(DISK_SIZE_MB) * 2048 - 2048)))
PART_OFFSET = 1048576

# Create a fresh disk with Minix filesystem and TCC sysroot installed (ONE command does everything)
# Usage: make disk-setup
disk-setup: $(USER_RUNTIME_OBJECTS) $(USER_RUNTIME_LIBS) $(USER_CRTI_OBJ) $(USER_CRTN_OBJ) $(TCC_LIBTCC1) $(USER_TCC) $(USER_OLIVE)
	@echo "=== VOS Disk Setup (requires sudo) ==="
	rm -f $(DISK_IMG)
	@echo "Creating $(DISK_SIZE_MB) MB disk image..."
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE_MB) status=progress
	@echo "Creating MBR partition table..."
	echo -e "o\nn\np\n1\n2048\n\nt\n81\na\nw" | fdisk $(DISK_IMG)
	@echo "Formatting Minix partition..."
	sudo bash -c '\
		LOOP=$$(losetup --find --show --offset $(PART_OFFSET) $(DISK_IMG)) && \
		mkfs.minix -2 -n 30 $$LOOP && \
		MOUNT=$$(mktemp -d) && \
		mount -t minix $$LOOP $$MOUNT && \
		echo "Installing sysroot..." && \
		bash $(SYSROOT_SCRIPT) $(DISK_IMG) $$MOUNT && \
		sync && \
		umount $$MOUNT && \
		rmdir $$MOUNT && \
		losetup -d $$LOOP \
	'
	@# Fix ownership so user can access the disk (if run via sudo)
	sudo chown $$(logname):$$(logname) $(DISK_IMG) 2>/dev/null || true
	@echo "=== Disk setup complete! TCC is ready to use. ==="

format-disk: $(DISK_IMG)
	@echo "Formatting Minix partition ($(PART_SECTORS) sectors)..."
	sudo losetup -o $$((2048*512)) --sizelimit $$(($(PART_SECTORS)*512)) /dev/loop0 $(DISK_IMG)
	sudo mkfs.minix -2 -n 30 /dev/loop0
	sudo losetup -d /dev/loop0
	@echo "Minix filesystem created on $(DISK_IMG)"

# Force recreate the disk image
disk-recreate:
	rm -f $(DISK_IMG)
	$(MAKE) $(DISK_IMG)
	$(MAKE) format-disk

# Install a sysroot onto $(DISK_IMG) so /usr is populated inside VOS.
sysroot: $(USER_RUNTIME_OBJECTS) $(USER_RUNTIME_LIBS) $(USER_CRTI_OBJ) $(USER_CRTN_OBJ) $(TCC_LIBTCC1) $(USER_TCC) $(USER_OLIVE) $(DISK_IMG)
	bash $(SYSROOT_SCRIPT) $(DISK_IMG)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_TAR)
	rm -rf $(ISO_DIR)/boot/grub/grub.cfg
	rm -f $(ISO)

# Run in QEMU (includes Sound Blaster 16 audio support and persistent disk)
run: $(ISO) $(DISK_IMG)
	qemu-system-i386 -m 4096 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES) \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-device sb16,audiodev=snd0 -audiodev pa,id=snd0

# Run in QEMU with debug output
debug: $(ISO) $(DISK_IMG)
	qemu-system-i386 -m 4096 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES) \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-device sb16,audiodev=snd0 -audiodev pa,id=snd0 -d int -no-reboot

.PHONY: all clean run debug sysroot disk-setup format-disk disk-recreate
