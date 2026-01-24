# Chapter 7: Interrupts and Exceptions

## What are Interrupts?

Interrupts are signals that temporarily stop the CPU to handle events. They're fundamental to operating systems for:

- Responding to hardware (keyboard, timer, disk)
- Handling CPU errors (divide by zero, page fault)
- Implementing system calls

### Types of Interrupts

1. **Hardware Interrupts (IRQs)**: From devices (keyboard, timer, disk)
2. **Software Interrupts**: Triggered by `int` instruction
3. **Exceptions**: CPU errors (divide by zero, page fault)

## The Interrupt Descriptor Table (IDT)

The IDT is an array of 256 entries, each describing how to handle an interrupt:

```
+-------+-----------------+---------------------------------+
| Entry |  Type           |  Description                    |
+-------+-----------------+---------------------------------+
| 0-31  |  Exceptions     |  CPU errors (divide, page fault)|
| 32-47 |  Hardware IRQs  |  Remapped PIC interrupts        |
| 48-127|  Available      |  Free for OS use                |
| 128   |  Syscall        |  int 0x80 for system calls      |
|129-255|  Available      |  Free for OS use                |
+-------+-----------------+---------------------------------+
```

## IDT Entry Structure

Each IDT entry (gate descriptor) is 8 bytes:

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
+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
|  P  |   DPL     |  0  |        Type           |
+-----+-----------+-----+-----------------------+

P (Present):     1 = Entry is valid
DPL:             Privilege level required to call (0 = kernel only, 3 = user)
Type:            0xE = 32-bit interrupt gate
                 0xF = 32-bit trap gate
```

### Gate Types

- **Interrupt Gate (0xE)**: Disables interrupts on entry (CLI implicit)
- **Trap Gate (0xF)**: Keeps interrupt flag unchanged

VOS uses interrupt gates for hardware interrupts (need atomicity) and the syscall (to prevent races).

## VOS IDT Configuration

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

    struct idt_ptr idtp = {
        .limit = sizeof(idt) - 1,
        .base = (uint32_t)&idt
    };

    // Default handler for all interrupts
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default, cs, 0x8E);
    }

    // CPU exceptions (0-31)
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], cs, 0x8E);
    }

    // Remap PIC
    pic_remap();

    // Hardware IRQs (32-47)
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, irq_stub_table[i], cs, 0x8E);
    }

    // Syscall gate (int 0x80) - callable from ring 3
    idt_set_gate(0x80, (uint32_t)isr128, cs, 0xEE);

    // Load IDT
    idt_flush((uint32_t)&idtp);
}
```

### Flag Values

| Purpose | Flags | Meaning |
|---------|-------|---------|
| Kernel-only interrupt | 0x8E | P=1, DPL=0, Type=0xE |
| User-callable syscall | 0xEE | P=1, DPL=3, Type=0xE |

The syscall gate has DPL=3 so user programs can execute `int 0x80`.

## CPU Exceptions (0-31)

| Vector | Name | Error Code | Description |
|--------|------|------------|-------------|
| 0 | Divide Error | No | Division by zero |
| 1 | Debug | No | Debug breakpoint |
| 2 | NMI | No | Non-maskable interrupt |
| 3 | Breakpoint | No | INT3 instruction |
| 4 | Overflow | No | INTO instruction |
| 5 | Bound Range | No | BOUND instruction |
| 6 | Invalid Opcode | No | Undefined instruction |
| 7 | Device Not Available | No | FPU not present |
| 8 | Double Fault | Yes (0) | Exception during exception |
| 9 | (Reserved) | No | |
| 10 | Invalid TSS | Yes | Bad TSS |
| 11 | Segment Not Present | Yes | Missing segment |
| 12 | Stack Fault | Yes | Stack segment fault |
| 13 | General Protection | Yes | Protection violation |
| 14 | Page Fault | Yes | Page not present or protected |
| 15 | (Reserved) | No | |
| 16 | x87 FPU Error | No | Floating point exception |
| 17 | Alignment Check | Yes | Unaligned access |
| 18 | Machine Check | No | Hardware error |
| 19 | SIMD Exception | No | SSE floating point |
| 20-31 | (Reserved) | | |

### Exceptions with Error Codes

Some exceptions push an error code onto the stack. VOS normalizes this by having the assembly stub push 0 for exceptions that don't have error codes.

## The Interrupt Frame

When an interrupt occurs, VOS builds a complete CPU state snapshot:

```c
typedef struct {
    // Pushed by isr_common_stub
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;

    // Pushed by stub macros
    uint32_t int_no;
    uint32_t err_code;

    // Pushed by CPU
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;

    // Only if privilege change (ring 3 -> ring 0)
    uint32_t user_esp;
    uint32_t user_ss;
} interrupt_frame_t;
```

This structure is used for:
- **Panic dumps**: Print register state on crash
- **Syscalls**: Read arguments, write return value
- **Context switching**: Save and restore task state

## Interrupt Handler Dispatch

The central dispatcher routes interrupts to appropriate handlers:

```c
interrupt_frame_t* interrupt_handler(interrupt_frame_t *frame) {
    uint32_t int_no = frame->int_no;

    // CPU exceptions (0-31)
    if (int_no < 32) {
        if (int_no == 14) {
            // Page fault - could be handled
            handle_page_fault(frame);
        } else {
            // Fatal exception
            panic_with_frame(exception_names[int_no], frame);
        }
    }

    // Syscall (0x80 = 128)
    else if (int_no == 0x80) {
        frame = syscall_handle(frame);
    }

    // Hardware IRQs (32-47)
    else if (int_no >= 32 && int_no < 48) {
        uint8_t irq = int_no - 32;

        // Call registered handler
        if (irq_handlers[irq]) {
            irq_handlers[irq](frame);
        }

        // Timer (IRQ0) triggers scheduler
        if (irq == 0) {
            frame = tasking_on_timer_tick(frame);
        }

        // Send End of Interrupt to PIC
        if (irq >= 8) {
            outb(0xA0, 0x20);  // Slave PIC
        }
        outb(0x20, 0x20);      // Master PIC
    }

    return frame;
}
```

### Context Switching via Return Value

The interrupt handler returns a frame pointer. If it returns a different frame than it received, the assembly stub will resume that frame instead - this is how context switching works.

## The Syscall Interface

VOS uses `int 0x80` for system calls:

### Convention

- **EAX**: Syscall number
- **EBX, ECX, EDX, ESI, EDI**: Arguments
- **EAX**: Return value (negative = error)

### Syscall Handler

```c
interrupt_frame_t* syscall_handle(interrupt_frame_t *frame) {
    uint32_t syscall_num = frame->eax;
    uint32_t arg1 = frame->ebx;
    uint32_t arg2 = frame->ecx;
    uint32_t arg3 = frame->edx;

    int32_t result;

    switch (syscall_num) {
        case SYS_EXIT:
            task_exit((int)arg1);
            // Never returns
            break;

        case SYS_WRITE:
            result = sys_write((int)arg1, (void*)arg2, (size_t)arg3);
            break;

        case SYS_READ:
            result = sys_read((int)arg1, (void*)arg2, (size_t)arg3);
            break;

        // ... 68 more syscalls ...

        default:
            result = -ENOSYS;
    }

    frame->eax = (uint32_t)result;
    return frame;
}
```

## Page Fault Handling

Page faults (interrupt 14) are special - they can be recoverable:

```c
void handle_page_fault(interrupt_frame_t *frame) {
    uint32_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    uint32_t err = frame->err_code;
    bool present = err & 0x1;       // Page was present
    bool write = err & 0x2;         // Write access
    bool user = err & 0x4;          // User mode
    bool reserved = err & 0x8;      // Reserved bits set
    bool fetch = err & 0x10;        // Instruction fetch

    // Check if this is a valid user-mode fault we can handle
    if (user && current_task) {
        // Could implement copy-on-write, demand paging, etc.
        // For now, kill the process
        task_signal(current_task, SIGSEGV);
        return;
    }

    // Kernel page fault - fatal
    serial_printf("Page fault at 0x%x, error=%x\n", fault_addr, err);
    panic_with_frame("Page Fault", frame);
}
```

### CR2 Register

On page fault, the CPU stores the faulting virtual address in CR2. This tells us exactly which address caused the fault.

### Error Code Bits

| Bit | Meaning if Set |
|-----|----------------|
| 0 | Page was present (protection violation) |
| 1 | Write access |
| 2 | User mode |
| 3 | Reserved bits set in page table |
| 4 | Instruction fetch |

## Enabling Interrupts

After setting up the IDT and PIC:

```c
// Enable interrupts
static inline void sti(void) {
    __asm__ volatile("sti");
}

// Disable interrupts
static inline void cli(void) {
    __asm__ volatile("cli");
}
```

VOS enables interrupts after:
1. GDT/TSS installed
2. IDT installed
3. PIC remapped and configured
4. Timer and keyboard handlers registered

## Summary

Interrupts are the foundation of a responsive operating system:

1. **IDT** maps interrupt numbers to handlers
2. **Exceptions** handle CPU errors
3. **IRQs** handle hardware events
4. **Syscalls** provide user-kernel interface
5. **Interrupt frames** save/restore CPU state
6. **Context switching** uses the same mechanism

The uniform interrupt frame design makes VOS's interrupt handling clean and enables features like preemptive multitasking.

---

*Previous: [Chapter 6: Segmentation: GDT and TSS](06_gdt_tss.md)*
*Next: [Chapter 8: Programmable Interrupt Controller](08_pic.md)*
