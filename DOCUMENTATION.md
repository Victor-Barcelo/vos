# VOS - Victor's Operating System
## A Comprehensive Guide to Building a Minimal Operating System

---

# Table of Contents

1. [Introduction](#1-introduction)
2. [Prerequisites and Tools](#2-prerequisites-and-tools)
3. [Understanding the Boot Process](#3-understanding-the-boot-process)
4. [The Multiboot Specification](#4-the-multiboot-specification)
5. [Assembly Entry Point](#5-assembly-entry-point)
6. [Protected Mode, Paging, and Memory](#6-protected-mode-paging-and-memory)
7. [Segmentation: GDT and TSS](#7-segmentation-gdt-and-tss)
8. [Interrupts, Exceptions, and Syscalls](#8-interrupts-exceptions-and-syscalls)
9. [Programmable Interrupt Controller (PIC)](#9-programmable-interrupt-controller-pic)
10. [Console Output: VGA and Framebuffer](#10-console-output-vga-and-framebuffer)
11. [PS/2 Keyboard Driver](#11-ps2-keyboard-driver)
12. [Kernel Initialization and Subsystems](#12-kernel-initialization-and-subsystems)
13. [Shell, Status Bar, and UX](#13-shell-status-bar-and-ux)
14. [Integrating a BASIC Interpreter](#14-integrating-a-basic-interpreter)
15. [Timekeeping (PIT + RTC)](#15-timekeeping-pit--rtc)
16. [Serial Logging and Panic](#16-serial-logging-and-panic)
17. [Memory Management (early_alloc, PMM, paging, kheap)](#17-memory-management-early_alloc-pmm-paging-kheap)
18. [initramfs and VFS](#18-initramfs-and-vfs)
19. [Tasking, Syscalls, User Mode, and ELF](#19-tasking-syscalls-user-mode-and-elf)
20. [Build System and Toolchain](#20-build-system-and-toolchain)
21. [Testing with QEMU and VirtualBox](#21-testing-with-qemu-and-virtualbox)
22. [Project Structure](#22-project-structure)
23. [Future Enhancements](#23-future-enhancements)
24. [Resources and References](#24-resources-and-references)

---

# 1. Introduction

## What is an Operating System?

An operating system (OS) is the fundamental software that manages computer hardware and provides services for programs. At its core, an OS handles:

- **Hardware Abstraction**: Providing a consistent interface to diverse hardware
- **Process Management**: Running and scheduling programs
- **Memory Management**: Allocating and protecting memory
- **I/O Management**: Handling input/output devices
- **File Systems**: Organizing and storing data

## About VOS

VOS (Victor's Operating System) is a minimal, educational operating system designed to demonstrate fundamental OS concepts. It features:

- **32-bit x86 architecture** (i386)
- **Multiboot-compliant** bootloader support (works with GRUB)
- **VGA text mode + Multiboot framebuffer console** (high resolution with PSF2 fonts)
- **Blue/white terminal UI**, safe-area padding, and a bottom status bar
- **Serial logging** (COM1) for debugging
- **Panic + exception reporting** (register dump, page-fault CR2)
- **PIT timer** (uptime, sleep, scheduler tick) and **CMOS RTC** (date/time, set date)
- **Paging + physical memory manager (PMM)** and a simple **kernel heap**
- **initramfs (tar) module + in-memory VFS** (ls/cat)
- **PS/2 keyboard** driver with interrupt handling
- **Spanish keyboard layout** with AltGr support
- **Command history** (up/down arrow navigation)
- **Simple shell** with built-in commands (including `run` for user-mode apps)
- **GDT + TSS**, basic **syscalls** (`int 0x80`), **round-robin tasking**, and **user mode**
- **ELF32 loader** and a tiny user-space `/bin/init`
- **uBASIC interpreter** with 10 example programs

## Why Build an OS from Scratch?

Building an operating system teaches you:

1. How computers actually work at the lowest level
2. Memory management and addressing
3. Hardware communication via ports and interrupts
4. Assembly language and its relationship to C
5. The boot process from power-on to running code
6. Driver development concepts
7. Systems programming techniques

---

# 2. Prerequisites and Tools

## Required Knowledge

Before diving into OS development, you should understand:

- **C Programming**: Pointers, memory management, structs
- **Basic Assembly**: x86 instructions, registers, stack operations
- **Binary/Hexadecimal**: Number systems and bit manipulation
- **Computer Architecture**: CPU, memory, buses, I/O

## Development Tools

### Assembler: NASM
```bash
sudo apt install nasm
```
NASM (Netwide Assembler) assembles our boot code and interrupt handlers.

### Compiler: GCC (32-bit)
```bash
sudo apt install gcc
```
We use GCC with special flags for freestanding (no standard library) compilation.

### Linker: LD
```bash
sudo apt install binutils
```
The GNU linker combines our object files into a single kernel binary.

### ISO Creation Tools
```bash
sudo apt install grub-pc-bin xorriso mtools
```
These tools create bootable ISO images using GRUB as the bootloader.

### Virtualization
```bash
sudo apt install virtualbox
# or
sudo apt install qemu-system-x86
```
Virtual machines let us test without rebooting real hardware.

---

# 3. Understanding the Boot Process

## The Boot Sequence

When you power on a computer, the following sequence occurs:

```
┌─────────────────────────────────────────────────────────────────┐
│  1. POWER ON                                                     │
│     └─> CPU starts in Real Mode (16-bit)                        │
│         └─> Executes code at address 0xFFFF0 (BIOS ROM)         │
│                                                                  │
│  2. BIOS POST (Power-On Self Test)                              │
│     └─> Tests RAM, detects hardware                             │
│         └─> Initializes basic hardware                          │
│                                                                  │
│  3. BIOS BOOT                                                   │
│     └─> Reads first sector (512 bytes) from boot device         │
│         └─> Loads it to address 0x7C00                          │
│             └─> Jumps to 0x7C00 to execute bootloader           │
│                                                                  │
│  4. BOOTLOADER (GRUB in our case)                               │
│     └─> Switches CPU to Protected Mode (32-bit)                 │
│         └─> Loads kernel into memory                            │
│             └─> Jumps to kernel entry point                     │
│                                                                  │
│  5. KERNEL                                                       │
│     └─> Initializes hardware (IDT, PIC, drivers)                │
│         └─> Starts the shell                                    │
└─────────────────────────────────────────────────────────────────┘
```

## Real Mode vs Protected Mode

### Real Mode (16-bit)
- CPU starts in this mode
- Can only access 1 MB of memory
- No memory protection
- Segmented memory model: `Physical = Segment * 16 + Offset`

### Protected Mode (32-bit)
- Can access up to 4 GB of memory
- Memory protection via segmentation and paging
- Privilege levels (Ring 0-3)
- Flat memory model possible

GRUB handles the transition to Protected Mode for us, which simplifies our kernel significantly.

---

# 4. The Multiboot Specification

## What is Multiboot?

Multiboot is a specification that defines how a bootloader loads an operating system kernel. By following this specification, our kernel can be loaded by any Multiboot-compliant bootloader (like GRUB).

## The Multiboot Header

Every Multiboot kernel must have a header in its first 8192 bytes:

```
┌────────────────────────────────────────┐
│  Offset   │  Field      │  Value       │
├───────────┼─────────────┼──────────────┤
│  0        │  Magic      │  0x1BADB002  │
│  4        │  Flags      │  (see below) │
│  8        │  Checksum   │  -(magic+f)  │
└────────────────────────────────────────┘
```

### Implementation in boot.asm

```nasm
; Multiboot header constants
MBALIGN  equ 1 << 0            ; Align modules on page boundaries
MEMINFO  equ 1 << 1            ; Provide memory map
FLAGS    equ MBALIGN | MEMINFO ; Our flags
MAGIC    equ 0x1BADB002        ; Multiboot magic number
CHECKSUM equ -(MAGIC + FLAGS)  ; Must sum to zero

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
```

### Flags Explained

| Bit | Name | Description |
|-----|------|-------------|
| 0 | MBALIGN | Align loaded modules on 4KB boundaries |
| 1 | MEMINFO | Request memory map from bootloader |
| 2 | VIDEO | Request specific video mode |

## What GRUB Provides

When GRUB loads our kernel, it:

1. Switches to 32-bit Protected Mode
2. Sets up a basic GDT (Global Descriptor Table)
3. Disables interrupts
4. Loads our kernel at the address specified in the linker script
5. Puts the Multiboot magic (0x2BADB002) in EAX
6. Puts a pointer to the Multiboot info structure in EBX
7. Jumps to our entry point

---

# 5. Assembly Entry Point

## The _start Function

Our kernel's entry point is written in assembly because we need precise control over the initial CPU state.

```nasm
section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot info pointer and magic number
    push ebx                    ; Multiboot info structure
    push eax                    ; Multiboot magic number

    ; Call the kernel main function
    call kernel_main

    ; If kernel returns, hang the system
    cli                         ; Disable interrupts
.hang:
    hlt                         ; Halt CPU until interrupt
    jmp .hang                   ; Loop forever
```

## Stack Setup

The stack grows downward in x86. We reserve 16 KB for our stack:

```nasm
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB stack
stack_top:
```

### Why align to 16 bytes?
The System V ABI (Application Binary Interface) requires the stack to be 16-byte aligned before a `call` instruction. This ensures proper alignment for SSE instructions and function calls.

## Understanding the Sections

| Section | Purpose |
|---------|---------|
| `.multiboot` | Contains the Multiboot header (must be first) |
| `.text` | Executable code |
| `.rodata` | Read-only data (constants, strings) |
| `.data` | Initialized read-write data |
| `.bss` | Uninitialized data (stack, buffers) |

---

# 6. Protected Mode, Paging, and Memory

This chapter is about how VOS lays out memory and how the CPU turns addresses into bytes. It starts with a conceptual overview (what you need to *think correctly* about memory), then points at the concrete VOS implementations (expanded in §17).

## Physical vs virtual memory (one-minute intuition)

- **Physical memory** is the actual RAM address seen by the memory controller.
- **Virtual memory** is what the CPU generates when it executes instructions (addresses inside pointers).
- Paging is the translation layer between the two.

In early boot, VOS keeps things simple and uses **identity mappings** for most boot-critical regions (virtual address == physical address). Once the system is up, it also maps higher virtual regions like the kernel heap.

## Memory Layout

In Protected Mode with a flat memory model, memory appears as a continuous 4 GB address space:

```
┌─────────────────────────────────────┐ 0xFFFFFFFF (4 GB)
│                                     │
│         (Unmapped/Reserved)         │
│                                     │
├─────────────────────────────────────┤
│                                     │
│         Available Memory            │
│                                     │
├─────────────────────────────────────┤ ~0x00100000 (1 MB)
│  BIOS Data, Video Memory, etc.      │
├─────────────────────────────────────┤ 0x000B8000
│  VGA Text Buffer (4000 bytes)       │
├─────────────────────────────────────┤ 0x000A0000
│  Conventional Memory                │
├─────────────────────────────────────┤ 0x00007E00
│  Bootloader (512 bytes)             │
├─────────────────────────────────────┤ 0x00007C00
│  BIOS Data Area                     │
└─────────────────────────────────────┘ 0x00000000
```

This diagram is a useful *mental model*, but remember that once paging is enabled, many virtual addresses may not map to physical RAM at all (or may map to different physical addresses).

## Our Kernel's Memory Map

Our linker script places the kernel at 1 MB:

```
┌─────────────────────────────────────┐
│  .multiboot    │  Multiboot header  │ 0x00100000
├────────────────┼────────────────────┤
│  .text         │  Code              │
├────────────────┼────────────────────┤
│  .rodata       │  Constants         │
├────────────────┼────────────────────┤
│  .data         │  Initialized data  │
├────────────────┼────────────────────┤
│  .bss          │  Stack, buffers    │
└────────────────┴────────────────────┘
```

VOS also defines a linker symbol `__kernel_end` (the first address after the kernel image). This symbol matters because early boot allocators need a “safe starting point” that won’t overwrite the kernel itself.

## The Linker Script

The linker script (`linker.ld`) tells the linker how to arrange our sections:

```ld
/* Linker script for VOS kernel */

ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1MB mark (standard for multiboot) */
    . = 1M;

    /* Multiboot header must be first */
    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
    }

    /* Text section (code) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    __kernel_end = .;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}
```

### Why the linker script matters in kernel work

Unlike user programs, kernels are “freestanding”: there is no host OS to load you, relocate you, or guarantee any runtime layout. Your linker script is part of your boot protocol.

In VOS, the script ensures:

- the Multiboot header exists and stays at a predictable place
- the kernel is aligned to page boundaries (useful for paging)
- the kernel has a well-defined end (`__kernel_end`) so early boot code can safely allocate memory

## Paging: the conceptual model (VOS-specific details in §17)

Once paging is enabled, every memory access goes through page tables.

In 32-bit x86 paging with 4 KiB pages:

```
virtual addr bits:  [31..22] [21..12] [11..0]
                    PDE idx  PTE idx  offset
```

- Page directory: 1024 entries → selects a page table
- Page table: 1024 entries → selects a 4 KiB physical frame
- Offset: 12 bits → byte offset within the page

Common flag meanings (as used by VOS in `include/paging.h`):

- `PAGE_PRESENT`: the entry is valid
- `PAGE_RW`: writable (otherwise read-only)
- `PAGE_USER`: accessible from ring 3 (otherwise supervisor-only)

VOS enables paging early in `kernel/paging.c`:

1. Build a page directory + page tables (allocated from `early_alloc`)
2. Identity-map early boot regions (kernel + Multiboot structures + framebuffer)
3. Load `CR3` with the page directory physical address
4. Set `CR0.PG` to turn paging on

Once paging is on, the page-fault exception (interrupt 14) becomes a central debugging tool: `CR2` tells you *which virtual address* faulted, and the error code tells you *why* (present vs not present, read vs write, user vs supervisor).

---

# 7. Segmentation: GDT and TSS

## What is the GDT?

The Global Descriptor Table defines memory segments in Protected Mode. Each entry describes:

- Base address of the segment
- Limit (size) of the segment
- Access rights and flags

## GDT Structure

```
┌────────────────────────────────────────────────────────────────┐
│ 63-56 │ 55-52 │ 51-48 │ 47-40 │ 39-32 │ 31-16 │ 15-0         │
├───────┼───────┼───────┼───────┼───────┼───────┼──────────────┤
│ Base  │ Flags │ Limit │Access │ Base  │ Base  │ Limit        │
│ 31-24 │       │ 19-16 │       │ 23-16 │ 15-0  │ 15-0         │
└───────┴───────┴───────┴───────┴───────┴───────┴──────────────┘
```

## VOS GDT layout (Ring 0 + Ring 3)

While GRUB enters 32-bit protected mode for us, VOS installs its **own** GDT early in boot (see `kernel/gdt.c` + `boot/boot.asm`). This gives us:

- A consistent **kernel** code/data pair (ring 0)
- A **user** code/data pair (ring 3)
- A **TSS descriptor** so the CPU can switch stacks on privilege changes

VOS uses a flat segmentation model (base = 0, limit = 4 GiB) and relies on paging for page-level permissions.

### Selectors used by VOS

| Selector | Meaning |
|----------|---------|
| `0x08` | Kernel code segment (ring 0) |
| `0x10` | Kernel data segment (ring 0) |
| `0x1B` | User code segment (ring 3) |
| `0x23` | User data segment (ring 3) |
| `0x28` | TSS selector |

The low 2 bits of a selector are the **RPL** (Requested Privilege Level). That’s why user selectors end in `...3` (ring 3).

## Task State Segment (TSS): kernel stack switching

When a user-mode task (ring 3) executes a syscall (`int 0x80`) or takes an interrupt, the CPU must transition to ring 0. That transition requires a **trusted kernel stack**.

The x86 mechanism for this is the TSS fields:

- `SS0`: the kernel data selector to use for the stack
- `ESP0`: the kernel stack pointer to load on entry to ring 0

VOS updates `ESP0` on every task switch (`tss_set_kernel_stack()`), so each task gets its own kernel stack during syscalls/interrupts.

---

# 8. Interrupts, Exceptions, and Syscalls

## What are Interrupts?

Interrupts are signals that temporarily stop the CPU to handle events:

### Types of Interrupts

1. **Hardware Interrupts (IRQs)**: From devices (keyboard, timer, disk)
2. **Software Interrupts**: Triggered by `int` instruction
3. **Exceptions**: CPU errors (divide by zero, page fault)

## The Interrupt Descriptor Table (IDT)

The IDT is an array of 256 entries, each describing how to handle an interrupt:

```
┌─────────────────────────────────────────────────────────────┐
│  Entry  │  Type           │  Description                    │
├─────────┼─────────────────┼─────────────────────────────────┤
│  0-31   │  Exceptions     │  CPU errors (divide, page fault)│
│  32-47  │  Hardware IRQs  │  Remapped PIC interrupts        │
│  48-255 │  Software/User  │  Available for OS use           │
└─────────┴─────────────────┴─────────────────────────────────┘
```

## IDT Entry Structure

Each IDT entry is 8 bytes:

```c
struct idt_entry {
    uint16_t base_low;    // Handler address bits 0-15
    uint16_t selector;    // Code segment selector
    uint8_t  zero;        // Always zero
    uint8_t  flags;       // Type and attributes
    uint16_t base_high;   // Handler address bits 16-31
} __attribute__((packed));
```

### Flags Field

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │
├─────┼─────┴─────┼─────┼─────┴─────┴─────┴─────┤
│  P  │   DPL     │  0  │        Type           │
└─────┴───────────┴─────┴───────────────────────┘

P (Present):     1 = Entry is valid
DPL:             Privilege level (0 = kernel)
Type:            0xE = 32-bit interrupt gate
                 0xF = 32-bit trap gate
```

VOS uses two main gate configurations:

- **Kernel-only interrupt gates**: flags = `0x8E` (P=1, DPL=0, type=0xE)
- **Syscall gate (user callable)**: flags = `0xEE` (P=1, DPL=3, type=0xE)

## Setting Up the IDT

```c
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    // Default handler for everything.
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default, cs, 0x8E);
    }

    // CPU exceptions (0..31) and PIC IRQs (32..47).
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], cs, 0x8E);
    }

    pic_remap();

    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, irq_stub_table[i], cs, 0x8E);
    }

    // Syscall gate (int 0x80) - callable from ring 3 (DPL=3).
    idt_set_gate(0x80, (uint32_t)isr128, cs, 0xEE);

    // Unmask timer+keyboard (+cascade), mask everything else.
    outb(0x21, 0xF8);
    outb(0xA1, 0xFF);

    idt_flush((uint32_t)&idtp);
}
```

In the real code (`kernel/idt.c`), exception and IRQ vectors point at tables of assembly stubs generated in `boot/boot.asm`.

## Unified assembly stubs + an interrupt frame

VOS uses a single “common stub” to build a consistent stack frame and call a C handler:

```nasm
; Pseudocode for the common path:
isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    ; Ensure we run C code with kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                 ; &interrupt_frame
    call interrupt_handler   ; returns (possibly new) frame pointer
    add esp, 4
    mov esp, eax             ; context switch happens here

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8               ; pop int_no + err_code
    iret
```

## The interrupt frame in C

The pointer passed to `interrupt_handler()` points at a C struct representing the saved CPU state (see `include/interrupts.h`). This is what makes debugging (panic dumps), syscalls, and task switching possible.

Simplified shape:

```c
typedef struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
    // If the interrupt came from ring 3, the CPU also pushed user ESP + SS
    // below these fields (so iret can return to user mode safely).
} interrupt_frame_t;
```

### Dispatch model

The top-level dispatcher (`kernel/interrupts.c`) is intentionally simple:

- **Exceptions (0..31)** → `panic_with_frame()` with a name table
- **Syscalls (0x80)** → `syscall_handle(frame)`
- **IRQs (32..47)** → optional per-IRQ handler + PIC EOI
  - IRQ0 additionally calls the scheduler tick and may return a different frame (preemption)

Because `interrupt_handler()` returns a frame pointer, the scheduler can context-switch by returning another task’s saved frame pointer.

### The IRET Instruction

`iret` (Interrupt Return) pops the following from the stack:
1. EIP (instruction pointer)
2. CS (code segment)
3. EFLAGS (flags register)

If the return privilege level changes (e.g. returning to ring 3), `iret` also pops:

4. ESP (user stack pointer)
5. SS (user stack segment)

---

# 9. Programmable Interrupt Controller (PIC)

## What is the PIC?

The 8259 PIC is a chip that manages hardware interrupts. PCs have two PICs in a cascade configuration:

```
┌─────────────────┐     ┌─────────────────┐
│   Master PIC    │     │    Slave PIC    │
│   (Port 0x20)   │     │   (Port 0xA0)   │
├─────────────────┤     ├─────────────────┤
│ IRQ0: Timer     │     │ IRQ8:  RTC      │
│ IRQ1: Keyboard  │     │ IRQ9:  ACPI     │
│ IRQ2: Cascade ──┼─────┤ IRQ10: Free     │
│ IRQ3: COM2      │     │ IRQ11: Free     │
│ IRQ4: COM1      │     │ IRQ12: PS/2 Mou │
│ IRQ5: LPT2      │     │ IRQ13: FPU      │
│ IRQ6: Floppy    │     │ IRQ14: Primary  │
│ IRQ7: LPT1      │     │ IRQ15: Second   │
└─────────────────┘     └─────────────────┘
```

## PIC Remapping

By default, the PIC sends IRQ 0-7 to interrupts 8-15, which conflicts with CPU exceptions. We remap them:

| IRQ | Default INT | Remapped INT |
|-----|-------------|--------------|
| 0-7 | 8-15 | 32-39 |
| 8-15 | 112-119 | 40-47 |

### Remapping Code

```c
static void pic_remap(void) {
    // Save masks
    uint8_t mask1 = inb(0x21);
    uint8_t mask2 = inb(0xA1);

    // Start initialization sequence (ICW1)
    outb(0x20, 0x11);  // Master
    io_wait();
    outb(0xA0, 0x11);  // Slave
    io_wait();

    // Set vector offsets (ICW2)
    outb(0x21, 0x20);  // Master: IRQ 0-7 -> INT 32-39
    io_wait();
    outb(0xA1, 0x28);  // Slave: IRQ 8-15 -> INT 40-47
    io_wait();

    // Set cascade (ICW3)
    outb(0x21, 0x04);  // Master: slave at IRQ2
    io_wait();
    outb(0xA1, 0x02);  // Slave: cascade identity
    io_wait();

    // Set 8086 mode (ICW4)
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Restore masks
    outb(0x21, mask1);
    outb(0xA1, mask2);
}
```

## IRQ Masking

We can enable/disable specific IRQs by setting bits in the mask register:

```c
// Enable only keyboard (IRQ1)
outb(0x21, 0xFD);  // 11111101 - bit 1 clear = IRQ1 enabled
outb(0xA1, 0xFF);  // All slave IRQs disabled
```

## End of Interrupt (EOI)

After handling an interrupt, we must send EOI to the PIC:

```c
outb(0x20, 0x20);  // EOI to master PIC
// If IRQ >= 8, also send to slave:
// outb(0xA0, 0x20);
```

---

# 10. Console Output: VGA and Framebuffer

VOS supports **two** text output backends:

1. **VGA text mode** (legacy): direct writes to the VGA text buffer at `0xB8000`
2. **Multiboot linear framebuffer** (modern-ish): a pixel framebuffer provided by GRUB/QEMU, where VOS renders font glyphs into pixels

At boot, `screen_init(multiboot_magic, mboot_info)` selects the framebuffer backend when available (Multiboot flag 12, direct RGB mode, supported bpp). Otherwise it falls back to VGA text mode.

VOS also applies a small **safe-area padding** (a 1-cell margin) and reserves the bottom row for the status bar.

## VGA Text Buffer

The VGA text mode buffer is located at physical address 0xB8000. It's a 2D array of character cells:

```
┌────────────────────────────────────────────────────────┐
│  Address: 0xB8000                                      │
│  Size: 80 columns × 25 rows × 2 bytes = 4000 bytes     │
│                                                        │
│  Each cell: [Character Byte][Attribute Byte]           │
└────────────────────────────────────────────────────────┘
```

## Character Cell Format

Each cell is 2 bytes:

```
Byte 0: ASCII character code
Byte 1: Attribute (colors)

Attribute byte:
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │
├─────┼─────┴─────┴─────┼─────┴─────┴─────┴─────┤
│Blink│   Background    │     Foreground        │
└─────┴─────────────────┴───────────────────────┘
```

## VGA Colors

```c
enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
};
```

## Screen Driver Implementation

### Creating a VGA Entry

```c
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}
```

### Printing a Character

The real implementation in `kernel/screen.c` supports both VGA text mode and the Multiboot framebuffer backend. The snippet below shows the high-level flow (dynamic cols/rows, safe-area padding, reserved bottom rows, and serial mirroring):

```c
void screen_putchar(char c) {
    int height = usable_height();      // rows minus reserved bottom rows

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            // Clear the previous cell in the active backend...
        }
    } else {
        // Write (c, current_color) into the active backend...
        cursor_x++;
    }

    if (cursor_x >= screen_cols_value) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= height) {
        // vga_scroll() or fb_scroll()
    }

    update_cursor();                   // VGA hardware cursor or framebuffer overlay
    serial_write_char(c);              // mirror output for debugging/logging
}
```

### Scrolling

```c
static void vga_scroll(void) {
    int height = usable_height();
    int phys_top = pad_top_rows;

    for (int y = 0; y < height - 1; y++) {
        int dst_y = phys_top + y;
        int src_y = phys_top + y + 1;
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
        }
    }

    int last_y = phys_top + (height - 1);
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[last_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_y = height - 1;
}
```

### Hardware Cursor

The VGA controller has a hardware cursor that we can control:

```c
static void vga_hw_cursor_update(void) {
    int phys_x = cursor_x + pad_left_cols;
    int phys_y = cursor_y + pad_top_rows;
    uint16_t pos = (uint16_t)(phys_y * VGA_WIDTH + phys_x);

    outb(0x3D4, 0x0F);           // Cursor location low register
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);           // Cursor location high register
    outb(0x3D5, (pos >> 8) & 0xFF);
}
```

## Multiboot framebuffer console (high resolution)

In framebuffer mode, the bootloader provides a **linear pixel buffer** plus metadata (width/height/pitch/bpp and RGB bitfield layout). This is ideal for higher resolutions and non-80×25 layouts.

### Color mapping

VOS keeps the classic 16-color VGA palette and maps it to framebuffer pixels:

- VGA-style color attribute: `fg | (bg << 4)`
- Palette index → RGB triplet
- RGB triplet → packed pixel based on Multiboot’s reported bit positions/sizes

This allows the same UI color scheme to work in both VGA and framebuffer backends.

### Fonts (PSF2)

In framebuffer mode, VOS renders characters using embedded **PSF2** fonts:

- A PSF2 parser validates the header and exposes glyph bytes (`kernel/font_psf2.c`)
- VOS selects a font based on resolution (e.g. larger fonts for 1024×768+)
- Fonts are stored under `third_party/fonts/` and embedded as byte arrays

### Text grid + rendering

Instead of writing `uint16_t` cells into VGA memory, VOS maintains a cell buffer (a “virtual text mode”):

- Each cell contains `(character, color attribute)`
- When a cell changes, VOS redraws it by painting the glyph into the framebuffer
- Scrolling is implemented by shifting the cell buffer and copying pixel rows

### Cursor, status bar, and serial mirroring

- In VGA mode, the cursor uses the VGA hardware cursor registers.
- In framebuffer mode, the cursor is handled by the screen layer (so it can blink consistently).
- The status bar uses `screen_set_reserved_bottom_rows(1)` so shell output never scrolls over it.
- For debugging, `screen_putchar()` mirrors all text output to the serial port.

---

# 11. PS/2 Keyboard Driver

## PS/2 Controller

The PS/2 keyboard connects through the 8042 controller:

| Port | Read | Write |
|------|------|-------|
| 0x60 | Read data | Write data |
| 0x64 | Read status | Write command |

## Scancodes

When a key is pressed, the keyboard sends a scancode. When released, it sends the same scancode with bit 7 set (scancode | 0x80).

### Scancode Set 1 (Default)

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ Esc │  1  │  2  │  3  │  4  │  5  │  6  │  7  │  8  │  9  │
│ 01  │ 02  │ 03  │ 04  │ 05  │ 06  │ 07  │ 08  │ 09  │ 0A  │
├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
│  0  │  -  │  =  │ BS  │ Tab │  Q  │  W  │  E  │  R  │  T  │
│ 0B  │ 0C  │ 0D  │ 0E  │ 0F  │ 10  │ 11  │ 12  │ 13  │ 14  │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
```

## Extended Scancodes

Some keys send a prefix byte (0xE0) followed by the scancode:

| Key | Scancodes |
|-----|-----------|
| Up Arrow | E0 48 |
| Down Arrow | E0 50 |
| Left Arrow | E0 4B |
| Right Arrow | E0 4D |
| Right Alt (AltGr) | E0 38 |

## Keyboard Handler

```c
void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Check for extended key prefix (0xE0).
    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }

    if (extended_key) {
        extended_key = false;

        // Extended keys (arrows, AltGr, etc.)
        if (!(scancode & 0x80)) {  // key press
            switch (scancode) {
                case 0x48: buffer_push(KEY_UP);    break;
                case 0x50: buffer_push(KEY_DOWN);  break;
                case 0x4B: buffer_push(KEY_LEFT);  break;
                case 0x4D: buffer_push(KEY_RIGHT); break;
                case 0x38: altgr_pressed = true;   break;
            }
        } else {
            if ((scancode & 0x7F) == 0x38) {
                altgr_pressed = false;
            }
        }
        return;
    }

    // Regular scancodes:
    // - update modifier key state (shift/ctrl/caps)
    // - map scancode -> ASCII using ES layout tables (+ AltGr)
    // - push resulting characters into the circular buffer
    //
    // Note: PIC EOI is sent by the common IRQ handler in `kernel/interrupts.c`.
}
```

## Spanish Keyboard Layout

The Spanish keyboard layout differs from US layout:

```
US:  1 2 3 4 5 6 7 8 9 0 - =
ES:  1 2 3 4 5 6 7 8 9 0 ' ¡

Shifted:
US:  ! @ # $ % ^ & * ( ) _ +
ES:  ! " # $ % & / ( ) = ? ¿

AltGr combinations:
AltGr+2 = @
AltGr+3 = #
AltGr+7 = {
AltGr+8 = [
AltGr+9 = ]
AltGr+0 = }
```

## Input Buffer

We use a circular buffer to store keypresses:

```c
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_start = 0;
static volatile size_t buffer_end = 0;

static void buffer_push(char c) {
    size_t next = (buffer_end + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_start) {  // Buffer not full
        keyboard_buffer[buffer_end] = c;
        buffer_end = next;
    }
}

static char buffer_pop(void) {
    if (buffer_start == buffer_end) {
        return 0;  // Buffer empty
    }
    char c = keyboard_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
```

## Command History

We implement command history for the shell:

```c
#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][256];
static int history_count = 0;

void keyboard_history_add(const char* cmd) {
    if (cmd[0] == '\0') return;

    // Don't add duplicates
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_SIZE;
        if (strcmp(history[last], cmd) == 0) return;
    }

    int idx = history_count % HISTORY_SIZE;
    strcpy(history[idx], cmd);
    history_count++;
}
```

---

# 12. Kernel Initialization and Subsystems

## Kernel Entry Point (C)

```c
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    // Early debug output.
    serial_init();

    // Console (VGA text or Multiboot framebuffer).
    screen_init(magic, mboot_info);

    // Boot-time allocators and mappings.
    early_alloc_init(compute_early_start(__kernel_end, (multiboot_info_t*)mboot_info));
    paging_init((multiboot_info_t*)mboot_info);
    pmm_init(magic, (multiboot_info_t*)mboot_info, (uint32_t)&__kernel_end);
    kheap_init();
    vfs_init((multiboot_info_t*)mboot_info);

    // CPU/system info, segmentation, interrupts, timer, drivers.
    system_init(magic, mboot_info);
    gdt_init();
    tss_set_kernel_stack((uint32_t)&stack_top);
    idt_init();
    timer_init(1000);
    irq_register_handler(1, keyboard_irq_handler);
    keyboard_init();

    // Tasking + user init.
    tasking_init();
    sti();
    try_start_init();     // loads /bin/init from initramfs

    // Shell runs in the boot task.
    shell_run();

    // If the shell ever returns, halt forever.
    cli();
    for (;;) { hlt(); }
}
```

### Why the initialization order matters

- **`serial_init()` first**: if the screen path breaks, serial still captures logs.
- **`screen_init()` early**: the rest of boot can print progress and errors.
- **Paging before PMM/heap**: page tables are allocated early and paging is enabled before higher-level allocators.
- **PMM before kheap**: the heap grows by mapping pages backed by PMM frames.
- **GDT/TSS before user syscalls**: user mode requires a TSS for safe stack switching.
- **IDT/PIC before `sti()`**: interrupts must be correctly routed before enabling them.

## Freestanding C Environment

Our kernel runs in a "freestanding" environment - no standard library. We must implement everything ourselves:

### Custom Types (types.h)

```c
typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;

typedef uint32_t size_t;
typedef uint8_t bool;
#define true  1
#define false 0
#define NULL ((void*)0)
```

### Port I/O (io.h)

```c
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline void hlt(void) {
    __asm__ volatile ("hlt");
}
```

### String Functions (string.h)

```c
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (num--) *d++ = *s++;
    return dest;
}
```

---

# 13. Shell, Status Bar, and UX

## Shell Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Shell Loop                          │
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │   Prompt    │───>│   Input     │───>│   Parse     │    │
│  │   Display   │    │   (getline) │    │   Command   │    │
│  └─────────────┘    └─────────────┘    └──────┬──────┘    │
│                                               │            │
│                                               v            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │   Output    │<───│   Execute   │<───│   Lookup    │    │
│  │   Result    │    │   Handler   │    │   Command   │    │
│  └─────────────┘    └─────────────┘    └─────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## Main Shell Loop

```c
void shell_run(void) {
    char command_buffer[256];

    statusbar_init();                  // reserves bottom row
    keyboard_set_idle_hook(shell_idle_hook);

    screen_println("Welcome to VOS Shell!");
    screen_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();
        keyboard_getline(command_buffer, 256);
        execute_command(command_buffer);
    }
}
```

## Status bar + cursor blinking

The shell installs an “idle hook” that runs while the keyboard input routine is waiting for keys:

- `statusbar_tick()` refreshes the bottom bar **once per minute** (date/time, uptime, memory, CPU string)
- The cursor is toggled on/off on a timer for a classic blinking terminal feel

This is a common kernel pattern: while the CPU is mostly waiting for input, you still want periodic UI updates without busy-waiting.

## Command Parsing

```c
static void execute_command(char* input) {
    // Skip leading whitespace
    while (*input == ' ') input++;

    if (*input == '\0') return;

    // Split command and arguments
    char* args = input;
    while (*args && *args != ' ') args++;
    if (*args == ' ') {
        *args = '\0';
        args++;
        while (*args == ' ') args++;
    }

    // Match and execute
    if (strcmp(input, "help") == 0) {
        cmd_help();
    } else if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) {
        cmd_clear();
    } else if (strcmp(input, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(input, "info") == 0 || strcmp(input, "about") == 0) {
        cmd_info();
    } else if (strcmp(input, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(input, "sleep") == 0) {
        cmd_sleep(args);
    } else if (strcmp(input, "date") == 0) {
        cmd_date();
    } else if (strcmp(input, "setdate") == 0) {
        cmd_setdate(args);
    } else if (strcmp(input, "ls") == 0) {
        cmd_ls();
    } else if (strcmp(input, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(input, "run") == 0) {
        cmd_run(args);
    } else if (strcmp(input, "color") == 0) {
        cmd_color(args);
    } else if (strcmp(input, "basic") == 0) {
        cmd_basic();
    } else if (strcmp(input, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(input, "halt") == 0 || strcmp(input, "shutdown") == 0) {
        cmd_halt();
    } else {
        screen_print("Unknown command: ");
        screen_println(input);
    }
}
```

## Built-in Commands

| Command | Description |
|---------|-------------|
| help | Display available commands |
| clear, cls | Clear the screen |
| echo <text> | Print text |
| info, about | Show system information |
| uptime | Show system uptime (s.ms) |
| sleep <ms> | Sleep for N milliseconds |
| date | Show RTC date/time |
| setdate <YYYY-MM-DD HH:MM:SS> | Set RTC date/time |
| ls | List initramfs files |
| cat <file> | Print a file from initramfs |
| run <elf> | Run a user-mode ELF from initramfs |
| color <0-15> | Change text color |
| basic | Start BASIC interpreter |
| reboot | Reboot the system |
| halt | Halt the system |

### Reboot Implementation

```c
static void cmd_reboot(void) {
    screen_println("Rebooting...");

    // Method: Keyboard controller reset
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);  // Reset command

    hlt();  // If that failed
}
```

---

# 14. Integrating a BASIC Interpreter

## uBASIC

We integrated uBASIC, a tiny BASIC interpreter by Adam Dunkels. It's designed for memory-constrained embedded systems.

### Supported Features

| Feature | Syntax |
|---------|--------|
| Print | `PRINT "text"` or `PRINT variable` |
| Variables | Single letters a-z (integers) |
| Assignment | `LET x = 5` or `x = 5` |
| Arithmetic | `+ - * / %` |
| Comparison | `< > =` |
| Logic | `& |` (bitwise) |
| Conditionals | `IF condition THEN statement ELSE statement` |
| Loops | `FOR i = 1 TO 10` ... `NEXT i` |
| Goto | `GOTO 100` |
| Subroutines | `GOSUB 200` ... `RETURN` |
| End | `END` |

### Limitations

- Integer variables only (no floating point)
- Single-letter variable names only
- No INPUT statement (can't read user input during execution)
- No arrays
- No string variables
- No RND (random) function

## Adapting uBASIC for VOS

### Custom printf

uBASIC uses printf for output. We provide a custom implementation:

```c
void basic_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's':
                    screen_print(va_arg(args, const char*));
                    break;
                case 'd':
                    screen_print_dec(va_arg(args, int));
                    break;
                case 'c':
                    screen_putchar(va_arg(args, int));
                    break;
            }
        } else {
            screen_putchar(*fmt);
        }
        fmt++;
    }

    va_end(args);
}
```

### Custom stdlib

```c
int atoi(const char* str) {
    int result = 0;
    int sign = 1;

    while (*str == ' ') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

void exit(int status) {
    // In our OS, just print error and return
    screen_print("BASIC Error (exit code: ");
    screen_print_dec(status);
    screen_println(")");
}
```

## BASIC Shell Commands

| Command | Description |
|---------|-------------|
| RUN | Execute the current program |
| LIST | Display the current program |
| NEW | Clear the program |
| DEMOS | Show available example programs |
| LOAD <1-10> | Load an example program |
| EXIT | Return to VOS shell |

## Example Programs

We include 10 example programs:

1. **Fibonacci Sequence** - Calculate first 20 Fibonacci numbers
2. **Prime Number Finder** - Find all primes from 2 to 100
3. **Multiplication Table** - Display 9x9 table
4. **Factorial Calculator** - Calculate 1! to 12!
5. **Number Pyramid** - Draw a pattern
6. **Powers of 2** - Calculate 2^0 to 2^20
7. **GCD Calculator** - Euclidean algorithm demo
8. **Sum of Series** - Sum integers, squares, cubes
9. **Triangle Patterns** - ASCII art
10. **Collatz Conjecture** - The famous 3n+1 problem

### Example: Collatz Conjecture

```basic
10 print "=== Collatz Conjecture ==="
20 print "Starting from 27:"
30 let n = 27
40 let c = 0
50 print n
60 if n = 1 then goto 120
70 let c = c + 1
80 let r = n % 2
90 if r = 0 then let n = n / 2
100 if r = 1 then let n = 3 * n + 1
110 goto 50
120 print "Reached 1 in", c, "steps"
130 end
```

---

# 15. Timekeeping (PIT + RTC)

Modern kernels rely on **time** for everything from scheduling to UI refresh. VOS deliberately uses two classic PC time sources:

- **PIT (Programmable Interval Timer)** for a *fast, monotonic tick* (good for uptime, timeouts, and scheduling)
- **CMOS RTC (Real-Time Clock)** for a *calendar clock* (good for “wall time” date/time)

## Two notions of time: monotonic vs wall time

It’s useful to separate “time” into two different concepts:

- **Monotonic time**: a counter that only moves forward while the system is running. This is what you use for:
  - uptime
  - “sleep for 100 ms”
  - scheduling time slices
  - rate-limiting UI refreshes
- **Wall time**: a human calendar date/time (year-month-day hour:minute:second). This is what you use for:
  - `date` output
  - timestamps in logs (if you add them)

### Can the PIT timer be used to track current date/time?

Yes, but not by itself.

- The PIT tells you “how much time has passed since boot” (uptime).
- To get “current time”, you need an **epoch** (a starting calendar time) and then add your uptime to it.

A typical pattern is:

1. Read the RTC at boot → `(year,month,day,hour,minute,second)` (an epoch).
2. Start a monotonic timer (PIT/APIC) → uptime ticks.
3. `now = epoch + uptime` (and periodically resync with the RTC to reduce drift).

VOS currently keeps these two concepts separate:

- **Uptime** comes from the PIT (`timer_uptime_ms()`).
- **Date/time** is read from the RTC on demand (`rtc_read_datetime()`), which is simple and avoids drift math.

## PIT Timer (IRQ0)

The PIT is the historical timer chip on PCs (Intel 8253/8254 compatible). It runs at a fixed input clock of:

```
PIT_BASE_HZ ≈ 1,193,182 Hz
```

In VOS, `timer_init(hz)` configures channel 0 in periodic mode so it generates an interrupt on **IRQ0** at (approximately) `hz`.

### Hardware background: ports and mode

The PIT uses two I/O ports in VOS:

- Command register: `0x43`
- Channel 0 data register: `0x40`

VOS programs mode `0x36` (channel 0, lobyte/hibyte, mode 3 “square wave”, binary). The driver then writes the divisor low byte and high byte.

### Divisor math

The PIT divides its input clock by a 16-bit divisor:

```
divisor = PIT_BASE_HZ / target_hz
actual_hz ≈ PIT_BASE_HZ / divisor
```

VOS requests **1000 Hz** (`timer_init(1000)`), which yields a divisor around ~1193. That gives a millisecond-ish tick that is easy to reason about and is fast enough for smooth scheduling and cursor blinking without being too expensive for a tiny kernel.

### Timer tick accounting

The IRQ0 handler is intentionally minimal: it increments a global counter. Everything else (uptime conversion, scheduling, UI refresh) uses that counter.

Key points in `kernel/timer.c`:

- `timer_ticks` is `volatile` and incremented from the IRQ handler.
- `timer_get_ticks()` uses `irq_save()`/`irq_restore()` to read ticks atomically (so you don’t tear the read if IRQ fires mid-read).
- `timer_uptime_ms()` derives milliseconds using the configured `timer_hz`.

### Timer API in VOS

See `include/timer.h`:

- `timer_init(hz)` → programs the PIT and registers IRQ0
- `timer_get_ticks()` / `timer_get_hz()` → raw tick count and configured rate
- `timer_uptime_ms()` → monotonic milliseconds since boot
- `timer_sleep_ms(ms)` → “sleep” by waiting for ticks while halting (`hlt`)

Shell commands built on this:

- `uptime` → prints seconds + milliseconds
- `sleep <ms>` → blocks the shell and waits

### `sleep`: why it uses `hlt`

In a kernel with interrupts enabled, a common way to wait is:

1. compute a target tick value
2. loop until you reach it
3. execute `hlt` inside the loop so the CPU sleeps until the next interrupt

This is exactly what `timer_sleep_ms()` does. It also temporarily enables interrupts if they were disabled, because without interrupts the PIT tick will never advance.

### Scheduling: turning ticks into time slices

VOS uses IRQ0 as the scheduler heartbeat:

- The common interrupt dispatcher (`kernel/interrupts.c`) calls `tasking_on_timer_tick(frame)` for IRQ0.
- The scheduler uses a small divider so it switches tasks every ~10 ticks (~10ms at 1kHz).

That means a task switch happens inside the same interrupt-frame mechanism used for exceptions and syscalls (the assembly stub simply resumes a different saved frame).

## CMOS RTC (date/time)

The RTC lives behind the CMOS registers on I/O ports:

- Index: `0x70`
- Data: `0x71`

The RTC provides calendar fields:

- year, month, day
- hour, minute, second

Unlike the PIT, the RTC keeps time across reboots (battery-backed), which is why it’s the natural source of “current date/time”.

### Reading the RTC safely (update-in-progress + stable reads)

The RTC updates its counters once per second. If you read while it is updating, you can get a mismatched timestamp (for example, seconds from “old” and minutes from “new”).

VOS handles this in `kernel/rtc.c`:

- Wait until the RTC is not “update-in-progress” (UIP bit in register A).
- Read all fields.
- Read them again until you get two identical samples (bounded retry loop).

### BCD vs binary, and 12h vs 24h

Depending on RTC configuration:

- fields may be stored as **BCD** (binary-coded decimal) or **binary**
- hours may be stored in **12h** mode with a PM bit, or **24h** mode

VOS reads register B to detect these modes, then normalizes to a 24-hour binary `rtc_datetime_t`.

VOS also reads the “century” CMOS register (`0x32`) when available. If that register is missing/zero, it falls back to assuming years are in the 2000s.

### Writing the RTC (why you temporarily stop updates)

When setting time, you want to avoid the RTC updating mid-write. VOS:

- sets the “update inhibit” bit in register B
- writes seconds/minutes/hours/day/month/year/century
- restores register B

It also validates ranges (month/day/hour/minute/second and leap years) and limits the supported year range to `1970..2099`.

### Shell commands

Built-in shell commands (see `kernel/shell.c`):

- `date` → prints `YYYY-MM-DD HH:MM:SS` using `rtc_read_datetime()`
- `setdate YYYY-MM-DD HH:MM:SS` → parses and writes the RTC using `rtc_set_datetime()`

### Limitations and next steps

VOS keeps the RTC as “local wall time”:

- no time zones
- no DST rules
- no high-resolution timestamps

A natural next step is to store “epoch-at-boot + uptime” as a cached wall clock and periodically resync from the RTC.

---

# 16. Serial Logging and Panic

When you’re writing a kernel, the screen might not be initialized yet (or might crash). A serial port gives you a simple, extremely reliable debug channel that keeps working even when the framebuffer path is broken.

## Serial logging (COM1)

VOS configures **COM1** at I/O base `0x3F8`:

- 115200 baud
- 8 data bits, no parity, 1 stop bit (8N1)
- FIFO enabled
- A loopback test ensures the device is responding

Core API (see `include/serial.h`):

- `serial_init()`
- `serial_write_char`, `serial_write_string`, `serial_write_hex`, `serial_write_dec`

### 16550 UART refresher (what those init writes mean)

The PC “serial port” is typically a 16550-compatible UART. Registers are addressed relative to the base:

- `base + 0`: data (THR/RBR) or divisor low (when DLAB=1)
- `base + 1`: interrupt enable (IER) or divisor high (when DLAB=1)
- `base + 3`: line control (LCR) — sets word length/parity/stop bits and the DLAB flag
- `base + 2`: FIFO control (FCR)
- `base + 4`: modem control (MCR) — also controls loopback mode
- `base + 5`: line status (LSR) — bit 5 indicates “transmit holding register empty”

VOS’s init sequence in `kernel/serial.c` does the classic:

1. Disable UART interrupts.
2. Set `DLAB=1` and write divisor `1` → 115200 baud.
3. Set 8N1 in LCR.
4. Enable and clear FIFOs.
5. Perform a loopback self-test to make sure the port is present.

### “Everything you print is also a log”

VOS mirrors **screen output** to serial inside `screen_putchar()`. This means the serial log contains:

- boot banner and `[OK]` messages
- panic output
- user-space `sys_write()` output (because it prints via the screen)

### QEMU: how to view the serial log

Common options:

```bash
# Show serial output in your terminal:
qemu-system-i386 -cdrom vos.iso -serial stdio

# Write serial output to a file:
qemu-system-i386 -cdrom vos.iso -serial file:vos-serial.log
```

You can then inspect logs with:

```bash
tail -f vos-serial.log
```

## Panic and exception reporting

VOS has two panic paths:

- `panic("message")`: a generic fatal error
- `panic_with_frame("message", frame)`: fatal error with a full CPU register dump

CPU exceptions (divide by zero, page fault, etc.) route through the interrupt path and ultimately call `panic_with_frame()` with the exception name and the saved register frame. For page faults (interrupt 14), VOS also prints **CR2**, which contains the faulting virtual address.

### What the panic dump contains (and why it matters)

In `kernel/panic.c`, `panic_with_frame()` prints:

- `int_no` and `err_code`
- `eip`, `cs`, `eflags`
- general registers (`eax..edi`, plus a saved `esp`)
- segment registers (`ds`, `es`, `fs`, `gs`)
- `cr2` for page faults (interrupt 14)

This is enough to:

- identify the exception class (e.g., page fault vs GP fault)
- locate the faulting instruction pointer (`eip`)
- see which privilege level you were in (`cs` selector + CPL)
- inspect register state without a debugger

### Page fault 101 (CR2 + error code bits)

For a page fault:

- `CR2` holds the virtual address that caused the fault.
- the error code (`err_code`) encodes what kind of fault it was.

Common bit meanings (x86):

- bit 0 (P): 0 = non-present page, 1 = protection violation
- bit 1 (W/R): 0 = read, 1 = write
- bit 2 (U/S): 0 = supervisor, 1 = user
- bit 3 (RSVD): reserved-bit violation
- bit 4 (I/D): instruction fetch (if supported)

VOS currently treats all exceptions as fatal, but the dump format is the same foundation you’d later use for a recoverable page-fault handler.

---

# 17. Memory Management (early_alloc, PMM, paging, kheap)

Memory management in kernels is layered. VOS builds up capabilities in stages:

1. **early allocator** (very early boot, before a real allocator exists)
2. **paging** (page tables + enabling CR0.PG)
3. **PMM** (physical frames allocator)
4. **kernel heap** (virtual allocations backed by PMM frames)

## A quick map of addresses used by VOS

VOS uses a few important address ranges you’ll see repeatedly:

- Kernel is linked at **1 MiB** (`linker.ld` sets `. = 1M;`)
- Paging is enabled early with identity mappings (virtual == physical) for boot regions
- Kernel heap virtual base: `0xD0000000` (`kernel/kheap.c`)
- User program virtual range: `0x01000000..0xC0000000` (`kernel/elf.c`)
- User stack top: `0x02000000` (8 pages mapped below it)

This is not a “higher-half kernel” design yet; it’s an identity-mapped kernel with some higher virtual regions used for dynamic allocations.

## early_alloc: the “bootstrap allocator”

`early_alloc` is a simple bump-pointer allocator used during early initialization (before PMM/heap are ready). The key challenge is: **where do we start allocating from without overwriting bootloader data?**

VOS computes `early_start` as the maximum of:

- the end of the kernel image (`__kernel_end`)
- the multiboot info struct
- the multiboot memory map region
- the end of all multiboot modules (including initramfs)

This prevents subtle “self-corruption” bugs where early allocations overwrite the initramfs module.

### Alignment and why `early_alloc` never frees

`early_alloc` exists to solve a bootstrapping problem: you need memory for page tables, bitmaps, and early structs *before* you have a real allocator.

In VOS (`kernel/early_alloc.c`):

- allocations optionally align to a power-of-two boundary (often 16 bytes or 4096 bytes)
- the pointer only moves forward
- memory is never freed

This is fine because:

- early allocations are few and mostly “forever” structures (page tables, bitmaps)
- it keeps code tiny and predictable during the fragile boot stage

## Paging (virtual memory)

Paging is where x86 turns a 32-bit virtual address into a physical address through two lookup tables:

```
virtual addr bits:  [31..22] [21..12] [11..0]
                    PDE idx  PTE idx  offset
```

- Page directory: 1024 entries → selects a page table
- Page table: 1024 entries → selects a 4 KiB page frame
- Offset: 12 bits → byte offset within the page

VOS uses 4 KiB pages and a single page directory shared by all tasks.

VOS uses 32-bit paging with a single page directory:

- Identity-maps the first **16 MiB** early (kernel + boot data)
- Identity-maps multiboot info/mmap/modules and the framebuffer (bochs-display often uses a high physical address)
- Enables paging by loading `CR3` and setting `CR0.PG`

VOS uses paging for:

- A high virtual address kernel heap region
- Marking user pages as `PAGE_USER` for ring 3 tasks

### Page table allocation and `paging_prepare_range()`

In VOS, page tables are allocated from `early_alloc`, not from the PMM. That matters because:

- PMM frames are meant to back “real” pages (heap pages, user pages, etc.).
- page tables themselves also consume physical frames, and they must never collide with frames handed out by PMM.

Two mechanisms keep this safe:

1. `paging_prepare_range(vaddr, size, flags)` creates any required page tables for a mapping range *before* allocating frames for the pages.
2. `pmm_alloc_frame()` has a guard (`pmm_reserve_new_early_alloc()`) that re-reserves the early allocator growth in case new page tables were allocated after PMM init.

This is one of those small details that prevents “mystery corruption” bugs later.

## PMM: Physical Memory Manager

The PMM tracks page frames using a bitmap:

- Multiboot memory map (`type == 1`) regions are marked free
- Everything else defaults to used
- The kernel, low memory, multiboot structures/modules, framebuffer, and early allocations are reserved

API:

- `pmm_alloc_frame()` → returns a physical address of a free 4 KiB frame
- `pmm_free_frame(paddr)` → frees it

### What PMM considers “free”

VOS uses the Multiboot memory map when available:

- entries with `type == 1` are “available RAM”
- everything else (ROM, MMIO, ACPI, reserved) is treated as used

Then VOS explicitly reserves critical regions:

- low memory (`0..1MiB`) for BIOS/real-mode artifacts and hardware regions
- the kernel image
- Multiboot info, mmap data, and module payloads (initramfs)
- the framebuffer region (often a high physical address)
- any pages consumed by `early_alloc` (bitmap + page tables + early structs)

The end result is a bitmap where “free” frames should really be safe to allocate.

## kheap: Kernel heap

The kernel heap is a minimal bump allocator at a fixed virtual base (`0xD0000000`):

- `kmalloc`/`kcalloc` allocate virtual memory
- The heap grows by mapping more pages using frames from the PMM
- `kfree` is currently a stub (no free list/coalescing yet)

### Why the heap calls `paging_prepare_range()` first

When the heap grows, it needs to:

1. ensure page tables exist for the virtual range it’s about to map
2. allocate physical frames for the new pages
3. map those frames into the page tables

If you allocate frames first and only later allocate page tables, you risk allocating a physical frame for a heap page that later gets consumed by a new page table (because page tables come from `early_alloc` too). VOS avoids that by preparing the range before grabbing frames.

### Limitations (and what a “real” allocator adds)

This heap is intentionally tiny:

- no `kfree` implementation
- no reuse of freed blocks
- no fragmentation management

Typical next steps include:

- a free-list allocator with coalescing
- a buddy allocator
- slab allocators for fixed-size kernel objects

---

# 18. initramfs and VFS

An initramfs is an “initial RAM filesystem” that provides files very early in boot, before you have a disk driver or a real filesystem.

## How VOS provides an initramfs

VOS builds a tar archive and passes it to the kernel as a **Multiboot module**:

- `initramfs/` (repo directory) becomes the root of the archive
- The build also injects a user program at `/bin/init`
- GRUB loads the tar as a module (`module /boot/initramfs.tar`)

## VFS: in-memory, read-only file index

On boot, VOS parses the tar module and builds a small index:

- Each file entry stores `(name, data pointer, size)`
- File data is not copied; it points into the module’s memory
- Only regular files are supported (tar `typeflag` `0` or `\\0`)

### Tar format: why everything is in 512-byte blocks

The initramfs is a **tar archive**. Tar is simple enough to parse in a kernel:

- Each header is exactly **512 bytes**.
- File size is stored as an **octal ASCII number** in the header.
- File data immediately follows the header.
- File data is padded up to the next 512-byte boundary.
- The archive ends with one or more “all-zero” 512-byte blocks.

VOS implements exactly these rules in `kernel/vfs.c` with:

- `parse_octal_u32()` for the size field
- `align_up_512()` to find the next header
- `is_zero_block()` to detect end-of-archive

### Memory model: zero-copy file data

VOS never copies file payload bytes out of the tar module:

- `vfs_read_file(path, &data, &size)` returns a pointer into the module’s memory.
- This makes `cat` and `run` extremely cheap and simple.
- It also means the initramfs module must remain mapped and must not be overwritten by allocators.

This is why `compute_early_start()` and PMM reservations treat Multiboot modules as “must keep”.

User-visible shell commands:

- `ls` → list files in the initramfs
- `cat <path>` → print a file (truncates very large files)

And the initramfs also provides the user program:

- `/bin/init` → executed automatically at boot (and you can also `run /bin/init`)

---

# 19. Tasking, Syscalls, User Mode, and ELF

This is where VOS moves from “a kernel with a shell” to “a kernel that can run user programs”.

In VOS, “tasking” is deliberately simple:

- tasks share a single address space (no per-process page directory yet)
- tasks have their own saved CPU context (an interrupt frame pointer) and a kernel stack
- the scheduler is round-robin

This is enough to demonstrate:

- preemption from a timer interrupt
- syscalls from ring 3
- user-mode entry/exit via `iret`

## Context switching via interrupt frames

VOS implements task switching by saving CPU state in an **interrupt frame**:

- On every interrupt/IRQ, the assembly stub saves registers and builds a frame on the current stack.
- The C interrupt handler returns a pointer to the frame that should be resumed next.
- The assembly stub sets `ESP` to that returned pointer and executes `iret`.

This design makes timer preemption and voluntary yields use the same mechanism.

### How VOS represents a task

See `kernel/task.c`:

- `esp`: saved pointer to an interrupt frame on the task’s kernel stack
- `kstack_top`: top of that kernel stack (used to update `TSS.esp0`)
- a simple circular `next` pointer for round-robin scheduling

VOS creates:

- a “boot task” representing the kernel context already running on the boot stack
- an “idle task” that executes `hlt` forever
- user tasks spawned from ELF images

## Syscalls (`int 0x80`)

VOS exposes a small syscall ABI:

- Triggered with `int 0x80` from ring 3
- Syscall number in `EAX`
- Arguments in registers (e.g. `EBX`, `ECX`)

Current syscalls:

- `SYS_WRITE (0)`: write a buffer to the screen
- `SYS_EXIT (1)`: terminate the current task
- `SYS_YIELD (2)`: voluntarily yield the CPU

### Syscall ABI details (register conventions)

See `kernel/syscall.c` and `user/syscall.h`:

- Enter with `int 0x80`
- `EAX` = syscall number
- arguments in registers (`EBX`, `ECX`, ...)
- return value in `EAX` (negative values are used for errors)

This is not a POSIX ABI; it’s a tiny educational ABI designed to be easy to call from C and assembly.

### Safety note: user pointers

`SYS_WRITE` currently trusts the user pointer (`EBX`) and length (`ECX`) and directly reads from it.

That is fine for early experiments, but in a hardened kernel you would:

- validate user pointers fall in user-mapped ranges
- copy user data into a kernel buffer (“copyin”) to avoid faults in the middle of kernel code

## ELF32 user programs

VOS includes a small ELF loader for **ELF32 i386 ET_EXEC**:

- Loads `PT_LOAD` segments into user virtual memory as `PAGE_USER` pages
- Copies file bytes and zeros the rest (BSS)
- Maps a fixed user stack near `0x02000000`

### What “user mode” means in VOS

The GDT installs ring-3 code/data segments:

- user CS selector: `0x1B`
- user DS/SS selectors: `0x23`

When the scheduler resumes a user task, it does so by returning an interrupt frame that contains a ring-3 `iret` frame:

- `SS`/`ESP` (user stack)
- `EFLAGS`
- `CS`/`EIP` (user entry point)

The CPU uses the TSS (`SS0`/`ESP0`) to choose a safe kernel stack when that user task later executes `int 0x80` or takes an interrupt.

### ELF loader constraints (what VOS supports)

In `kernel/elf.c`, VOS supports:

- ELF32, little endian
- `e_type == ET_EXEC`
- `e_machine == EM_386`
- loadable segments only (`PT_LOAD`)

For each `PT_LOAD` segment VOS:

1. validates the segment is within a user virtual range (`0x01000000..0xC0000000`)
2. allocates and maps pages as `PAGE_USER` (and `PAGE_RW` if writable)
3. copies initialized bytes, zeros the remainder (BSS)

Then it maps an 8-page user stack below `0x02000000` and returns:

- `entry` (EIP)
- `user_esp` (initial user stack pointer)

The kernel tries to start `/bin/init` automatically from the initramfs. You can also start an ELF manually from the shell with:

```
run /bin/init
```

### The minimal userland: `crt0` + syscalls

VOS includes a tiny user-space runtime:

- `user/crt0.asm` provides `_start`, calls `main`, then `SYS_EXIT`.
- `user/syscall.h` provides inline-assembly wrappers for `SYS_WRITE`, `SYS_YIELD`, `SYS_EXIT`.
- `user/init.c` is the simplest possible user program: it prints a message and exits.

This is intentionally small so you can see the full kernel↔user interface without extra libraries.

---

# 20. Build System and Toolchain

## Makefile

```makefile
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
```

### Build and run workflow

- `make` builds `vos.iso` (kernel + initramfs + `/bin/init`)
- `make run` launches QEMU with the Bochs display device (high-resolution framebuffer)
- `make debug` is the same but enables QEMU interrupt tracing

To override the framebuffer resolution used by QEMU:

```bash
make run QEMU_XRES=1920 QEMU_YRES=1080
```

## Compiler Flags Explained

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
| `-c` | Compile only, don't link |

## Linker Flags Explained

| Flag | Purpose |
|------|---------|
| `-m elf_i386` | Output 32-bit ELF format |
| `-T linker.ld` | Use our linker script |
| `-nostdlib` | Don't link standard libraries |

---

# 21. Testing with QEMU and VirtualBox

## QEMU (Quick Testing)

```bash
# Build + run the ISO (recommended)
make run
```

### Choosing a resolution

The Makefile uses the Bochs display device (a simple linear framebuffer) and lets you override resolution:

```bash
make run QEMU_XRES=1920 QEMU_YRES=1080
```

### Capturing the serial log (COM1)

VOS initializes COM1 and mirrors screen output to serial. To see it:

```bash
# Show serial output in your terminal:
qemu-system-i386 -cdrom vos.iso -vga none -device bochs-display,xres=1280,yres=800 -serial stdio

# Or write serial output to a file:
qemu-system-i386 -cdrom vos.iso -vga none -device bochs-display,xres=1280,yres=800 -serial file:vos-serial.log
tail -f vos-serial.log
```

### Debug mode

```bash
make debug
```

This enables QEMU interrupt tracing (`-d int`) and disables automatic reboot (`-no-reboot`), which is useful when you are debugging early exceptions.

## VirtualBox (Full Testing)

1. Create New VM:
   - Name: VOS
   - Type: Other
   - Version: Other/Unknown (32-bit)

2. Memory: 64 MB (minimum)

3. Storage: No hard disk needed

4. Settings → Storage:
   - Add optical drive
   - Choose vos.iso

5. Start the VM

## Debugging Tips

### VirtualBox Logs
```bash
cat ~/VirtualBox\ VMs/vos/Logs/VBox.log | grep -E "(GURU|eip|Changing)"
```

### Common Issues

| Symptom | Likely Cause |
|---------|--------------|
| Triple Fault | Invalid IDT entry, wrong segment selector |
| No output | Crash before `screen_init()` or no usable framebuffer (falls back to VGA) |
| No keyboard input | QEMU window not focused (click the window; release with Ctrl+Alt) or IRQ1 masked |
| Keyboard not working | PIC not remapped, IRQ masked, wrong IDT entry/handler |
| Crashes on key press | Interrupt handler not returning properly |

---

# 22. Project Structure

```
vos/
├── boot/
│   └── boot.asm              # Multiboot header, entry point, ISR stubs, GDT/IDT helpers
│
├── kernel/
│   ├── kernel.c              # kernel_main + init order
│   ├── idt.c                 # IDT + PIC setup
│   ├── interrupts.c          # Top-level dispatcher + IRQ EOI + syscalls
│   ├── gdt.c                 # GDT + TSS (ring transitions)
│   ├── task.c                # Round-robin tasking + user task spawn
│   ├── syscall.c             # int 0x80 syscall handler
│   ├── elf.c                 # ELF32 loader (ET_EXEC, PT_LOAD)
│   ├── paging.c              # Page tables + paging enable
│   ├── pmm.c                 # Physical memory manager (bitmap frames)
│   ├── kheap.c               # Simple bump heap (maps pages on demand)
│   ├── early_alloc.c         # Early bump allocator (bootstraps paging/PMM)
│   ├── vfs.c                 # initramfs tar parser + in-memory file index
│   ├── timer.c               # PIT driver + uptime/sleep
│   ├── rtc.c                 # CMOS RTC read/set
│   ├── serial.c              # COM1 serial logging
│   ├── panic.c               # panic + exception dumps (CR2 on page faults)
│   ├── screen.c              # VGA + framebuffer console (PSF2 fonts), cursor, padding
│   ├── statusbar.c           # Bottom status bar (date/time, uptime, mem, CPU)
│   ├── keyboard.c            # PS/2 keyboard (ES layout + history)
│   ├── shell.c               # Shell commands + UX glue
│   ├── font_psf2.c           # PSF2 font parser
│   ├── font_vga_psf2.c       # Embedded PSF2 font bytes (from third_party)
│   ├── font_terminus_psf2.c  # Embedded PSF2 font bytes (from third_party)
│   ├── font8x8.c             # Tiny bitmap font helpers (legacy/demo)
│   ├── basic_io.c            # printf/exit for BASIC
│   ├── basic_programs.c      # Example BASIC programs
│   ├── tokenizer.c           # uBASIC tokenizer
│   ├── ubasic.c              # uBASIC interpreter
│   └── string.c              # Minimal string/mem helpers
│
├── include/
│   ├── types.h               # Basic types (uint8_t, etc.)
│   ├── io.h                  # Port I/O functions
│   ├── string.h              # String functions
│   ├── ctype.h               # Character classification
│   ├── stdio.h               # printf wrapper
│   ├── stdlib.h              # atoi, exit
│   ├── multiboot.h           # Multiboot1 structs/constants
│   ├── screen.h              # Console API (VGA + framebuffer)
│   ├── font.h                # Font abstraction (PSF2)
│   ├── keyboard.h            # Keyboard input + history
│   ├── statusbar.h           # Status bar API
│   ├── timer.h               # PIT timer API
│   ├── rtc.h                 # CMOS RTC API
│   ├── serial.h              # Serial logging API
│   ├── panic.h               # panic + panic_with_frame
│   ├── early_alloc.h         # Early allocator API
│   ├── paging.h              # Paging API/flags
│   ├── pmm.h                 # Physical memory manager API
│   ├── kheap.h               # Kernel heap API
│   ├── vfs.h                 # initramfs VFS API
│   ├── gdt.h                 # GDT/TSS API
│   ├── idt.h                 # IDT + stub tables
│   ├── interrupts.h          # interrupt_frame_t + IRQ helpers
│   ├── task.h                # Tasking API
│   ├── syscall.h             # Syscall entry (kernel side)
│   ├── elf.h                 # ELF loader API
│   ├── shell.h               # Shell API
│   ├── ubasic.h              # BASIC interpreter
│   ├── tokenizer.h           # BASIC tokenizer
│   └── basic_programs.h      # BASIC demo programs
│
├── user/
│   ├── crt0.asm              # Minimal user entry point
│   ├── init.c                # /bin/init (prints from user mode + exits)
│   ├── linker.ld             # Userland linker script
│   └── syscall.h             # User syscall wrappers (int 0x80)
│
├── initramfs/
│   └── hello.txt             # Example file (accessible via `cat hello.txt`)
│
├── third_party/
│   └── fonts/
│       ├── terminus/LICENSE
│       └── vga/LICENSE
│
├── iso/boot/grub/
│   └── grub.cfg              # GRUB configuration
│
├── build/                    # Build artifacts (objects, kernel.bin, user/, initramfs_root/)
│
├── linker.ld                 # Linker script
├── Makefile                  # Build system
├── DOCUMENTATION.md          # This file
└── vos.iso                   # Bootable ISO (build output)
```

---

# 23. Future Enhancements

VOS already implements many “classic next steps” (paging, a PMM, a heap, an initramfs, syscalls, user mode). What’s left is where kernels become *systems* rather than demos.

Below is a roadmap that’s both practical and educational (each bullet corresponds to a real OS concept you can learn and implement incrementally).

## Stability and safety

- **Non-fatal user faults**: handle page faults from ring 3 by killing the user task instead of panicking the kernel.
- **User pointer validation**: add “copyin/copyout” helpers so syscalls don’t trust user pointers.
- **Guard pages**: place unmapped pages below stacks (kernel and user) to catch stack overflows early.
- **Better diagnostics**: print page-fault error decoding, backtraces (even simple frame-pointer walks help).

## Memory management

- **Real `kfree`**: replace the bump allocator with a free-list allocator (coalescing), buddy allocator, and/or slab caches.
- **Per-process address spaces**: give each process its own page directory; keep the kernel mapped but supervisor-only.
- **User allocations**: add a `brk`/`mmap`-style syscall so user programs can allocate memory safely.

## Processes and scheduling

- **Sleep/wakeup**: integrate a sleep queue with the timer tick so `sleep` becomes a syscall instead of a shell-side busy wait.
- **Exit codes + wait**: `exit(status)` + `wait(pid)` is a great “first process API”.
- **Signals/kill**: even a tiny `kill(pid)` teaches process lifecycle and synchronization.
- **Scheduler evolution**: priorities, timeslice accounting, and eventually blocking I/O.

## Storage and filesystems

- **Block device layer**: a generic “read/write blocks” interface that drivers implement.
- **ATA/IDE driver**: classic first disk driver on i386.
- **Real filesystem**: FAT12/16 is approachable (and has lots of references); ext2 is a common “next”.
- **VFS growth**: directories, file descriptors, and `open/read/write/close`.

## Drivers and hardware discovery

- **PS/2 mouse**: a natural extension of the keyboard driver.
- **PCI enumeration**: discover devices and build a driver model.
- **ACPI**: for power management and cleaner shutdown/reboot paths.

## Userland and UX

- **More syscalls**: `read`, `fork`/`exec` (or at least an `exec`-like loader), `gettime`, `sleep`, etc.
- **A tiny libc**: implement enough of `libc` for convenience, or port one.
- **Shell upgrades**: quoting, pipelines, redirects, jobs, tab-completion, and better line editing.
- **Timestamps in logs**: combine RTC + uptime for readable logs.

## Networking (later, but fun)

- **NIC driver**: e.g., NE2000 is classic for hobby OS work.
- **TCP/IP stack**: often easiest as a “bring-up” using an existing small stack.

## Existing projects worth studying/borrowing

If you want to accelerate by integrating proven code, these are common “hobby OS” building blocks:

- **Filesystems**: ChaN’s FatFs (FAT), various ext2 implementations
- **Networking**: lwIP (embedded TCP/IP)
- **C library**: musl or newlib (porting work required)
- **Shells**: small embedded shells, or writing a minimal POSIX-ish shell with a growing syscall set

The key is to define a stable syscall/file-descriptor API first; that determines how reusable “outside code” will be.

---

# 24. Resources and References

## Books

- **Operating Systems: Design and Implementation** - Andrew Tanenbaum
- **Operating System Concepts** - Silberschatz, Galvin, Gagne
- **The Little Book About OS Development** - Erik Helin & Adam Renberg

## Online Resources

- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development wiki
- [Intel Software Developer Manuals](https://www.intel.com/sdm) - x86 architecture reference
- [Bran's Kernel Development Tutorial](http://www.osdever.net/bkerndev/)
- [James Molloy's Kernel Tutorials](http://www.jamesmolloy.co.uk/tutorial_html/)

### OSDev pages that map directly to VOS subsystems

- Boot + Multiboot:
  - https://wiki.osdev.org/Multiboot
  - https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
- GDT/TSS + privilege levels:
  - https://wiki.osdev.org/GDT
  - https://wiki.osdev.org/TSS
  - https://wiki.osdev.org/Privilege_Level
- Interrupts + PIC:
  - https://wiki.osdev.org/Interrupts
  - https://wiki.osdev.org/IDT
  - https://wiki.osdev.org/8259_PIC
- Paging:
  - https://wiki.osdev.org/Paging
  - https://wiki.osdev.org/Page_Fault
- Timers and RTC:
  - https://wiki.osdev.org/PIT
  - https://wiki.osdev.org/CMOS
  - https://wiki.osdev.org/RTC
- Serial:
  - https://wiki.osdev.org/Serial_Ports
- initramfs/tar:
  - https://wiki.osdev.org/Initrd
- ELF:
  - https://wiki.osdev.org/ELF
- PS/2 keyboard:
  - https://wiki.osdev.org/PS/2_Keyboard
- Fonts / framebuffer text:
  - https://wiki.osdev.org/Drawing_In_A_Linear_Framebuffer
  - https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html (PSF font formats)

## uBASIC

- [uBASIC by Adam Dunkels](https://dunkels.com/adam/ubasic/) - The BASIC interpreter we integrated
- BSD 3-Clause License

## Embedded fonts

VOS embeds PSF2 font data derived from third-party sources.

- Licenses are stored in:
  - `third_party/fonts/terminus/LICENSE`
  - `third_party/fonts/vga/LICENSE`

## Tools

- [NASM](https://www.nasm.us/) - The Netwide Assembler
- [GCC](https://gcc.gnu.org/) - GNU Compiler Collection
- [GRUB](https://www.gnu.org/software/grub/) - Grand Unified Bootloader
- [VirtualBox](https://www.virtualbox.org/) - Virtualization
- [QEMU](https://www.qemu.org/) - Emulation

---

# Appendix A: Complete Scancode Table (Set 1)

```
┌──────┬─────────────┬──────┬─────────────┐
│ Code │ Key         │ Code │ Key         │
├──────┼─────────────┼──────┼─────────────┤
│ 01   │ Escape      │ 21   │ F           │
│ 02   │ 1           │ 22   │ G           │
│ 03   │ 2           │ 23   │ H           │
│ 04   │ 3           │ 24   │ J           │
│ 05   │ 4           │ 25   │ K           │
│ 06   │ 5           │ 26   │ L           │
│ 07   │ 6           │ 27   │ ; (ñ ES)    │
│ 08   │ 7           │ 28   │ ' (´ ES)    │
│ 09   │ 8           │ 29   │ ` (º ES)    │
│ 0A   │ 9           │ 2A   │ Left Shift  │
│ 0B   │ 0           │ 2B   │ \ (ç ES)    │
│ 0C   │ - (' ES)    │ 2C   │ Z           │
│ 0D   │ = (¡ ES)    │ 2D   │ X           │
│ 0E   │ Backspace   │ 2E   │ C           │
│ 0F   │ Tab         │ 2F   │ V           │
│ 10   │ Q           │ 30   │ B           │
│ 11   │ W           │ 31   │ N           │
│ 12   │ E           │ 32   │ M           │
│ 13   │ R           │ 33   │ ,           │
│ 14   │ T           │ 34   │ .           │
│ 15   │ Y           │ 35   │ / (- ES)    │
│ 16   │ U           │ 36   │ Right Shift │
│ 17   │ I           │ 37   │ * (Keypad)  │
│ 18   │ O           │ 38   │ Left Alt    │
│ 19   │ P           │ 39   │ Space       │
│ 1A   │ [ (` ES)    │ 3A   │ Caps Lock   │
│ 1B   │ ] (+ ES)    │ 3B   │ F1          │
│ 1C   │ Enter       │ ...  │ ...         │
│ 1D   │ Left Ctrl   │      │             │
│ 1E   │ A           │      │             │
│ 1F   │ S           │      │             │
│ 20   │ D           │      │             │
└──────┴─────────────┴──────┴─────────────┘

Extended (E0 prefix):
┌──────┬─────────────┐
│ E0 48│ Up Arrow    │
│ E0 50│ Down Arrow  │
│ E0 4B│ Left Arrow  │
│ E0 4D│ Right Arrow │
│ E0 38│ Right Alt   │
│ E0 1D│ Right Ctrl  │
└──────┴─────────────┘
```

---

# Appendix B: VGA Color Reference

```
┌───────┬───────────────┬─────────────────────┐
│ Value │ Color         │ Hex (RGB approx)    │
├───────┼───────────────┼─────────────────────┤
│   0   │ Black         │ #000000             │
│   1   │ Blue          │ #0000AA             │
│   2   │ Green         │ #00AA00             │
│   3   │ Cyan          │ #00AAAA             │
│   4   │ Red           │ #AA0000             │
│   5   │ Magenta       │ #AA00AA             │
│   6   │ Brown         │ #AA5500             │
│   7   │ Light Grey    │ #AAAAAA             │
│   8   │ Dark Grey     │ #555555             │
│   9   │ Light Blue    │ #5555FF             │
│  10   │ Light Green   │ #55FF55             │
│  11   │ Light Cyan    │ #55FFFF             │
│  12   │ Light Red     │ #FF5555             │
│  13   │ Light Magenta │ #FF55FF             │
│  14   │ Yellow        │ #FFFF55             │
│  15   │ White         │ #FFFFFF             │
└───────┴───────────────┴─────────────────────┘
```

---

# Appendix C: x86 Register Reference

## General Purpose Registers (32-bit)

```
┌─────────────────────────────────────────────────────┐
│  31              16 15       8 7        0           │
├─────────────────────────────────────────────────────┤
│        EAX         │    AH    │    AL   │ Accumulator
├─────────────────────────────────────────────────────┤
│        EBX         │    BH    │    BL   │ Base
├─────────────────────────────────────────────────────┤
│        ECX         │    CH    │    CL   │ Counter
├─────────────────────────────────────────────────────┤
│        EDX         │    DH    │    DL   │ Data
├─────────────────────────────────────────────────────┤
│        ESI         │                    │ Source Index
├─────────────────────────────────────────────────────┤
│        EDI         │                    │ Dest Index
├─────────────────────────────────────────────────────┤
│        EBP         │                    │ Base Pointer
├─────────────────────────────────────────────────────┤
│        ESP         │                    │ Stack Pointer
└─────────────────────────────────────────────────────┘
```

## Segment Registers

| Register | Purpose |
|----------|---------|
| CS | Code Segment |
| DS | Data Segment |
| SS | Stack Segment |
| ES | Extra Segment |
| FS | Extra Segment |
| GS | Extra Segment |

## Special Registers

| Register | Purpose |
|----------|---------|
| EIP | Instruction Pointer |
| EFLAGS | Status flags |
| CR0-CR4 | Control registers |
| GDTR | GDT Register |
| IDTR | IDT Register |

---

**VOS - Victor's Operating System**
**A minimal educational OS for learning systems programming**

*Created with assistance from Claude (Anthropic)*
*uBASIC interpreter by Adam Dunkels (BSD License)*
