#include "idt.h"
#include "io.h"

// IDT with 256 entries
static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void isr_default(void);
extern uint32_t isr_stub_table[32];
extern uint32_t irq_stub_table[16];
extern void isr128(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

// Remap the PIC (Programmable Interrupt Controller)
static void pic_remap(void) {
    // Save masks
    uint8_t mask1 = inb(0x21);
    uint8_t mask2 = inb(0xA1);

    // Start initialization sequence
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    // Set vector offsets
    outb(0x21, 0x20);  // Master PIC: IRQ 0-7 -> INT 32-39
    io_wait();
    outb(0xA1, 0x28);  // Slave PIC: IRQ 8-15 -> INT 40-47
    io_wait();

    // Set up cascading
    outb(0x21, 0x04);  // Master: slave at IRQ2
    io_wait();
    outb(0xA1, 0x02);  // Slave: cascade identity
    io_wait();

    // Set 8086 mode
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Restore masks (but enable IRQ1 for keyboard)
    outb(0x21, mask1);
    outb(0xA1, mask2);
}

void idt_init(void) {
    uint16_t code_selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_selector));

    // Set up IDT pointer
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    // Set default handler for all vectors
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default, code_selector, 0x8E);
    }

    // CPU exceptions (0-31)
    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], code_selector, 0x8E);
    }

    // Remap the PIC
    pic_remap();

    // Hardware IRQs (32-47)
    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32 + i), irq_stub_table[i], code_selector, 0x8E);
    }

    // Syscall gate (int 0x80) - callable from ring 3.
    idt_set_gate(0x80, (uint32_t)isr128, code_selector, 0xEE);

    // Mask all IRQs except timer (IRQ0) and keyboard (IRQ1).
    // Master PIC: unmask IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade)
    outb(0x21, 0xF8);
    // Slave PIC: mask all by default
    outb(0xA1, 0xFF);

    // Load the IDT
    idt_flush((uint32_t)&idtp);
}
