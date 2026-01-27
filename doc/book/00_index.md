# VOS - Victor's Operating System
## A Comprehensive Guide to Building a Minimal Operating System

**Version:** 3.0 (Updated January 2026)

---

# Table of Contents

## Part I: Foundations

1. [Introduction](01_introduction.md)
   - What is an Operating System?
   - About VOS
   - Why Build an OS from Scratch?
   - **VOS Filesystem Structure** (runtime directories explained)

2. [Prerequisites and Tools](02_prerequisites.md)
   - Required Knowledge
   - Development Tools
   - Setting Up the Environment

3. [Understanding the Boot Process](03_boot_process.md)
   - The Boot Sequence
   - Real Mode vs Protected Mode
   - BIOS and UEFI

4. [The Multiboot Specification](04_multiboot.md)
   - What is Multiboot?
   - The Multiboot Header
   - What GRUB Provides

5. [Assembly Entry Point](05_assembly_entry.md)
   - The _start Function
   - Stack Setup
   - Transitioning to C

## Part II: Core Kernel

6. [Segmentation: GDT and TSS](06_gdt_tss.md)
   - Global Descriptor Table
   - Task State Segment
   - Privilege Levels

7. [Interrupts and Exceptions](07_interrupts.md)
   - Interrupt Descriptor Table
   - Exception Handling
   - The Syscall Interface

8. [Programmable Interrupt Controller](08_pic.md)
   - The 8259 PIC
   - Remapping IRQs
   - End of Interrupt

9. [Memory Management](09_memory.md)
   - Physical Memory Manager
   - Paging and Virtual Memory
   - Kernel Heap
   - Early Allocator

10. [Timekeeping](10_timekeeping.md)
    - Programmable Interval Timer
    - CMOS Real-Time Clock
    - Sleep and Uptime

## Part III: Drivers and I/O

11. [Console Output](11_console.md)
    - VGA Text Mode
    - Framebuffer Graphics
    - PSF2 Fonts
    - ANSI Escape Sequences
    - Status Bar

12. [Keyboard Driver](12_keyboard.md)
    - PS/2 Controller
    - Scancodes and Layouts
    - Input Buffer
    - Command History

13. [Mouse Driver](13_mouse.md)
    - PS/2 Mouse Protocol
    - Button and Movement Tracking
    - Xterm Mouse Sequences

14. [ATA Disk Driver](14_ata.md)
    - ATA/IDE Protocol
    - Sector Read/Write
    - LBA Addressing

15. [Serial Port](15_serial.md)
    - COM1 Configuration
    - Debug Logging
    - Panic Handling

## Part IV: File Systems

16. [Virtual File System](16_vfs.md)
    - VFS Architecture
    - Path Resolution
    - Mount Points
    - File Operations
    - **Overlay Aliases** (NEW: /etc, /home, /root overlays)

17. [RAM Filesystem](17_ramfs.md)
    - In-Memory Storage
    - Directory Tree
    - Initramfs Integration

18. [FAT16 Filesystem](18_fat16.md)
    - FAT Structure
    - Cluster Chains
    - Directory Entries
    - Long Filenames
    - Persistent Storage

## Part V: Process Management

19. [Tasking and Scheduling](19_tasking.md)
    - Task Structure
    - Context Switching
    - Round-Robin Scheduler
    - Task States

20. [User Mode and ELF](20_usermode.md)
    - Ring 3 Execution
    - ELF32 Loading
    - User Address Space
    - Stack and Heap

21. [System Calls](21_syscalls.md)
    - The Syscall Interface
    - Complete Syscall Reference (75+ syscalls)
    - Error Handling

22. [Signals and IPC](22_signals.md)
    - Signal Handling
    - Signal Delivery
    - Process Groups
    - Pipes

23. [Terminal I/O](23_termios.md)
    - Termios Structure
    - Canonical and Raw Mode
    - ioctl Operations
    - Window Size

## Part VI: POSIX Compliance

24. [POSIX Overview and VOS Compliance](24_posix.md)
    - What is POSIX?
    - POSIX Standards History
    - VOS POSIX Implementation Status
    - Supported Functions
    - Missing Features

## Part VII: User Space

25. [Shell and Commands](25_shell.md)
    - Kernel Shell
    - User Shell (dash - POSIX compliant)
    - Built-in Commands
    - External Utilities (~89 programs)

26. [Newlib Integration](26_newlib.md)
    - C Library Support
    - Syscall Stubs
    - Header Files

27. [Graphics Programming](27_graphics.md)
    - Framebuffer Access
    - Olive Graphics Library
    - Small3dlib
    - Demo Programs

28. [BASIC Interpreter](28_basic.md)
    - uBASIC Integration
    - Supported Features
    - Example Programs

## Part VIII: Development

29. [Build System](29_build.md)
    - Makefile Structure
    - Cross-Compilation
    - Toolchain Setup

30. [Testing with QEMU](30_testing.md)
    - QEMU Configuration
    - Debugging Techniques
    - Serial Output

31. [Project Structure](31_structure.md)
    - Directory Layout
    - File Organization
    - Code Statistics

## Part IX: Advanced Features

32. [Future Enhancements](32_future.md)
    - Planned Features
    - Known Limitations
    - Contributing

33. [Resources and References](33_references.md)
    - OSDev Wiki
    - Intel Manuals
    - Related Projects

34. [Syscall Quick Reference](34_syscall_reference.md)
    - Complete Syscall Table
    - Register Conventions
    - Error Codes

35. [MCP Server for LLM Integration](35_mcp_server.md)
    - Overview and Installation
    - Available Tools (start, exec, screenshot, upload)
    - LLM Workflow Guide
    - Writing Code with Heredoc
    - Architecture and Troubleshooting

36. [Emoji Support](36_emoji.md)
    - Supported Emoji (89 color emoji)
    - Implementation Details
    - Alpha Blending and Double-Width Cells
    - Using Emoji in Programs
    - Generating Emoji Data

## Part X: Audio and Multimedia

37. [**Sound Blaster 16 Audio**](37_sound_blaster.md) (NEW)
    - SB16 Hardware Architecture
    - DSP Programming and Commands
    - DMA (Direct Memory Access) Transfers
    - Audio Format Configuration
    - IRQ Handling
    - Mixer and Volume Control
    - MOD and MIDI Players

## Part XI: Customization

38. [**Font System and Themes**](38_fonts.md) (NEW)
    - PSF2 Font Format
    - Multiple Built-in Fonts
    - Runtime Font Switching
    - Font Selection by Resolution
    - Color Themes
    - The `font` and `theme` Utilities
    - Persistent Preferences

39. [**User Management and Login**](39_users.md) (NEW)
    - The passwd File Format
    - VFS Overlay for /etc
    - The login Program
    - Privilege Separation (UID 0 vs regular)
    - Password Authentication
    - Home Directories
    - Shell Profiles (/etc/profile)
    - Security Considerations

40. [**Virtual Consoles**](40_vconsoles.md) (NEW)
    - Multiple Terminal Sessions
    - Alt+1/2/3/4 Switching
    - Per-Console State
    - Process-to-Console Mapping
    - Keyboard Input Routing
    - Status Bar Integration

---

## About This Documentation

This book was generated from the VOS source code and provides a comprehensive guide to understanding and extending the operating system. Each chapter includes:

- Conceptual explanations
- Code examples from VOS
- Implementation details
- Practical exercises

## VOS Statistics

| Metric | Value |
|--------|-------|
| **Total Syscalls** | 75+ |
| **Kernel Size** | ~25,000 lines of C |
| **User Programs** | ~89 utilities |
| **POSIX Compliance** | ~50% |
| **Supported Fonts** | 5 built-in |
| **Virtual Consoles** | 4 |
| **Audio Support** | Sound Blaster 16 |

## Key Features

- **Multiboot-compliant** kernel bootable via GRUB
- **Framebuffer graphics** with multiple font sizes
- **FAT16 persistent storage** with overlay filesystem
- **Multi-user authentication** with privilege separation
- **~89 userland utilities** including vi editor, emulators, and tools
- **Sound Blaster 16** audio driver with DMA
- **MCP integration** for LLM-assisted development
- **Emoji support** with alpha blending

## Quick Start

```bash
# Build VOS
make

# Run in QEMU
make run

# Login as root (no password)
root

# Or login as victor (no password)
victor

# Try some commands
neofetch
ls /bin
font
top
```

---

*Last Updated: January 2026*
*VOS Version: 3.0*
