# Chapter 5: Assembly Entry Point

## The _start Function

Our kernel's entry point is written in assembly because we need precise control over the initial CPU state. The bootloader jumps here after loading the kernel.

```nasm
section .text
global _start
extern kernel_main

_start:
    ; We are in 32-bit protected mode
    ; EAX = Multiboot magic (0x2BADB002)
    ; EBX = Multiboot info pointer
    ; Interrupts are disabled
    ; Stack is undefined

    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot info for kernel_main
    push ebx                    ; Multiboot info structure
    push eax                    ; Multiboot magic number

    ; Call the kernel main function
    call kernel_main

    ; If kernel returns, halt forever
    cli                         ; Disable interrupts
.hang:
    hlt                         ; Halt CPU until interrupt
    jmp .hang                   ; Loop forever (in case of NMI)
```

## Why Assembly?

We must use assembly for the entry point because:

1. **No stack exists yet** - We can't call C functions without a stack
2. **Must save bootloader info** - EAX/EBX contain Multiboot data
3. **Precise control needed** - Must set up correct state for C

Once we establish a stack and call `kernel_main()`, we can use C for everything else.

## Stack Setup

The stack grows downward in x86. We reserve space in the BSS section:

```nasm
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB stack
stack_top:
```

### Stack Layout

```
Higher addresses
+------------------+
|    stack_top     | <- ESP starts here
+------------------+
|                  |
|   16KB of space  | <- Stack grows DOWN
|                  |
+------------------+
|   stack_bottom   |
+------------------+
Lower addresses
```

### Why 16-byte Alignment?

The System V ABI requires the stack to be 16-byte aligned before a `call` instruction. This ensures:

- SSE instructions work correctly (require 16-byte alignment)
- Function calls follow the expected convention
- Compiler-generated code works properly

## Understanding the Sections

Our boot.asm uses several sections:

| Section | Purpose |
|---------|---------|
| `.multiboot` | Contains the Multiboot header (must be first) |
| `.text` | Executable code |
| `.rodata` | Read-only data (constants, strings) |
| `.data` | Initialized read-write data |
| `.bss` | Uninitialized data (stack, buffers) |

### Section Order in Linker Script

The linker script ensures proper ordering:

```ld
SECTIONS
{
    . = 1M;                     /* Load at 1MB */

    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)           /* Multiboot header first! */
    }

    .text BLOCK(4K) : ALIGN(4K)
    {
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

    __kernel_end = .;           /* Symbol marking kernel end */
}
```

## Interrupt and IRQ Stubs

VOS also defines interrupt stubs in assembly. These create a uniform stack frame for all interrupts:

### Exception Stubs (0-31)

Some exceptions push an error code, others don't. We normalize this:

```nasm
; Exceptions WITHOUT error code
%macro ISR_NOERRCODE 1
isr%1:
    push dword 0               ; Push dummy error code
    push dword %1              ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Exceptions WITH error code
%macro ISR_ERRCODE 1
isr%1:
    ; Error code already pushed by CPU
    push dword %1              ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Generate stubs for all exceptions
ISR_NOERRCODE 0                ; Division by zero
ISR_NOERRCODE 1                ; Debug
ISR_NOERRCODE 2                ; NMI
; ... etc ...
ISR_ERRCODE 8                  ; Double fault (has error code)
; ... etc ...
ISR_ERRCODE 14                 ; Page fault (has error code)
```

### IRQ Stubs (32-47)

Hardware interrupts are simpler - they never have error codes:

```nasm
%macro IRQ 2
irq%1:
    push dword 0               ; Dummy error code
    push dword %2              ; Interrupt number (32+irq)
    jmp isr_common_stub
%endmacro

IRQ 0, 32                      ; Timer
IRQ 1, 33                      ; Keyboard
; ... etc ...
```

### Common Stub

All interrupts go through a common handler:

```nasm
isr_common_stub:
    ; Save all registers
    pusha

    ; Save segment registers
    push ds
    push es
    push fs
    push gs

    ; Load kernel data segment
    mov ax, 0x10               ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push pointer to stack frame
    push esp

    ; Call C handler
    extern interrupt_handler
    call interrupt_handler

    ; Handler returns (possibly new) stack pointer in EAX
    mov esp, eax

    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore general registers
    popa

    ; Remove interrupt number and error code
    add esp, 8

    ; Return from interrupt
    iret
```

## The Interrupt Frame

The stack after `isr_common_stub` saves registers:

```c
typedef struct {
    // Pushed by us (isr_common_stub)
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;  // pusha
    uint32_t ebx, edx, ecx, eax;        // pusha

    // Pushed by stub macros
    uint32_t int_no;
    uint32_t err_code;

    // Pushed by CPU
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;

    // Only present if privilege change (ring 3 -> ring 0)
    uint32_t user_esp;
    uint32_t user_ss;
} interrupt_frame_t;
```

This structure is central to:
- Exception handling (panic dumps)
- System calls (reading arguments, returning values)
- Task switching (saving/restoring context)

## GDT and IDT Loading

Assembly code loads the GDT and IDT using special instructions:

```nasm
; Load GDT
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]         ; Get GDT pointer argument
    lgdt [eax]                 ; Load GDT register

    ; Reload segment registers
    mov ax, 0x10               ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    jmp 0x08:.flush            ; Kernel code selector
.flush:
    ret

; Load IDT
global idt_flush
idt_flush:
    mov eax, [esp + 4]         ; Get IDT pointer argument
    lidt [eax]                 ; Load IDT register
    ret
```

## TSS Loading

```nasm
global tss_flush
tss_flush:
    mov ax, 0x28               ; TSS selector (index 5 in GDT)
    ltr ax                     ; Load task register
    ret
```

## User Mode Entry

When returning to user mode, we build a stack frame and use `iret`:

```nasm
global enter_usermode
enter_usermode:
    ; Arguments: entry_point, user_stack

    cli                        ; Disable interrupts during setup

    mov ax, 0x23               ; User data selector (ring 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build iret frame
    push dword 0x23            ; SS (user data)
    push dword [esp + 12]      ; ESP (user stack)
    pushf                      ; EFLAGS
    pop eax
    or eax, 0x200              ; Enable interrupts in user mode
    push eax
    push dword 0x1B            ; CS (user code)
    push dword [esp + 16]      ; EIP (entry point)

    iret                       ; "Return" to user mode
```

## Summary

The assembly entry point:

1. **Receives control from GRUB** with Multiboot info
2. **Sets up the initial stack** for C code
3. **Calls kernel_main()** with Multiboot parameters
4. **Provides interrupt stubs** for uniform exception handling
5. **Loads segment tables** (GDT, IDT, TSS)
6. **Enables user mode transitions** via iret

This assembly code is the bridge between the bootloader and our C kernel.

---

*Previous: [Chapter 4: The Multiboot Specification](04_multiboot.md)*
*Next: [Chapter 6: Segmentation: GDT and TSS](06_gdt_tss.md)*
