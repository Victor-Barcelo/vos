# Chapter 29: Build System

VOS uses a comprehensive Makefile-based build system that compiles the kernel, userland programs, and creates a bootable ISO image.

## Build Overview

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ boot.asm    │────>│ boot.o      │     │             │
└─────────────┘     └─────────────┘     │             │
                            │            │             │
┌─────────────┐     ┌─────────────┐     │             │
│ kernel/*.c  │────>│ *.o         │────>│ kernel.bin  │
└─────────────┘     └─────────────┘     │             │
                            │            │             │
┌─────────────┐     ┌─────────────┐     │             │
│ fonts/*.psf │────>│ fonts/*.o   │     │             │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                               │
                                               v
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ user/*.c    │────>│ *.elf       │────>│ initramfs   │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                               │
                                               v
                                        ┌─────────────┐
                                        │  vos.iso    │
                                        └─────────────┘
```

## Cross-Compiler Toolchain

### Requirements

VOS requires a cross-compiler targeting `i686-elf`:

- **GCC**: i686-elf-gcc
- **Binutils**: i686-elf-ld, i686-elf-ar
- **Newlib**: C library for userland

### Building the Toolchain

```bash
./tools/build_newlib_toolchain.sh
```

This script:
1. Downloads GCC, Binutils, and Newlib sources
2. Builds Binutils for i686-elf
3. Builds GCC stage 1 (C compiler only)
4. Builds Newlib C library
5. Builds GCC stage 2 (with Newlib support)

Installation location: `toolchain/opt/cross/`

### Toolchain Variables

```makefile
CROSS_PREFIX ?= $(CURDIR)/toolchain/opt/cross
CROSS_TARGET ?= i686-elf
CROSS_CC := $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-gcc
CROSS_LD := $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-ld
AR = $(CROSS_PREFIX)/bin/$(CROSS_TARGET)-ar
```

## Makefile Structure

### Directory Layout

```makefile
BOOT_DIR = boot
KERNEL_DIR = kernel
INCLUDE_DIR = include
THIRD_PARTY_DIR = third_party
BUILD_DIR = build
ISO_DIR = iso
USER_DIR = user
```

### Compiler Flags

#### Kernel Flags

```makefile
ASFLAGS = -f elf32
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -nostdlib \
         -Wall -Wextra -I$(INCLUDE_DIR) -O2 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
```

#### Userland Flags

```makefile
USER_CFLAGS = -ffreestanding -fno-stack-protector -fno-pie \
              -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
USER_LDFLAGS = -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld \
               -Wl,--gc-sections
```

## Build Targets

### Main Targets

```makefile
all: $(ISO)        # Build bootable ISO
clean:             # Remove build artifacts
run: $(ISO)        # Run in QEMU
debug: $(ISO)      # Run with debugging
sysroot:           # Install development files
```

### Building the Kernel

```makefile
# Compile assembly
$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm
    $(AS) $(ASFLAGS) $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
    $(CC) $(CFLAGS) $< -o $@

# Link kernel
$(KERNEL): $(OBJECTS)
    $(LD) $(LDFLAGS) $(OBJECTS) -o $@
```

### Font Embedding

PSF2 fonts are embedded as binary objects:

```makefile
EXTRA_FONT_PSF = \
    $(FONTS_DIR)/spleen/spleen-12x24.psf \
    $(FONTS_DIR)/spleen/spleen-16x32.psf \
    $(FONTS_DIR)/terminus/Uni3-Terminus28x14.psf

$(FONTS_BUILD_DIR)/%.o: $(FONTS_DIR)/%.psf
    $(LD) -r -b binary $< -o $@
```

### Building Userland Programs

```makefile
# Compile userland C
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.c
    $(CC) $(USER_CFLAGS) -c $< -o $@

# Link userland binary
$(USER_SH): $(USER_RUNTIME_OBJECTS) $(USER_SH_OBJ) $(USER_RUNTIME_LIBS)
    $(CC) -nostartfiles -Wl,-T,$(USER_DIR)/linker.ld \
          -o $@ $^ -lc -lgcc
```

### Initramfs Creation

```makefile
$(INITRAMFS_TAR): $(USER_BINS) $(INITRAMFS_FILES)
    rm -rf $(INITRAMFS_ROOT)
    mkdir -p $(INITRAMFS_ROOT)/bin
    cp $(USER_INIT) $(INITRAMFS_ROOT)/bin/init
    cp $(USER_SH) $(INITRAMFS_ROOT)/bin/sh
    # ... copy more binaries
    tar -C $(INITRAMFS_ROOT) -cf $(INITRAMFS_TAR) .
```

### ISO Creation

```makefile
$(ISO): $(KERNEL) $(USER_BINS) $(INITRAMFS_TAR)
    mkdir -p $(ISO_DIR)/boot/grub
    cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
    # Generate GRUB config
    echo 'menuentry "VOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
    echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
    echo '    module /boot/initramfs.tar' >> $(ISO_DIR)/boot/grub/grub.cfg
    echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
    grub-mkrescue -o $(ISO) $(ISO_DIR)
```

## Third-Party Libraries

### sbase (Unix Utilities)

```makefile
SBASE_TOOLS = cat echo ls pwd cp mv rm mkdir rmdir grep sed ...

$(SBASE_BUILD_DIR)/%.o: $(SBASE_DIR)/%.c
    $(CC) $(USER_CFLAGS) -DPATH_MAX=256 -c $< -o $@

$(SBASE_LIBUTIL): $(SBASE_LIBUTIL_OBJECTS)
    $(AR) rcs $@ $^
```

### TinyCC (Native Compiler)

```makefile
TCC_C_SOURCES = $(TCC_DIR)/tcc.c $(TCC_DIR)/libtcc.c ...

$(TCC_BUILD_DIR)/%.o: $(TCC_DIR)/%.c
    $(CC) $(USER_CFLAGS) -DONE_SOURCE=0 -DTCC_TARGET_I386 \
          -DCONFIG_TCC_STATIC -c $< -o $@
```

### Olive.c (Graphics)

```makefile
$(OLIVE_OBJ): $(OLIVE_DIR)/olive.c
    $(CC) $(USER_CFLAGS) -DOLIVEC_IMPLEMENTATION -c $< -o $@

$(USER_OLIVE): $(OLIVE_OBJ)
    $(AR) rcs $@ $^
```

## Linker Scripts

### Kernel Linker Script (linker.ld)

```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
```

### Userland Linker Script (user/linker.ld)

```ld
ENTRY(_start)

SECTIONS
{
    . = 0x08048000;

    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) *(COMMON) }
}
```

## QEMU Targets

### Basic Run

```makefile
QEMU_XRES ?= 1920
QEMU_YRES ?= 1080

run: $(ISO)
    qemu-system-i386 -cdrom $(ISO) -vga none \
        -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES)
```

### Debug Run

```makefile
debug: $(ISO)
    qemu-system-i386 -cdrom $(ISO) -vga none \
        -device bochs-display,xres=$(QEMU_XRES),yres=$(QEMU_YRES) \
        -d int -no-reboot
```

### Custom Resolution

```bash
make run QEMU_XRES=1280 QEMU_YRES=720
```

## Sysroot Installation

The sysroot target installs development files to a FAT16 disk image:

```makefile
DISK_IMG ?= vos-disk.img
DISK_SIZE_MB ?= 256

$(DISK_IMG):
    truncate -s $(DISK_SIZE_MB)M $@
    mkfs.fat -F 16 -n VOSDISK $@

sysroot: $(USER_RUNTIME_OBJECTS) $(TCC_LIBTCC1) $(DISK_IMG)
    bash $(SYSROOT_SCRIPT) $(DISK_IMG)
```

The script installs:
- `/usr/include/` - Header files
- `/usr/lib/` - Libraries (libc.a, libm.a, crt0.o)
- Enables native compilation with TCC

## Build Commands

```bash
# Build everything
make

# Build and run
make run

# Build with specific resolution
make run QEMU_XRES=1024 QEMU_YRES=768

# Clean build
make clean

# Install sysroot for native development
make sysroot

# Debug build
make debug
```

## Adding New Programs

### 1. Add Source File

Create `user/myprogram.c`

### 2. Add to Makefile

```makefile
USER_MYPROGRAM_OBJ = $(USER_BUILD_DIR)/myprogram.o
USER_MYPROGRAM = $(USER_BUILD_DIR)/myprogram.elf

$(USER_MYPROGRAM): $(USER_RUNTIME_OBJECTS) $(USER_MYPROGRAM_OBJ) $(USER_RUNTIME_LIBS)
    $(USER_LINK_CMD)

USER_BINS += $(USER_MYPROGRAM)
```

### 3. Add to Initramfs

```makefile
$(ISO): ... $(USER_MYPROGRAM) ...
    ...
    cp $(USER_MYPROGRAM) $(INITRAMFS_ROOT)/bin/myprogram
```

## Summary

The VOS build system provides:

1. **Cross-compilation** with i686-elf toolchain
2. **Kernel building** with freestanding flags
3. **Userland programs** linked with Newlib
4. **Third-party integration** (sbase, TCC, dash, olive.c, nextvi)
5. **Font embedding** as binary objects
6. **ISO generation** with GRUB
7. **QEMU integration** for testing
8. **Sysroot installation** for native development

---

*Previous: [Chapter 28: BASIC Interpreter](28_basic.md)*
*Next: [Chapter 30: Testing with QEMU](30_testing.md)*
