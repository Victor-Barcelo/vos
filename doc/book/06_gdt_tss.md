# Chapter 6: Segmentation: GDT and TSS

## What is the GDT?

The Global Descriptor Table (GDT) defines memory segments in Protected Mode. Each entry describes:

- **Base address** of the segment
- **Limit** (size) of the segment
- **Access rights** and flags

While modern systems use paging for memory protection, the GDT is still required in Protected Mode for:

- Defining code and data segments
- Establishing privilege levels (rings)
- Task State Segment for context switching

## GDT Entry Structure

Each GDT entry is 8 bytes with a complex layout:

```
Byte 7    Byte 6    Byte 5    Byte 4    Byte 3    Byte 2    Byte 1    Byte 0
+--------+--------+--------+--------+--------+--------+--------+--------+
| Base   | Flags  | Limit  | Access | Base   | Base   | Limit  | Limit  |
| 31-24  | + Lim  | 19-16  | Byte   | 23-16  | 15-8   | 15-8   | 7-0    |
|        | 19-16  |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Access Byte

```
Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
+-------+-------+-------+-------+-------+-------+-------+-------+
|   P   |  DPL  |  DPL  |   S   |   E   |  DC   |  RW   |   A   |
+-------+-------+-------+-------+-------+-------+-------+-------+
```

| Bit | Name | Description |
|-----|------|-------------|
| P | Present | 1 = valid segment |
| DPL | Privilege | 0 = kernel, 3 = user |
| S | Descriptor type | 1 = code/data, 0 = system |
| E | Executable | 1 = code, 0 = data |
| DC | Direction/Conforming | Data: 0=grow up; Code: 0=non-conforming |
| RW | Read/Write | Code: readable; Data: writable |
| A | Accessed | Set by CPU when accessed |

### Flags Nibble

```
Bit 3   Bit 2   Bit 1   Bit 0
+-------+-------+-------+-------+
|   G   |  DB   |   L   |  AVL  |
+-------+-------+-------+-------+
```

| Bit | Name | Description |
|-----|------|-------------|
| G | Granularity | 0 = 1B, 1 = 4KB units |
| DB | Size | 0 = 16-bit, 1 = 32-bit |
| L | Long mode | 0 for 32-bit |
| AVL | Available | For OS use |

## VOS GDT Layout

VOS uses a flat segmentation model with five entries:

| Index | Selector | Description |
|-------|----------|-------------|
| 0 | 0x00 | Null descriptor (required) |
| 1 | 0x08 | Kernel code (ring 0) |
| 2 | 0x10 | Kernel data (ring 0) |
| 3 | 0x18 | User code (ring 3) |
| 4 | 0x20 | User data (ring 3) |
| 5 | 0x28 | TSS descriptor |

### Selector Format

A segment selector is 16 bits:

```
+-------+-------+-------+-------+-------+-------+-------+-------+
|              Index (13 bits)          |  TI   |     RPL       |
+-------+-------+-------+-------+-------+-------+-------+-------+
```

- **Index**: Entry number in GDT
- **TI**: Table Indicator (0 = GDT, 1 = LDT)
- **RPL**: Requested Privilege Level (0-3)

For kernel code (entry 1): `0x08 = 0000 0000 0000 1000`
For user code (entry 3): `0x1B = 0000 0000 0001 1011` (RPL=3)

## Implementation

### GDT Entry Setup

```c
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;  // Includes limit high + flags
    uint8_t  base_high;
} __attribute__((packed));

static struct gdt_entry gdt[6];

void gdt_set_entry(int num, uint32_t base, uint32_t limit,
                   uint8_t access, uint8_t flags) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (flags << 4);

    gdt[num].access = access;
}
```

### GDT Initialization

```c
void gdt_init(void) {
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel code: base=0, limit=4GB, ring 0, executable
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x0C);

    // Kernel data: base=0, limit=4GB, ring 0, writable
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x0C);

    // User code: base=0, limit=4GB, ring 3, executable
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0x0C);

    // User data: base=0, limit=4GB, ring 3, writable
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x0C);

    // TSS (set up separately)
    write_tss(5, 0x10, 0);

    // Load the GDT
    struct gdt_ptr gdtp = {
        .limit = sizeof(gdt) - 1,
        .base = (uint32_t)&gdt
    };
    gdt_flush((uint32_t)&gdtp);

    // Load TSS
    tss_flush();
}
```

### Access Byte Values

| Segment | Access | Binary | Meaning |
|---------|--------|--------|---------|
| Kernel code | 0x9A | 10011010 | Present, ring 0, code, readable |
| Kernel data | 0x92 | 10010010 | Present, ring 0, data, writable |
| User code | 0xFA | 11111010 | Present, ring 3, code, readable |
| User data | 0xF2 | 11110010 | Present, ring 3, data, writable |

## Task State Segment (TSS)

The TSS is critical for user mode support. When a user-mode task (ring 3) executes a syscall (`int 0x80`) or takes an interrupt, the CPU must transition to ring 0. This requires a **trusted kernel stack**.

### TSS Structure

```c
struct tss_entry {
    uint32_t prev_tss;    // Previous TSS (for hardware task switching)
    uint32_t esp0;        // Stack pointer for ring 0
    uint32_t ss0;         // Stack segment for ring 0
    uint32_t esp1;        // Stack pointer for ring 1 (unused)
    uint32_t ss1;
    uint32_t esp2;        // Stack pointer for ring 2 (unused)
    uint32_t ss2;
    uint32_t cr3;         // Page directory
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));
```

### What We Actually Use

VOS only uses two TSS fields:

- **ss0**: The kernel data selector (0x10) for the stack
- **esp0**: The kernel stack pointer to load on ring transition

The rest is for hardware task switching, which VOS doesn't use.

### TSS Setup

```c
static struct tss_entry tss;

void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    // Clear TSS
    memset(&tss, 0, sizeof(tss));

    // Set stack for ring 0
    tss.ss0 = ss0;
    tss.esp0 = esp0;

    // Set segment selectors
    tss.cs = 0x08 | 3;  // Kernel code, RPL 3
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x10 | 3;

    // I/O permission bitmap
    tss.iomap_base = sizeof(tss);

    // Install in GDT
    // TSS descriptor has special access byte format
    gdt_set_entry(num, base, limit, 0xE9, 0x00);
}
```

### Updating ESP0 on Task Switch

Each task needs its own kernel stack. VOS updates the TSS on every context switch:

```c
void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}

void switch_to_task(task_t *next) {
    // Update TSS with this task's kernel stack
    tss_set_kernel_stack(next->kernel_stack_top);

    // ... perform context switch ...
}
```

## Ring Transitions

### User to Kernel (Interrupt/Syscall)

When a ring 3 program triggers an interrupt:

1. CPU reads TSS.ss0 and TSS.esp0
2. Switches to kernel stack
3. Pushes user SS, ESP, EFLAGS, CS, EIP
4. Jumps to interrupt handler

### Kernel to User (IRET)

When returning to user mode:

1. IRET pops EIP, CS, EFLAGS
2. Detects privilege change (CS RPL = 3)
3. Also pops ESP, SS
4. Continues in ring 3

## Privilege Levels

x86 has four privilege levels (rings):

```
+----------------+
|    Ring 0      |  Kernel - Full hardware access
+----------------+
|    Ring 1      |  (Unused in most OSes)
+----------------+
|    Ring 2      |  (Unused in most OSes)
+----------------+
|    Ring 3      |  User - Restricted access
+----------------+
```

VOS uses only rings 0 (kernel) and 3 (user programs).

### CPL, DPL, and RPL

- **CPL** (Current Privilege Level): Stored in CS bits 0-1
- **DPL** (Descriptor Privilege Level): In segment descriptor
- **RPL** (Requested Privilege Level): In segment selector

Access is allowed when: `max(CPL, RPL) <= DPL`

## Flat Model

VOS uses a "flat" memory model:

- All segments have base = 0
- All segments have limit = 4GB
- No segment translation needed

This means segmentation is essentially disabled, and we rely on paging for memory protection. The GDT exists only because Protected Mode requires it.

## Summary

The GDT and TSS provide:

1. **Segment definitions** for Protected Mode
2. **Privilege levels** for kernel/user separation
3. **Stack switching** on ring transitions
4. **Foundation for user mode** execution

Even though VOS uses flat segmentation, the GDT is essential for:
- Distinguishing kernel from user code
- Safe syscall and interrupt handling
- Context switching between tasks

---

*Previous: [Chapter 5: Assembly Entry Point](05_assembly_entry.md)*
*Next: [Chapter 7: Interrupts and Exceptions](07_interrupts.md)*
