# Chapter 1: Introduction

## What is an Operating System?

An operating system (OS) is the fundamental software that manages computer hardware and provides services for programs. At its core, an OS handles:

- **Hardware Abstraction**: Providing a consistent interface to diverse hardware
- **Process Management**: Running and scheduling programs
- **Memory Management**: Allocating and protecting memory
- **I/O Management**: Handling input/output devices
- **File Systems**: Organizing and storing data

## About VOS

VOS (Victor's Operating System) is an educational operating system designed to demonstrate fundamental OS concepts while providing practical functionality. It has evolved from a simple educational kernel to a capable system that can:

### Core Features

- **32-bit x86 architecture** (i386/i686)
- **Multiboot-compliant** bootloader support (works with GRUB)
- **High-resolution framebuffer console** with PSF2 fonts and ANSI escape sequences
- **VGA text mode fallback** for compatibility
- **Blue/white terminal UI** with safe-area padding and status bar

### Hardware Support

- **PS/2 Keyboard** with Spanish layout and AltGr support
- **PS/2 Mouse** with xterm-compatible mouse reporting
- **ATA/IDE disk** for persistent storage
- **Serial port (COM1)** at 115200 baud for debugging
- **PIT timer** (uptime, sleep, scheduler tick)
- **CMOS RTC** (date/time, settable)

### Memory Management

- **Paging** with 4KB pages
- **Physical Memory Manager (PMM)** with bitmap allocation
- **Kernel heap** with kmalloc/kfree
- **User-space memory** with sbrk and mmap syscalls
- **Virtual Memory Areas (VMAs)** for process memory tracking

### Process Management

- **71 system calls** for comprehensive functionality
- **Preemptive multitasking** with round-robin scheduling
- **fork() and execve()** for UNIX-style process creation
- **Signals** (32 signals with handlers and delivery)
- **Process groups** and session management
- **Pipes** for inter-process communication
- **Terminal I/O (termios)** with canonical and raw modes

### File Systems

- **Virtual File System (VFS)** with mount points
- **RAM filesystem (ramfs)** for temporary storage
- **FAT16 filesystem** for persistent disk storage
- **initramfs** (tar) for initial root filesystem
- **POSIX-like file operations** (open, read, write, seek, stat, etc.)

### User Space

- **ELF32 loader** for executable programs
- **Newlib C library** integration
- **TCC (Tiny C Compiler)** for native compilation
- **40+ user programs** including shell utilities
- **uBASIC interpreter** with 10 example programs
- **Graphics support** via olive.c and small3dlib

## Why Build an OS from Scratch?

Building an operating system teaches you:

1. **How computers actually work at the lowest level**
   - CPU modes and privilege levels
   - Memory addressing and protection
   - Hardware communication via ports and interrupts

2. **Systems programming fundamentals**
   - Assembly language and its relationship to C
   - Memory management without garbage collection
   - Concurrent programming and synchronization

3. **Operating system concepts**
   - Process scheduling and context switching
   - Virtual memory and address spaces
   - File systems and device drivers
   - System call interfaces

4. **Debugging techniques**
   - Working without a debugger (serial logging, panic dumps)
   - Understanding crash dumps and register states
   - Hardware-level debugging

## VOS Design Philosophy

VOS follows several design principles:

### Educational First
Code is written for clarity over cleverness. Each subsystem is documented and explained.

### Incremental Complexity
Features are layered: early boot uses simple mechanisms, which are replaced by more sophisticated ones as the system initializes.

### Practical Functionality
While educational, VOS aims to be a usable system where you can compile programs, run utilities, and explore.

### POSIX Inspiration
Where practical, VOS follows POSIX conventions for syscalls and interfaces, making it easier to port existing software.

## What VOS Can Do

### Interactive Shell
```
VOS Shell v2.0
Type 'help' for available commands.

vos:/$ ls /disk
home/
usr/
etc/
vos:/$ cat /etc/motd
Welcome to VOS!
vos:/$ tcc -o hello hello.c
vos:/$ ./hello
Hello, World!
```

### Graphics Programs
VOS supports framebuffer graphics through syscalls, with libraries like olive.c for 2D rendering and small3dlib for software 3D.

### Development Environment
With TCC integrated, you can write, compile, and run C programs entirely within VOS.

## System Requirements

### For Running in QEMU
- Any modern x86/x64 Linux system
- QEMU with i386 emulation
- 64MB RAM allocated to VM (minimum)
- 4GB disk image for FAT16 partition

### For Running on Real Hardware
- i386 or newer CPU (32-bit mode)
- PS/2 keyboard (or USB keyboard with legacy support)
- VGA-compatible display
- IDE/ATA hard drive (or compatible controller)

---

*Next: [Chapter 2: Prerequisites and Tools](02_prerequisites.md)*
