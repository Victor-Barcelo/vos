; Multiboot header constants
MBALIGN  equ 1 << 0                 ; Align loaded modules on page boundaries
MEMINFO  equ 1 << 1                 ; Provide memory map
FLAGS    equ MBALIGN | MEMINFO      ; Multiboot flags
MAGIC    equ 0x1BADB002             ; Magic number for bootloader
CHECKSUM equ -(MAGIC + FLAGS)       ; Checksum to prove we are multiboot

; Multiboot header section
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Stack setup
section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KB stack
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

global idt_flush
idt_flush:
    mov eax, [esp + 4]              ; Get IDT pointer
    lidt [eax]                      ; Load IDT
    ret

; Interrupt handlers
global isr_keyboard
global isr_default
global isr_timer
extern keyboard_handler

; Default handler for unhandled interrupts
isr_default:
    iret

; Timer interrupt handler (IRQ0 = INT 32)
isr_timer:
    push eax
    mov al, 0x20                    ; Send EOI to PIC
    out 0x20, al
    pop eax
    iret

; Keyboard interrupt handler (IRQ1 = INT 33)
isr_keyboard:
    pusha                           ; Save all registers
    call keyboard_handler           ; Call C handler
    popa                            ; Restore registers
    iret                            ; Return from interrupt

; Mark stack as non-executable (for linker)
section .note.GNU-stack noalloc noexec nowrite progbits
