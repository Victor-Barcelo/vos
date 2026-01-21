# VOS Makefile

# Tools
AS = nasm

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

# Simple userland (ELF32 + newlib)
USER_DIR = user
USER_BUILD_DIR = $(BUILD_DIR)/user
USER_ASM_SOURCES = $(USER_DIR)/crt0.asm
USER_ASM_OBJECTS = $(USER_BUILD_DIR)/crt0.o
USER_RUNTIME_C_SOURCES = $(USER_DIR)/newlib_syscalls.c
USER_RUNTIME_C_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_RUNTIME_C_SOURCES))
USER_RUNTIME_OBJECTS = $(USER_ASM_OBJECTS) $(USER_RUNTIME_C_OBJECTS)
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
# Zork I (userland)
USER_ZORK_DIR = $(USER_DIR)/zork1c
USER_ZORK_C_SOURCES = $(USER_ZORK_DIR)/_parser.c $(USER_ZORK_DIR)/_game.c $(USER_ZORK_DIR)/_villains.c \
                      $(USER_ZORK_DIR)/_data.c $(USER_ZORK_DIR)/tables.c $(USER_ZORK_DIR)/mt.c $(USER_ZORK_DIR)/compress.c
USER_ZORK_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_ZORK_C_SOURCES))
USER_ZORK = $(USER_BUILD_DIR)/zork.elf

USER_LINENOISE_OBJ = $(USER_BUILD_DIR)/linenoise.o

# BASIC (userland)
USER_BASIC_DIR = $(USER_DIR)/basic
USER_BASIC_C_SOURCES = $(USER_BASIC_DIR)/basic.c $(USER_BASIC_DIR)/ubasic.c $(USER_BASIC_DIR)/tokenizer.c $(USER_BASIC_DIR)/basic_programs.c
USER_BASIC_OBJECTS = $(patsubst $(USER_DIR)/%.c,$(USER_BUILD_DIR)/%.o,$(USER_BASIC_C_SOURCES))
USER_BASIC = $(USER_BUILD_DIR)/basic.elf

USER_BINS = $(USER_INIT) $(USER_ELIZA) $(USER_LSH) $(USER_SH) $(USER_UPTIME) $(USER_DATE) $(USER_SETDATE) $(USER_PS) $(USER_TOP) $(USER_NEOFETCH) $(USER_FONT) $(USER_BASIC) $(USER_ZORK)

# QEMU defaults
QEMU_XRES ?= 1920
QEMU_YRES ?= 1080

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
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -c $< -o $@

# Vendored linenoise (userland line editing)
$(USER_LINENOISE_OBJ): $(THIRD_PARTY_DIR)/linenoise/linenoise.c | $(USER_BUILD_DIR)
	$(CC) -ffreestanding -fno-stack-protector -fno-pie -Wall -Wextra -O2 -I$(USER_DIR) -I$(THIRD_PARTY_DIR)/linenoise -c $< -o $@

# Link userland init (static, freestanding)
$(USER_INIT): $(USER_RUNTIME_OBJECTS) $(USER_INIT_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland eliza (static, freestanding)
$(USER_ELIZA): $(USER_RUNTIME_OBJECTS) $(USER_ELIZA_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland linenoise demo
$(USER_LSH): $(USER_RUNTIME_OBJECTS) $(USER_LINENOISE_OBJ) $(USER_LSH_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland shell (linenoise)
$(USER_SH): $(USER_RUNTIME_OBJECTS) $(USER_LINENOISE_OBJ) $(USER_SH_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland uptime/date/setdate/ps/top (static, freestanding)
$(USER_UPTIME): $(USER_RUNTIME_OBJECTS) $(USER_UPTIME_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

$(USER_DATE): $(USER_RUNTIME_OBJECTS) $(USER_DATE_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

$(USER_SETDATE): $(USER_RUNTIME_OBJECTS) $(USER_SETDATE_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

$(USER_PS): $(USER_RUNTIME_OBJECTS) $(USER_PS_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

$(USER_TOP): $(USER_RUNTIME_OBJECTS) $(USER_TOP_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland neofetch (static, freestanding)
$(USER_NEOFETCH): $(USER_RUNTIME_OBJECTS) $(USER_NEOFETCH_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland font selector
$(USER_FONT): $(USER_RUNTIME_OBJECTS) $(USER_FONT_OBJ)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland BASIC interpreter
$(USER_BASIC): $(USER_RUNTIME_OBJECTS) $(USER_BASIC_OBJECTS)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

# Link userland zork (static, freestanding)
$(USER_ZORK): $(USER_RUNTIME_OBJECTS) $(USER_ZORK_OBJECTS)
	$(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld -Wl,--gc-sections -o $@ $^ -lc -lgcc

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
$(ISO): $(KERNEL) $(USER_BINS) $(FAT_IMG)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)
	if [ -d "$(INITRAMFS_DIR)" ]; then cp -r $(INITRAMFS_DIR)/* $(INITRAMFS_ROOT)/ 2>/dev/null || true ; fi
	mkdir -p $(INITRAMFS_ROOT)/bin
	cp $(USER_INIT) $(INITRAMFS_ROOT)/bin/init
	cp $(USER_ELIZA) $(INITRAMFS_ROOT)/bin/eliza
	cp $(USER_LSH) $(INITRAMFS_ROOT)/bin/lsh
	cp $(USER_SH) $(INITRAMFS_ROOT)/bin/sh
	cp $(USER_UPTIME) $(INITRAMFS_ROOT)/bin/uptime
	cp $(USER_DATE) $(INITRAMFS_ROOT)/bin/date
	cp $(USER_SETDATE) $(INITRAMFS_ROOT)/bin/setdate
	cp $(USER_PS) $(INITRAMFS_ROOT)/bin/ps
	cp $(USER_TOP) $(INITRAMFS_ROOT)/bin/top
	cp $(USER_NEOFETCH) $(INITRAMFS_ROOT)/bin/neofetch
	cp $(USER_FONT) $(INITRAMFS_ROOT)/bin/font
	cp $(USER_BASIC) $(INITRAMFS_ROOT)/bin/basic
	cp $(USER_ZORK) $(INITRAMFS_ROOT)/bin/zork
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

.PHONY: all clean run debug
