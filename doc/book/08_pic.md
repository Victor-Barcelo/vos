# Chapter 8: Programmable Interrupt Controller

## What is the PIC?

The 8259 Programmable Interrupt Controller (PIC) is a chip that manages hardware interrupts. It accepts interrupt requests from devices and signals the CPU.

PCs have two PICs in a cascade configuration, allowing 15 hardware interrupt lines:

```
+-------------------+       +-------------------+
|    Master PIC     |       |    Slave PIC      |
|   (Port 0x20)     |       |   (Port 0xA0)     |
+-------------------+       +-------------------+
| IRQ0: Timer       |       | IRQ8:  RTC        |
| IRQ1: Keyboard    |       | IRQ9:  ACPI       |
| IRQ2: Cascade ----+-------+ IRQ10: Available  |
| IRQ3: COM2        |       | IRQ11: Available  |
| IRQ4: COM1        |       | IRQ12: PS/2 Mouse |
| IRQ5: LPT2        |       | IRQ13: FPU        |
| IRQ6: Floppy      |       | IRQ14: Primary ATA|
| IRQ7: LPT1        |       | IRQ15: Second ATA |
+-------------------+       +-------------------+
```

IRQ2 on the master is connected to the slave PIC, giving us IRQs 8-15.

## The Problem: Conflicting Interrupt Numbers

By default, the PIC sends interrupts to these CPU vectors:

| IRQ | Default Vector |
|-----|----------------|
| 0-7 | 8-15 |
| 8-15 | 112-119 |

This conflicts with CPU exceptions (0-31). We must **remap** the PIC to use vectors 32-47.

## PIC Remapping

Remapping involves sending a sequence of Initialization Command Words (ICWs):

```c
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

static void io_wait(void) {
    // Write to unused port for small delay
    outb(0x80, 0);
}

void pic_remap(void) {
    // Save current masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: Start initialization sequence
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // ICW2: Set vector offsets
    outb(PIC1_DATA, 0x20);      // Master: IRQ 0-7 -> INT 32-39
    io_wait();
    outb(PIC2_DATA, 0x28);      // Slave: IRQ 8-15 -> INT 40-47
    io_wait();

    // ICW3: Configure cascade
    outb(PIC1_DATA, 0x04);      // Master: slave on IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02);      // Slave: cascade identity
    io_wait();

    // ICW4: Set 8086 mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Restore masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
```

### After Remapping

| IRQ | Vector |
|-----|--------|
| 0-7 | 32-39 |
| 8-15 | 40-47 |

Now hardware interrupts don't conflict with CPU exceptions.

## Initialization Command Words

### ICW1 (Command Port)

```
Bit 4: 1 = ICW4 needed
Bit 3: 0 = Cascade mode
Bit 2: 0 = 8-byte interrupt vectors
Bit 1: 0 = Cascade mode
Bit 0: 1 = ICW4 will be sent

Value: 0x11
```

### ICW2 (Data Port)

Sets the vector offset. Upper 5 bits of the vector number:

- Master: 0x20 (vectors 32-39)
- Slave: 0x28 (vectors 40-47)

### ICW3 (Data Port)

Configures cascade:

- Master: Bit mask of IRQ line connected to slave (0x04 = IRQ2)
- Slave: IRQ number on master (0x02)

### ICW4 (Data Port)

```
Bit 0: 1 = 8086 mode

Value: 0x01
```

## IRQ Masking

The PIC has an Interrupt Mask Register (IMR) that enables/disables individual IRQs. A **1 bit disables** the corresponding IRQ.

```c
// Enable only timer (IRQ0) and keyboard (IRQ1)
outb(PIC1_DATA, 0xFC);    // 11111100 - IRQ0,1 enabled
outb(PIC2_DATA, 0xFF);    // All slave IRQs disabled

// Later, enable mouse (IRQ12)
uint8_t mask = inb(PIC2_DATA);
mask &= ~(1 << 4);        // IRQ12 = slave IRQ 4
outb(PIC2_DATA, mask);
```

### VOS Default Mask

```c
// Enable: Timer, Keyboard, Cascade, ATA primary/secondary
outb(PIC1_DATA, 0xF8);    // Timer, Keyboard, Cascade
outb(PIC2_DATA, 0x3F);    // ATA drives
```

## End of Interrupt (EOI)

After handling an IRQ, we must signal the PIC that we're done:

```c
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // Send EOI to slave PIC
        outb(PIC2_COMMAND, 0x20);
    }
    // Send EOI to master PIC
    outb(PIC1_COMMAND, 0x20);
}
```

### Why EOI Matters

The PIC tracks which interrupts are being serviced. Without EOI:

- That IRQ line stays masked
- No more interrupts from that device
- Eventually, the system appears hung

### EOI in Interrupt Handler

```c
if (int_no >= 32 && int_no < 48) {
    uint8_t irq = int_no - 32;

    // Handle the interrupt
    if (irq_handlers[irq]) {
        irq_handlers[irq](frame);
    }

    // Send EOI
    pic_send_eoi(irq);
}
```

## Spurious Interrupts

The PIC can generate spurious interrupts (usually IRQ7 or IRQ15) due to electrical noise or timing issues.

### Detecting Spurious Interrupts

Check the In-Service Register (ISR):

```c
uint8_t pic_get_isr(void) {
    outb(PIC1_COMMAND, 0x0B);
    return inb(PIC1_COMMAND);
}

void irq7_handler(void) {
    // Check if this is a real IRQ7
    if (!(pic_get_isr() & 0x80)) {
        // Spurious interrupt - no EOI needed
        return;
    }

    // Real IRQ7 - handle it
    // ...
    pic_send_eoi(7);
}
```

For spurious IRQ7, don't send EOI. For spurious IRQ15, send EOI only to master.

## Common IRQs in VOS

| IRQ | Vector | Device | Handler |
|-----|--------|--------|---------|
| 0 | 32 | Timer (PIT) | Scheduler tick, uptime |
| 1 | 33 | Keyboard | Key input |
| 12 | 44 | PS/2 Mouse | Mouse input |
| 14 | 46 | Primary ATA | Disk I/O |
| 15 | 47 | Secondary ATA | Disk I/O |

## Registering IRQ Handlers

VOS provides a simple handler registration:

```c
typedef void (*irq_handler_t)(interrupt_frame_t *frame);
static irq_handler_t irq_handlers[16] = {0};

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
}

// Example: Register keyboard handler
void keyboard_init(void) {
    irq_register_handler(1, keyboard_irq_handler);

    // Unmask IRQ1
    uint8_t mask = inb(PIC1_DATA);
    mask &= ~(1 << 1);
    outb(PIC1_DATA, mask);
}
```

## Alternative: APIC

Modern systems use the APIC (Advanced PIC) instead of the 8259:

- **LAPIC**: Local APIC in each CPU core
- **I/O APIC**: Handles external interrupts

APIC advantages:
- More interrupt lines
- Better SMP support
- Message-based (no physical lines)

VOS uses the 8259 PIC for simplicity and compatibility with older/virtual hardware.

## Debugging PIC Issues

### Symptom: No Timer Interrupts

```c
// Check mask
uint8_t mask = inb(PIC1_DATA);
serial_printf("PIC1 mask: %02x\n", mask);
// Bit 0 should be 0 (IRQ0 enabled)
```

### Symptom: Keyboard Not Working

```c
// Check mask and IRR (Interrupt Request Register)
outb(PIC1_COMMAND, 0x0A);
uint8_t irr = inb(PIC1_COMMAND);
serial_printf("PIC1 IRR: %02x\n", irr);
// Bit 1 set means keyboard interrupt pending
```

### Symptom: System Hangs After Interrupt

Usually means EOI wasn't sent:

```c
// Add debug logging
void irq_handler(uint8_t irq) {
    serial_printf("IRQ %d: handling\n", irq);
    // ... handle ...
    serial_printf("IRQ %d: sending EOI\n", irq);
    pic_send_eoi(irq);
    serial_printf("IRQ %d: done\n", irq);
}
```

## Summary

The PIC is essential for hardware interrupt management:

1. **Remap** IRQs to avoid conflicting with exceptions
2. **Mask** unused IRQs for efficiency
3. **Send EOI** after handling each interrupt
4. **Handle spurious** interrupts correctly

While simple, the 8259 PIC is sufficient for VOS and teaches fundamental interrupt controller concepts.

---

*Previous: [Chapter 7: Interrupts and Exceptions](07_interrupts.md)*
*Next: [Chapter 9: Memory Management](09_memory.md)*
