; Multiboot header constants
MBALIGN  equ 1 << 0                 ; Align loaded modules on page boundaries
MEMINFO  equ 1 << 1                 ; Provide memory map
VIDMODE  equ 1 << 2                 ; Request a video mode (graphics)
FLAGS    equ MBALIGN | MEMINFO | VIDMODE ; Multiboot flags
MAGIC    equ 0x1BADB002             ; Magic number for bootloader
CHECKSUM equ -(MAGIC + FLAGS)       ; Checksum to prove we are multiboot

; Multiboot header section
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0                            ; mode_type: 0=graphics, 1=text
    dd 1920                         ; width
    dd 1080                         ; height
    dd 32                           ; depth (bits per pixel)

; Stack setup
section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KB stack
global stack_top
stack_top:

; Entry point
section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot info pointer and magic number
    push ebx                        ; Multiboot info structure
    push eax                        ; Multiboot magic number

    ; Call the kernel main function
    call kernel_main

    ; If kernel returns, hang the system
    cli
.hang:
    hlt
    jmp .hang

; GDT and IDT helper functions
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]              ; Get GDT pointer
    lgdt [eax]                      ; Load GDT
    mov ax, 0x10                    ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush                 ; Far jump to code segment
.flush:
    ret

global tss_flush
tss_flush:
    mov ax, [esp + 4]              ; TSS selector
    ltr ax
    ret

global idt_flush
idt_flush:
    mov eax, [esp + 4]              ; Get IDT pointer
    lidt [eax]                      ; Load IDT
    ret

; Interrupt/exception stubs
global isr_default
extern interrupt_handler

; Default handler for unhandled interrupts (no error code frame)
isr_default:
    iret

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0                    ; Dummy error code
    push dword %1                   ; Interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1                   ; Interrupt number (CPU already pushed error code)
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push dword 0                    ; Dummy error code
    push dword %2                   ; Interrupt number (PIC remapped vector)
    jmp isr_common_stub
%endmacro

; CPU exceptions (ISRs 0-31)
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Syscall interrupt (int 0x80)
ISR_NOERRCODE 128

; Hardware IRQs (mapped to vectors 32-47)
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; Tables of stub addresses for C setup
global isr_stub_table
isr_stub_table:
    dd isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    dd isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    dd isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dd isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

global irq_stub_table
irq_stub_table:
    dd irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
    dd irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

; Common entry point: builds an interrupt frame and calls into C
isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10                    ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp
    call interrupt_handler
    add esp, 4
    mov esp, eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                    ; Pop int_no and err_code
    iret

; Mark stack as non-executable (for linker)
section .note.GNU-stack noalloc noexec nowrite progbits
