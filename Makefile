# VOS Makefile

# Tools
AS = nasm
CC = gcc
LD = ld

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
INCLUDE_DIR = include
BUILD_DIR = build
ISO_DIR = iso

# Flags
ASFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib \
         -Wall -Wextra -I$(INCLUDE_DIR) -O2 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Source files
ASM_SOURCES = $(BOOT_DIR)/boot.asm
C_SOURCES = $(wildcard $(KERNEL_DIR)/*.c)

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Output
KERNEL = $(BUILD_DIR)/kernel.bin
ISO = vos.iso

# Initramfs (multiboot module)
INITRAMFS_DIR = initramfs
INITRAMFS_TAR = $(ISO_DIR)/boot/initramfs.tar
INITRAMFS_ROOT = $(BUILD_DIR)/initramfs_root

# Simple userland (ELF32)
USER_DIR = user
USER_BUILD_DIR = $(BUILD_DIR)/user
USER_ASM_SOURCES = $(USER_DIR)/crt0.asm
USER_C_SOURCES = $(USER_DIR)/init.c
USER_ASM_OBJECTS = $(USER_BUILD_DIR)/crt0.o
USER_C_OBJECTS = $(USER_BUILD_DIR)/init.o
USER_OBJECTS = $(USER_ASM_OBJECTS) $(USER_C_OBJECTS)
USER_INIT = $(USER_BUILD_DIR)/init.elf

# QEMU defaults
QEMU_XRES ?= 1280
QEMU_YRES ?= 800

# Default target
all: $(ISO)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create user build directory
$(USER_BUILD_DIR): | $(BUILD_DIR)
	mkdir -p $(USER_BUILD_DIR)

# Compile assembly
$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile userland assembly
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.asm | $(USER_BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Compile userland C
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.c | $(USER_BUILD_DIR)
	$(CC) -m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib -Wall -Wextra -O2 -I$(USER_DIR) -c $< -o $@

# Link userland init (static, freestanding)
$(USER_INIT): $(USER_OBJECTS)
	$(LD) -m elf_i386 -T $(USER_DIR)/linker.ld -nostdlib $(USER_OBJECTS) -o $@

# Compile C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link kernel
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# Create bootable ISO
$(ISO): $(KERNEL) $(USER_INIT)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)
	if [ -d "$(INITRAMFS_DIR)" ]; then cp -r $(INITRAMFS_DIR)/* $(INITRAMFS_ROOT)/ 2>/dev/null || true ; fi
	mkdir -p $(INITRAMFS_ROOT)/bin
	cp $(USER_INIT) $(INITRAMFS_ROOT)/bin/init
	tar -C $(INITRAMFS_ROOT) -cf $(INITRAMFS_TAR) .
	echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod all_video' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod gfxterm' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'insmod font' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'loadfont /boot/grub/fonts/unicode.pf2' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set gfxmode=1280x800,1280x720,1024x768,800x600,640x480,auto' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set gfxpayload=keep' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'terminal_output gfxterm' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "VOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    module /boot/initramfs.tar' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)/boot/kernel.bin
	rm -rf $(INITRAMFS_TAR)
	rm -rf $(ISO_DIR)/boot/grub/grub.cfg
	rm -f $(ISO)

# Run in QEMU (for quick testing)
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES)

# Run in QEMU with debug output
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -vga none -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES) -d int -no-reboot

.PHONY: all clean run debug
