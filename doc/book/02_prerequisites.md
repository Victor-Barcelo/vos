# Chapter 2: Prerequisites and Tools

## Required Knowledge

Before diving into OS development, you should understand:

### C Programming
- Pointers and pointer arithmetic
- Memory management (malloc/free concepts)
- Structs and unions
- Bitwise operations
- Function pointers

### Basic Assembly
- x86 registers (EAX, EBX, etc.)
- Stack operations (push, pop)
- Common instructions (mov, add, cmp, jmp)
- Calling conventions

### Binary and Hexadecimal
- Number system conversions
- Bit manipulation
- Memory addressing

### Computer Architecture
- CPU, memory, and bus concepts
- I/O ports and memory-mapped I/O
- Interrupt handling basics

## Development Tools

### Assembler: NASM

NASM (Netwide Assembler) is used for boot code and interrupt handlers.

```bash
# Ubuntu/Debian
sudo apt install nasm

# Fedora
sudo dnf install nasm

# Arch Linux
sudo pacman -S nasm
```

NASM assembles our `.asm` files into ELF32 object files that can be linked with C code.

### Compiler: GCC (32-bit)

We use GCC with special flags for freestanding compilation.

```bash
# Ubuntu/Debian
sudo apt install gcc gcc-multilib

# Fedora
sudo dnf install gcc gcc-multilib

# Arch Linux
sudo pacman -S gcc
```

For cross-compilation (recommended for serious development):

```bash
# Build or install i686-elf-gcc cross-compiler
# See OSDev Wiki for cross-compiler build instructions
```

### Linker: GNU LD

The GNU linker combines object files into the final kernel binary.

```bash
# Usually installed with binutils
sudo apt install binutils
```

### ISO Creation Tools

These tools create bootable ISO images with GRUB:

```bash
# Ubuntu/Debian
sudo apt install grub-pc-bin xorriso mtools

# Fedora
sudo dnf install grub2-pc xorriso mtools

# Arch Linux
sudo pacman -S grub xorriso mtools
```

### Virtualization

For testing without rebooting real hardware:

```bash
# QEMU (recommended for quick testing)
sudo apt install qemu-system-x86

# VirtualBox (for more realistic testing)
sudo apt install virtualbox
```

## Setting Up the Environment

### Project Structure

```
vos/
├── boot/           # Assembly entry point
├── kernel/         # Kernel C source files
├── include/        # Header files
├── user/           # User-space programs
├── third_party/    # External libraries
├── initramfs/      # Initial filesystem content
├── doc/            # Documentation
├── linker.ld       # Kernel linker script
└── Makefile        # Build system
```

### Essential Makefile Targets

```makefile
# Build the ISO
make

# Run in QEMU
make run

# Run with serial output visible
make run-serial

# Clean build artifacts
make clean

# Build disk image with Minix filesystem
make disk format-disk
```

### QEMU Testing Setup

```bash
# Basic run
qemu-system-i386 -cdrom vos.iso

# With high-resolution framebuffer
qemu-system-i386 -cdrom vos.iso \
    -vga none \
    -device bochs-display,xres=1280,yres=800

# With serial output
qemu-system-i386 -cdrom vos.iso \
    -serial stdio

# With disk image
qemu-system-i386 -cdrom vos.iso \
    -drive file=disk.img,format=raw,if=ide
```

### Cross-Compiler Setup (Recommended)

For reliable OS development, build a cross-compiler:

```bash
# Set target
export TARGET=i686-elf
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

# Download sources
wget https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.xz
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz

# Build binutils
tar xf binutils-2.40.tar.xz
cd binutils-2.40
mkdir build && cd build
../configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make && make install

# Build GCC
cd ../..
tar xf gcc-13.2.0.tar.xz
cd gcc-13.2.0
mkdir build && cd build
../configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make all-gcc && make all-target-libgcc
make install-gcc && make install-target-libgcc
```

## Compiler Flags Explained

VOS uses specific compiler flags for freestanding kernel code:

| Flag | Purpose |
|------|---------|
| `-m32` | Generate 32-bit code |
| `-ffreestanding` | No standard library, no assumptions about environment |
| `-fno-stack-protector` | Disable stack canaries (no libc to support them) |
| `-fno-pie` | Disable position-independent executable |
| `-nostdlib` | Don't link standard libraries |
| `-Wall -Wextra` | Enable all warnings |
| `-I$(INCLUDE_DIR)` | Add include directory |
| `-O2` | Optimization level 2 |

## Linker Flags Explained

| Flag | Purpose |
|------|---------|
| `-m elf_i386` | Output 32-bit ELF format |
| `-T linker.ld` | Use our linker script |
| `-nostdlib` | Don't link standard libraries |

## Debugging Tools

### GDB with QEMU

```bash
# Start QEMU with GDB server
qemu-system-i386 -cdrom vos.iso -s -S

# In another terminal
gdb build/kernel.bin
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Serial Logging

VOS mirrors all output to COM1. Capture it:

```bash
# In terminal
qemu-system-i386 -cdrom vos.iso -serial stdio

# To a file
qemu-system-i386 -cdrom vos.iso -serial file:debug.log
```

### objdump

Examine compiled binaries:

```bash
# Disassemble
objdump -d build/kernel.bin | less

# Show sections
objdump -h build/kernel.bin

# Show symbols
nm build/kernel.bin
```

## Editor Setup

Any text editor works, but these have good support for OS development:

### VS Code
- C/C++ extension for syntax highlighting
- x86 Assembly extension for .asm files
- Makefile Tools extension

### Vim/Neovim
- Built-in syntax highlighting for C and Assembly
- ctags for code navigation

### Emacs
- CC Mode for C
- asm-mode for Assembly

## Next Steps

With your environment set up:

1. Clone or create the VOS project structure
2. Build the kernel with `make`
3. Test with `make run`
4. Verify you see the VOS shell prompt

If everything works, you're ready to understand how VOS boots and runs.

---

*Previous: [Chapter 1: Introduction](01_introduction.md)*
*Next: [Chapter 3: Understanding the Boot Process](03_boot_process.md)*
