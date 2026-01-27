# Chapter 33: References and Resources

This chapter provides references for further learning about operating system development.

## Books

### Operating Systems

| Title | Author | Description |
|-------|--------|-------------|
| **Operating Systems: Three Easy Pieces** | Remzi & Andrea Arpaci-Dusseau | Free online textbook covering virtualization, concurrency, persistence |
| **Modern Operating Systems** | Andrew Tanenbaum | Comprehensive OS textbook |
| **Operating System Concepts** | Silberschatz et al. | Classic "dinosaur book" |
| **The Design of the UNIX Operating System** | Maurice Bach | Deep dive into UNIX internals |
| **Understanding the Linux Kernel** | Bovet & Cesati | Linux kernel internals |
| **Linux Kernel Development** | Robert Love | Practical Linux development |

### x86 Architecture

| Title | Author | Description |
|-------|--------|-------------|
| **Intel 64 and IA-32 Architectures Software Developer's Manual** | Intel | Official x86 reference |
| **AMD64 Architecture Programmer's Manual** | AMD | AMD64 reference |
| **PC Assembly Language** | Paul Carter | x86 assembly tutorial |
| **Programming from the Ground Up** | Jonathan Bartlett | Assembly introduction |

### C Programming

| Title | Author | Description |
|-------|--------|-------------|
| **The C Programming Language** | Kernighan & Ritchie | The classic K&R |
| **C: A Reference Manual** | Harbison & Steele | Comprehensive C reference |
| **Expert C Programming** | Peter van der Linden | Advanced C techniques |

## Online Resources

### Tutorials and Wikis

| Resource | URL | Description |
|----------|-----|-------------|
| **OSDev Wiki** | wiki.osdev.org | Comprehensive OS development wiki |
| **OSDev Forums** | forum.osdev.org | Community discussion |
| **JamesM's Kernel Tutorial** | jamesmolloy.co.uk/tutorial_html | Classic tutorial (some bugs) |
| **BrokenThorn OS Development** | brokenthorn.com/Resources | Detailed tutorials |
| **Writing an OS in Rust** | os.phil-opp.com | Modern OS in Rust |

### Specifications

| Specification | URL |
|---------------|-----|
| **Multiboot Specification** | gnu.org/software/grub/manual/multiboot |
| **ELF Specification** | refspecs.linuxfoundation.org/elf |
| **POSIX.1-2017** | pubs.opengroup.org/onlinepubs/9699919799 |
| **System V ABI** | refspecs.linuxfoundation.org |

### Reference Implementations

| Project | URL | Description |
|---------|-----|-------------|
| **xv6** | github.com/mit-pdos/xv6-public | MIT teaching OS |
| **Minix 3** | minix3.org | Microkernel OS |
| **Linux** | kernel.org | Production OS |
| **FreeBSD** | freebsd.org | BSD Unix |
| **SerenityOS** | serenityos.org | Modern hobbyist OS |
| **ToaruOS** | toaruos.org | Hobbyist Unix-like |

## Hardware Documentation

### Intel Manuals

Volume 1: Basic Architecture
- Processor architecture overview
- Execution environment
- Data types and addressing

Volume 2: Instruction Set Reference
- Complete instruction listing
- Encoding details

Volume 3: System Programming Guide
- Protected mode
- Interrupts and exceptions
- Memory management
- Task management

Download from: intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

### Device Documentation

| Device | Documentation |
|--------|---------------|
| 8259 PIC | wiki.osdev.org/8259_PIC |
| 8254 PIT | wiki.osdev.org/Programmable_Interval_Timer |
| PS/2 Keyboard | wiki.osdev.org/PS/2_Keyboard |
| PS/2 Mouse | wiki.osdev.org/PS/2_Mouse |
| VGA | wiki.osdev.org/VGA_Hardware |
| ATA/IDE | wiki.osdev.org/ATA_PIO_Mode |
| Serial Port | wiki.osdev.org/Serial_Ports |

## Tools Documentation

### GCC

- GCC Manual: gcc.gnu.org/onlinedocs
- Inline Assembly: gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
- Attributes: gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html

### Binutils

- LD Manual: sourceware.org/binutils/docs/ld
- AS Manual: sourceware.org/binutils/docs/as
- objdump, readelf, nm

### NASM

- NASM Manual: nasm.us/doc
- Instruction reference
- Macro system

### QEMU

- QEMU Documentation: qemu.org/documentation
- Monitor commands
- Device emulation

### GDB

- GDB Manual: sourceware.org/gdb/documentation
- Remote debugging
- Breakpoints and watchpoints

### GRUB

- GRUB Manual: gnu.org/software/grub/manual
- Configuration
- Module loading

## Third-Party Libraries

### Used in VOS

| Library | URL | Purpose |
|---------|-----|---------|
| **Newlib** | sourceware.org/newlib | C library |
| **TinyCC** | bellard.org/tcc | Native compiler |
| **sbase** | git.suckless.org/sbase | Unix utilities |
| **nextvi** | github.com/kyx0r/nextvi | Vim-like editor |
| **dash** | git.kernel.org/pub/scm/utils/dash/dash.git | POSIX shell |
| **olive.c** | github.com/tsoding/olive.c | 2D graphics |
| **small3dlib** | gitlab.com/drummyfish/small3dlib | 3D renderer |
| **linenoise** | github.com/antirez/linenoise | Line editing |
| **stb_image** | github.com/nothings/stb | Image loading |
| **jsmn** | github.com/zserge/jsmn | JSON parser |
| **microrl** | github.com/Helius/microrl | Line editing (kernel) |

### Alternative Libraries

| Library | URL | Purpose |
|---------|-----|---------|
| **PDCLib** | rootdirectory.de/dokuwiki/pdclib | Minimal C library |
| **musl** | musl.libc.org | Lightweight C library |
| **dietlibc** | fefe.de/dietlibc | Small C library |
| **uClibc-ng** | uclibc-ng.org | Embedded C library |

## Standards

### POSIX

- IEEE Std 1003.1-2017 (POSIX.1-2017)
- Shell and Utilities
- System Interfaces
- Base Definitions

### C Language

- ISO/IEC 9899:2018 (C18)
- ISO/IEC 9899:2011 (C11)
- ISO/IEC 9899:1999 (C99)

### File Formats

| Format | Specification |
|--------|---------------|
| ELF | Tool Interface Standard (TIS) |
| FAT | Microsoft FAT Specification |
| PSF2 | kbd project documentation |
| tar | POSIX.1 ustar format |

## Video Resources

### YouTube Channels

| Channel | Content |
|---------|---------|
| **Ben Eater** | Hardware and 6502 |
| **Tsoding Daily** | Live OS development |
| **Low Level Learning** | Systems programming |

### Courses

| Course | Platform |
|--------|----------|
| **CS 140** | Stanford (Pintos) |
| **6.828** | MIT (xv6) |
| **COMP 310** | McGill |

## Communities

### Forums

- OSDev Forums: forum.osdev.org
- Reddit r/osdev: reddit.com/r/osdev
- Stack Overflow [osdev] tag

### Chat

- OSDev Discord
- Libera.Chat #osdev

## Debugging Resources

### Common Problems

| Issue | Resource |
|-------|----------|
| Triple fault | wiki.osdev.org/Triple_Fault |
| Page fault | wiki.osdev.org/Exceptions#Page_Fault |
| GPF | wiki.osdev.org/Exceptions#General_Protection_Fault |
| Interrupt issues | wiki.osdev.org/I_Can't_Get_Interrupts_Working |

### Tools

| Tool | Purpose |
|------|---------|
| Bochs | x86 emulator with debugging |
| QEMU monitor | Runtime inspection |
| GDB | Source-level debugging |
| objdump | Binary inspection |
| nm | Symbol listing |

## Glossary

| Term | Definition |
|------|------------|
| **GDT** | Global Descriptor Table - defines memory segments |
| **IDT** | Interrupt Descriptor Table - defines interrupt handlers |
| **TSS** | Task State Segment - stores task context |
| **PIC** | Programmable Interrupt Controller |
| **PIT** | Programmable Interval Timer |
| **RTC** | Real-Time Clock |
| **ATA** | AT Attachment - disk interface |
| **VFS** | Virtual File System |
| **ELF** | Executable and Linkable Format |
| **MMU** | Memory Management Unit |
| **TLB** | Translation Lookaside Buffer |
| **IRQ** | Interrupt Request |
| **ISR** | Interrupt Service Routine |
| **DPL** | Descriptor Privilege Level |
| **CPL** | Current Privilege Level |
| **RPL** | Requested Privilege Level |

## Summary

This reference guide provides:

1. **Books** for in-depth learning
2. **Online resources** for practical guidance
3. **Specifications** for standards compliance
4. **Tools documentation** for development
5. **Third-party libraries** for functionality
6. **Communities** for support

Use these resources to deepen your understanding and extend VOS.

---

*Previous: [Chapter 32: Future Enhancements](32_future.md)*
*Next: [Chapter 34: Syscall Reference](34_syscall_reference.md)*
