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
├── speaker.c               # PC speaker driver
├── sb16.c                  # Sound Blaster 16 audio driver
├── dma.c                   # DMA controller for audio
├── ata.c                   # ATA disk driver
├── mbr.c                   # Master Boot Record parsing
├── paging.c                # Virtual memory
├── pmm.c                   # Physical memory manager
├── kheap.c                 # Kernel heap
├── early_alloc.c           # Boot-time allocator
├── vfs.c                   # Virtual filesystem
├── vfs_posix.c             # POSIX wrappers
├── ramfs.c                 # RAM filesystem
├── minixfs.c               # Minix filesystem
├── fatdisk.c               # Legacy FAT16 support (deprecated)
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
├── emoji_data.c            # 89 color emoji with alpha blending
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
| ata.c, mbr.c | Storage |
| keyboard.c, mouse.c | Input devices |
| screen.c, serial.c | Output devices |
| sb16.c, dma.c, speaker.c | Audio |
| emoji_data.c, font_psf2.c | Graphics/fonts |

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
├── fatdisk.h               # FAT16 interface (legacy)
├── mbr.h                   # MBR parsing
├── task.h                  # Task structures
├── elf.h                   # ELF structures
├── keyboard.h              # Keyboard interface
├── mouse.h                 # Mouse interface
├── screen.h                # Screen interface
├── timer.h                 # Timer interface
├── rtc.h                   # RTC interface
├── serial.h                # Serial interface
├── speaker.h               # PC speaker interface
├── sb16.h                  # Sound Blaster 16 interface
├── dma.h                   # DMA controller interface
├── ata.h                   # ATA interface
├── io.h                    # Port I/O
├── string.h                # String functions
├── panic.h                 # Panic interface
├── kerrno.h                # Kernel error numbers
├── usercopy.h              # User copy interface
├── font.h                  # Font interface
├── font8x8.h               # 8x8 font data
├── emoji.h                 # Emoji rendering interface
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
├── newlib_syscalls.c       # Newlib syscall stubs (~3,390 lines)
├── env.c                   # Environment variables
├── syscall.h               # Syscall wrappers
├── vos_screen.h            # VOS screen API
├── init.c                  # Init process (termbox2 installer UI)
├── login.c                 # Login program
├── eliza.c                 # Eliza chatbot
├── uptime.c                # Uptime command
├── setdate.c               # Set date command
├── ps.c                    # Process list
├── top.c                   # Process monitor
├── sysview.c               # System information viewer
├── neofetch.c              # System info display
├── font.c                  # Font selector
├── theme.c                 # Color theme selector
├── json.c                  # JSON parser tool
├── img.c                   # Image viewer (SDL_image)
├── df.c                    # Disk usage statistics
├── tree.c                  # Directory tree visualization
├── s3lcube.c               # 3D cube demo
├── s3lfly.c                # 3D flight demo
├── olivedemo.c             # 2D graphics demo
├── gbemu.c                 # Game Boy emulator (Peanut-GB)
├── nesemu.c                # NES emulator (Nofrendo)
├── modplay.c               # MOD/tracker player (pocketmod)
├── midiplay.c              # MIDI player (TinySoundFont)
├── beep.c                  # PC speaker beeper
├── mdview.c                # Markdown viewer
├── gzip.c                  # Gzip compression
├── zip.c                   # ZIP archive tool
├── unzip.c                 # ZIP extraction tool
├── useradd.c               # Add user accounts
├── userdel.c               # Delete user accounts
├── groupadd.c              # Add groups
├── groupdel.c              # Delete groups
├── chown.c                 # Change file ownership
├── basic/                  # BASIC interpreter
│   ├── basic.c             # Main entry
│   ├── ubasic.c            # Interpreter
│   ├── tokenizer.c         # Tokenizer
│   └── basic_programs.c    # Demo programs
├── zork1c/                 # Zork text adventure
│   ├── _parser.c
│   ├── _game.c
│   └── ...
├── nextvi/                 # Vi editor port (41 files)
│   └── vi.c
└── sys/                    # System headers
    ├── stat.h
    ├── sysmacros.h
    └── utsname.h
```

### Program Categories

| Category | Programs |
|----------|----------|
| System | init, login, dash (sh) |
| Utilities | uptime, ps, top, sysview, neofetch, df, tree |
| Editors | vi (nextvi), mdview |
| Entertainment | eliza, zork, basic, gbemu, nesemu |
| Graphics | s3lcube, s3lfly, olivedemo, img |
| Audio | modplay, midiplay, beep |
| Development | tcc, font, theme, json, gzip, zip, unzip |
| User Management | useradd, userdel, groupadd, groupdel, chown |

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
├── sheredom_json/          # JSON writer
│   └── json.h
├── stb/                    # stb single-header libraries
│   └── stb_image.h
├── small3dlib/             # 3D software renderer
│   └── small3dlib.h
├── olive/                  # 2D graphics library
│   ├── olive.c
│   └── olive.h
├── nofrendo/               # NES emulator (14+ files)
│   ├── nes.c
│   └── ...
├── peanut-gb/              # Game Boy emulator (single-header)
│   └── peanut_gb.h
├── pocketmod/              # MOD player library
│   └── pocketmod.h
├── tsf/                    # TinySoundFont (MIDI synthesis)
│   └── tsf.h
├── md4c/                   # Markdown parser
│   └── md4c.h
├── miniz/                  # Compression library
│   └── miniz.h
├── termbox2/               # TUI library
│   └── termbox2.h
├── fonts/                  # PSF2 fonts (42 fonts)
│   ├── spleen/
│   └── terminus/
├── emoji/                  # Emoji data (89 color emoji)
│   └── ...
├── tcc/                    # Tiny C Compiler
│   ├── tcc.c
│   ├── libtcc.c
│   └── ...
├── dash/                   # POSIX shell (40 source files)
│   └── ...
├── sbase/                  # Portable Unix utilities (60+ tools)
│   ├── cat.c
│   ├── ls.c
│   ├── grep.c
│   └── ...
├── nextvi/                 # Vi editor port (41 files)
│   └── ...
├── klystrack/              # Chiptune tracker (separate build)
│   └── ...
├── sdl2/                   # SDL2 shim for VOS
│   └── ...
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
| Kernel C | ~39,000 |
| Kernel ASM | ~500 |
| Userland | ~51,000 |
| Third-party | ~100,000+ |
| Total (VOS code) | ~90,000+ |

> **Note**: These statistics include the core VOS kernel and userland programs. Third-party code (dash, TCC, nofrendo, sbase, etc.) is not counted in the VOS total but is included in the project.

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
