# VOS - Victor's Operating System
## A Comprehensive Guide to Building a Minimal Operating System

---

# Table of Contents

1. [Introduction](#1-introduction)
2. [Prerequisites and Tools](#2-prerequisites-and-tools)
3. [Understanding the Boot Process](#3-understanding-the-boot-process)
4. [The Multiboot Specification](#4-the-multiboot-specification)
5. [Assembly Entry Point](#5-assembly-entry-point)
6. [Protected Mode and Memory](#6-protected-mode-and-memory)
7. [The Global Descriptor Table (GDT)](#7-the-global-descriptor-table-gdt)
8. [Interrupt Handling and the IDT](#8-interrupt-handling-and-the-idt)
9. [Programmable Interrupt Controller (PIC)](#9-programmable-interrupt-controller-pic)
10. [VGA Text Mode Display](#10-vga-text-mode-display)
11. [PS/2 Keyboard Driver](#11-ps2-keyboard-driver)
12. [The Kernel](#12-the-kernel)
13. [Building a Shell](#13-building-a-shell)
14. [Integrating a BASIC Interpreter](#14-integrating-a-basic-interpreter)
15. [Build System and Toolchain](#15-build-system-and-toolchain)
16. [Testing with VirtualBox and QEMU](#16-testing-with-virtualbox-and-qemu)
17. [Project Structure](#17-project-structure)
18. [Future Enhancements](#18-future-enhancements)
19. [Resources and References](#19-resources-and-references)

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
- **VGA text mode** display (80x25 characters, 16 colors)
- **PS/2 keyboard** driver with interrupt handling
- **Spanish keyboard layout** with AltGr support
- **Command history** (up/down arrow navigation)
- **Simple shell** with built-in commands
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

# 6. Protected Mode and Memory

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

## The Linker Script

The linker script (`linker.ld`) tells the linker how to arrange our sections:

```ld
ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1MB mark */
    . = 1M;

    /* Multiboot header must be first */
    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
    }

    /* Code section */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized data and stack */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
```

---

# 7. The Global Descriptor Table (GDT)

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

## GRUB's GDT

GRUB sets up a minimal GDT for us:

| Selector | Index | Description |
|----------|-------|-------------|
| 0x00 | 0 | Null descriptor (required) |
| 0x08 | 1 | (May be unused or different) |
| 0x10 | 2 | Code segment (base 0, limit 4GB) |
| 0x18 | 3 | Data segment (base 0, limit 4GB) |

**Important**: GRUB uses selector 0x10 for code and 0x18 for data, not the conventional 0x08/0x10. We discovered this by examining the CPU state and fixed our IDT to use selector 0x10.

## Flat Memory Model

With base = 0 and limit = 4GB, both segments cover the entire address space. This is called a "flat memory model" - all addresses are direct physical addresses.

---

# 8. Interrupt Handling and the IDT

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

For our interrupt gates, we use flags = 0x8E:
- P = 1 (present)
- DPL = 0 (kernel privilege)
- Type = 0xE (32-bit interrupt gate)

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
    // Set up IDT pointer
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    // Set default handler for all interrupts
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default, 0x10, 0x8E);
    }

    // Remap the PIC
    pic_remap();

    // Set up specific handlers
    idt_set_gate(32, (uint32_t)isr_timer, 0x10, 0x8E);
    idt_set_gate(33, (uint32_t)isr_keyboard, 0x10, 0x8E);

    // Load the IDT
    idt_flush((uint32_t)&idtp);
}
```

## Interrupt Handlers in Assembly

Interrupt handlers must follow a specific protocol:

```nasm
; Keyboard interrupt handler
isr_keyboard:
    pusha                   ; Save all registers
    call keyboard_handler   ; Call C handler
    popa                    ; Restore registers
    iret                    ; Return from interrupt

; Timer handler (just acknowledge and return)
isr_timer:
    push eax
    mov al, 0x20            ; Send EOI to PIC
    out 0x20, al
    pop eax
    iret

; Default handler for unhandled interrupts
isr_default:
    iret
```

### The IRET Instruction

`iret` (Interrupt Return) pops the following from the stack:
1. EIP (instruction pointer)
2. CS (code segment)
3. EFLAGS (flags register)

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

# 10. VGA Text Mode Display

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

```c
void screen_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_BUFFER[cursor_y * 80 + cursor_x] = vga_entry(' ', color);
        }
    } else {
        VGA_BUFFER[cursor_y * 80 + cursor_x] = vga_entry(c, color);
        cursor_x++;
    }

    // Handle line wrap
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle scrolling
    if (cursor_y >= 25) {
        screen_scroll();
    }

    update_cursor();
}
```

### Scrolling

```c
static void screen_scroll(void) {
    // Move all lines up by one
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            VGA_BUFFER[y * 80 + x] = VGA_BUFFER[(y + 1) * 80 + x];
        }
    }

    // Clear the last line
    for (int x = 0; x < 80; x++) {
        VGA_BUFFER[24 * 80 + x] = vga_entry(' ', color);
    }

    cursor_y = 24;
}
```

### Hardware Cursor

The VGA controller has a hardware cursor that we can control:

```c
static void update_cursor(void) {
    uint16_t pos = cursor_y * 80 + cursor_x;

    outb(0x3D4, 0x0F);           // Cursor location low register
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);           // Cursor location high register
    outb(0x3D5, (pos >> 8) & 0xFF);
}
```

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

    // Check for extended key prefix
    if (scancode == 0xE0) {
        extended_key = true;
        outb(0x20, 0x20);  // Send EOI
        return;
    }

    if (extended_key) {
        extended_key = false;

        // Handle extended keys
        if (!(scancode & 0x80)) {  // Key press
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
        outb(0x20, 0x20);
        return;
    }

    // Handle regular keys...
    // (see full implementation in keyboard.c)

    outb(0x20, 0x20);  // Send EOI to PIC
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

# 12. The Kernel

## Kernel Entry Point (C)

```c
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    (void)mboot_info;  // Reserved for future use

    // Initialize screen
    screen_init();

    // Display boot message
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("========================================");
    screen_println("          VOS - Minimal Kernel          ");
    screen_println("========================================");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    // Verify multiboot
    if (magic == 0x2BADB002) {
        screen_println("[OK] Multiboot verified");
    }

    // Initialize IDT and interrupts
    idt_init();
    screen_println("[OK] IDT initialized");

    // Initialize keyboard
    keyboard_init();
    screen_println("[OK] Keyboard initialized");

    // Enable interrupts
    sti();
    screen_println("[OK] Interrupts enabled");

    // Run the shell
    shell_run();

    // If shell exits, halt
    cli();
    for (;;) hlt();
}
```

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

# 13. Building a Shell

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

    screen_println("Welcome to VOS Shell!");
    screen_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();
        keyboard_getline(command_buffer, 256);
        execute_command(command_buffer);
    }
}
```

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
    } else if (strcmp(input, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(input, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(input, "basic") == 0) {
        cmd_basic();
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

# 15. Build System and Toolchain

## Makefile

```makefile
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

# Compiler flags
ASFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pie \
         -nostdlib -Wall -Wextra -I$(INCLUDE_DIR) -O2 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Source files
ASM_SOURCES = $(BOOT_DIR)/boot.asm
C_SOURCES = $(wildcard $(KERNEL_DIR)/*.c)

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Targets
all: vos.iso

$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm
    $(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
    $(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJECTS)
    $(LD) $(LDFLAGS) $(OBJECTS) -o $@

vos.iso: $(BUILD_DIR)/kernel.bin
    mkdir -p $(ISO_DIR)/boot/grub
    cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
    echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
    echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
    echo 'menuentry "VOS" { multiboot /boot/kernel.bin }' >> \
         $(ISO_DIR)/boot/grub/grub.cfg
    grub-mkrescue -o vos.iso $(ISO_DIR)

clean:
    rm -rf $(BUILD_DIR) vos.iso

.PHONY: all clean
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

# 16. Testing with VirtualBox and QEMU

## QEMU (Quick Testing)

```bash
# Run the ISO
make run
# or
qemu-system-i386 -cdrom vos.iso

# Debug mode (shows interrupts)
qemu-system-i386 -cdrom vos.iso -d int -no-reboot
```

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
| No output | Screen not initialized, wrong VGA address |
| Keyboard not working | PIC not remapped, IRQ masked, wrong IDT entry |
| Crashes on key press | Interrupt handler not returning properly |

---

# 17. Project Structure

```
vos/
├── boot/
│   └── boot.asm              # Multiboot header, entry point, ISRs
│
├── kernel/
│   ├── kernel.c              # Main kernel entry
│   ├── screen.c              # VGA text mode driver
│   ├── keyboard.c            # PS/2 keyboard driver
│   ├── idt.c                 # Interrupt Descriptor Table
│   ├── shell.c               # Command shell
│   ├── string.c              # String functions
│   ├── basic_io.c            # printf/exit for BASIC
│   ├── basic_programs.c      # Example BASIC programs
│   ├── tokenizer.c           # uBASIC tokenizer
│   └── ubasic.c              # uBASIC interpreter
│
├── include/
│   ├── types.h               # Basic types (uint8_t, etc.)
│   ├── screen.h              # Screen functions
│   ├── keyboard.h            # Keyboard functions
│   ├── idt.h                 # IDT functions
│   ├── io.h                  # Port I/O functions
│   ├── shell.h               # Shell functions
│   ├── string.h              # String functions
│   ├── ctype.h               # Character classification
│   ├── stdio.h               # printf wrapper
│   ├── stdlib.h              # atoi, exit
│   ├── ubasic.h              # BASIC interpreter
│   ├── tokenizer.h           # BASIC tokenizer
│   └── basic_programs.h      # Example programs
│
├── iso/boot/grub/
│   └── grub.cfg              # GRUB configuration
│
├── linker.ld                 # Linker script
├── Makefile                  # Build system
└── DOCUMENTATION.md          # This file
```

---

# 18. Future Enhancements

## Memory Management
- Physical memory manager (bitmap allocator)
- Virtual memory (paging)
- Heap allocator (malloc/free)

## File System
- RAM disk
- FAT12/FAT16 support
- VFS (Virtual File System) layer

## Process Management
- Task switching
- Scheduler
- System calls

## Drivers
- Mouse support
- Serial port (COM1)
- Real-time clock
- PC speaker

## Enhanced BASIC
- INPUT statement (user input)
- Arrays
- String variables
- Random numbers
- Graphics mode

## Networking
- NE2000 network card driver
- TCP/IP stack

---

# 19. Resources and References

## Books

- **Operating Systems: Design and Implementation** - Andrew Tanenbaum
- **Operating System Concepts** - Silberschatz, Galvin, Gagne
- **The Little Book About OS Development** - Erik Helin & Adam Renberg

## Online Resources

- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development wiki
- [Intel Software Developer Manuals](https://www.intel.com/sdm) - x86 architecture reference
- [Bran's Kernel Development Tutorial](http://www.osdever.net/bkerndev/)
- [James Molloy's Kernel Tutorials](http://www.jamesmolloy.co.uk/tutorial_html/)

## uBASIC

- [uBASIC by Adam Dunkels](https://dunkels.com/adam/ubasic/) - The BASIC interpreter we integrated
- BSD 3-Clause License

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
