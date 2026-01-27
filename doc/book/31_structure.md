# Chapter 31: Project Structure

This chapter provides a comprehensive overview of the VOS source tree organization.

## Directory Layout

```
vos/
├── boot/                   # Boot code
├── kernel/                 # Kernel source files
├── include/                # Kernel headers
├── user/                   # Userland programs
├── initramfs/              # Initial filesystem contents
├── third_party/            # External libraries
├── tools/                  # Build tools and scripts
├── toolchain/              # Cross-compiler (generated)
├── build/                  # Build output (generated)
├── iso/                    # ISO contents (generated)
├── doc/                    # Documentation
├── Makefile                # Main build script
├── linker.ld               # Kernel linker script
└── vos.iso                 # Bootable image (generated)
```

## Boot Directory

```
boot/
└── boot.asm                # Entry point, multiboot header
```

The assembly entry point that:
- Provides multiboot header
- Sets up initial stack
- Calls `kernel_main()`
- Contains interrupt stubs

## Kernel Directory

```
kernel/
├── kernel.c                # Main kernel entry point
├── gdt.c                   # Global Descriptor Table
├── idt.c                   # Interrupt Descriptor Table
├── interrupts.c            # Interrupt handlers
├── timer.c                 # PIT timer driver
├── rtc.c                   # Real-time clock
├── keyboard.c              # Keyboard driver
├── mouse.c                 # Mouse driver
├── screen.c                # Console/framebuffer
├── serial.c                # Serial port (COM1)
├── ata.c                   # ATA disk driver
├── paging.c                # Virtual memory
├── pmm.c                   # Physical memory manager
├── kheap.c                 # Kernel heap
├── early_alloc.c           # Boot-time allocator
├── vfs.c                   # Virtual filesystem
├── vfs_posix.c             # POSIX wrappers
├── ramfs.c                 # RAM filesystem
├── minixfs.c               # Minix filesystem
├── task.c                  # Process management
├── syscall.c               # System call dispatcher
├── elf.c                   # ELF loader
├── shell.c                 # Kernel shell
├── editor.c                # Kernel text editor
├── statusbar.c             # Status bar
├── panic.c                 # Kernel panic handler
├── string.c                # String functions
├── usercopy.c              # User/kernel copy
├── font_psf2.c             # PSF2 font loading
├── font8x8.c               # Built-in 8x8 font
├── font_vga_psf2.c         # VGA font data
├── font_terminus_psf2.c    # Terminus font data
├── ubasic.c                # BASIC interpreter (kernel)
├── tokenizer.c             # BASIC tokenizer
├── basic_programs.c        # BASIC demo programs
├── basic_io.c              # BASIC I/O
└── system.c                # System information
```

### Core Subsystems

| File | Subsystem |
|------|-----------|
| gdt.c, idt.c, interrupts.c | CPU management |
| paging.c, pmm.c, kheap.c | Memory management |
| task.c, syscall.c, elf.c | Process management |
| vfs.c, ramfs.c, minixfs.c | Filesystem |
| keyboard.c, mouse.c | Input devices |
| screen.c, serial.c | Output devices |

## Include Directory

```
include/
├── types.h                 # Basic types (uint32_t, etc.)
├── multiboot.h             # Multiboot structures
├── gdt.h                   # GDT interface
├── idt.h                   # IDT interface
├── interrupts.h            # Interrupt numbers
├── syscall.h               # Syscall numbers
├── paging.h                # Paging interface
├── pmm.h                   # PMM interface
├── kheap.h                 # Heap interface
├── early_alloc.h           # Early allocator
├── vfs.h                   # VFS interface
├── ramfs.h                 # RAMFS interface
├── minixfs.h               # Minix interface
├── task.h                  # Task structures
├── elf.h                   # ELF structures
├── keyboard.h              # Keyboard interface
├── mouse.h                 # Mouse interface
├── screen.h                # Screen interface
├── timer.h                 # Timer interface
├── rtc.h                   # RTC interface
├── serial.h                # Serial interface
├── ata.h                   # ATA interface
├── io.h                    # Port I/O
├── string.h                # String functions
├── panic.h                 # Panic interface
├── kerrno.h                # Kernel error numbers
├── usercopy.h              # User copy interface
├── font.h                  # Font interface
├── font8x8.h               # 8x8 font data
├── shell.h                 # Shell interface
├── editor.h                # Editor interface
├── statusbar.h             # Status bar interface
├── ubasic.h                # BASIC interface
├── tokenizer.h             # BASIC tokenizer
├── basic_programs.h        # BASIC programs
├── system.h                # System info
├── stdio.h                 # Kernel stdio
├── stdlib.h                # Kernel stdlib
└── ctype.h                 # Character types
```

## User Directory

```
user/
├── crt0.asm                # C runtime startup
├── crti.asm                # Init section begin
├── crtn.asm                # Init section end
├── linker.ld               # Userland linker script
├── newlib_syscalls.c       # Newlib syscall stubs
├── env.c                   # Environment variables
├── syscall.h               # Syscall wrappers
├── vos_screen.h            # VOS screen API
├── init.c                  # Init process
├── login.c                 # Login program
├── eliza.c                 # Eliza chatbot
├── uptime.c                # Uptime command
├── setdate.c               # Set date command
├── ps.c                    # Process list
├── top.c                   # Process monitor
├── neofetch.c              # System info display
├── font.c                  # Font selector
├── json.c                  # JSON parser tool
├── img.c                   # Image viewer
├── s3lcube.c               # 3D cube demo
├── olivedemo.c             # 2D graphics demo
├── basic/                  # BASIC interpreter
│   ├── basic.c             # Main entry
│   ├── ubasic.c            # Interpreter
│   ├── tokenizer.c         # Tokenizer
│   └── basic_programs.c    # Demo programs
├── zork1c/                 # Zork text adventure
│   ├── _parser.c
│   ├── _game.c
│   └── ...
└── sys/                    # System headers
    ├── stat.h
    ├── sysmacros.h
    └── utsname.h
```

### Program Categories

| Category | Programs |
|----------|----------|
| System | init, login, dash (sh) |
| Utilities | uptime, ps, top, sysview, neofetch |
| Editors | vi (nextvi) |
| Entertainment | eliza, zork, basic |
| Graphics | s3lcube, s3lfly, olivedemo, img |
| Development | tcc, font, theme, json |

## Third-Party Directory

```
third_party/
├── microrl/                # Readline-style editing (kernel)
│   ├── microrl.c
│   └── microrl.h
├── linenoise/              # Line editing (userland)
│   ├── linenoise.c
│   └── linenoise.h
├── jsmn/                   # JSON parser
│   └── jsmn.h
├── stb/                    # stb_image
│   └── stb_image.h
├── small3dlib/             # 3D software renderer
│   └── small3dlib.h
├── olive/                  # 2D graphics library
│   ├── olive.c
│   └── olive.h
├── fonts/                  # PSF2 fonts
│   ├── spleen/
│   └── terminus/
├── tcc/                    # Tiny C Compiler
│   ├── tcc.c
│   ├── libtcc.c
│   └── ...
├── sbase/                  # Portable Unix utilities
│   ├── cat.c
│   ├── ls.c
│   ├── grep.c
│   └── ...
├── ne/                     # Nice Editor (available, not built by default)
│   └── src/
│       └── ...
└── xv6-public/             # Reference material
```

## Tools Directory

```
tools/
├── build_newlib_toolchain.sh   # Build cross-compiler
├── install_sysroot.sh          # Install dev files to disk
└── mkfat.c                     # FAT image creator
```

## Build Output

```
build/
├── boot.o                  # Compiled boot code
├── *.o                     # Kernel objects
├── kernel.bin              # Linked kernel
├── fonts/                  # Font objects
│   └── spleen/
│       └── spleen-16x32.o
├── user/                   # Userland objects
│   ├── crt0.o
│   ├── dash/               # Dash shell objects
│   ├── dash.elf
│   └── ...
├── initramfs_root/         # Initramfs staging
│   └── bin/
│       ├── init
│       ├── sh (dash)
│       ├── dash
│       └── ...
└── tools/                  # Build tools
    └── mkfat
```

## ISO Structure

```
iso/
└── boot/
    ├── kernel.bin          # VOS kernel
    ├── initramfs.tar       # Initial filesystem
    ├── fat.img             # FAT demo image
    └── grub/
        ├── grub.cfg        # GRUB configuration
        └── fonts/
            └── unicode.pf2 # GRUB font
```

## Documentation

```
doc/
├── DOCUMENTATION.md        # Legacy single file
├── vos_capabilities.md     # Feature reference
└── book/                   # This book
    ├── 00_index.md
    ├── 01_introduction.md
    └── ...
```

## Key Files

### Entry Points

| File | Purpose |
|------|---------|
| boot/boot.asm | System entry (_start) |
| kernel/kernel.c | Kernel entry (kernel_main) |
| user/crt0.asm | User entry (_start → main) |
| user/init.c | First user process |

### Configuration

| File | Purpose |
|------|---------|
| Makefile | Build rules |
| linker.ld | Kernel memory layout |
| user/linker.ld | User memory layout |
| iso/boot/grub/grub.cfg | GRUB configuration |

### Data Files

| Location | Contents |
|----------|----------|
| initramfs/ | Files for initial ramdisk |
| third_party/fonts/ | PSF2 font files |

## Code Statistics

Approximate line counts:

| Component | Lines |
|-----------|-------|
| Kernel C | ~15,000 |
| Kernel ASM | ~500 |
| Userland | ~8,000 |
| Third-party | ~50,000+ |
| Total (VOS code) | ~23,500 |

## Dependency Graph

```
boot.asm
    └── kernel.c
        ├── gdt.c
        ├── idt.c
        │   └── interrupts.c
        ├── pmm.c
        │   └── paging.c
        │       └── kheap.c
        ├── timer.c
        ├── keyboard.c
        ├── screen.c
        ├── vfs.c
        │   ├── ramfs.c
        │   └── minixfs.c
        │       └── ata.c
        ├── task.c
        │   ├── elf.c
        │   └── syscall.c
        └── shell.c
```

## Summary

VOS project structure follows a modular organization:

1. **Clear separation** between kernel and userland
2. **Isolated subsystems** (memory, filesystem, tasks)
3. **Third-party code** in separate directory
4. **Build artifacts** separated from source
5. **Documentation** alongside code

This structure facilitates:
- Understanding code organization
- Adding new features
- Debugging issues
- Building and testing

---

*Previous: [Chapter 30: Testing with QEMU](30_testing.md)*
*Next: [Chapter 32: Future Enhancements](32_future.md)*
