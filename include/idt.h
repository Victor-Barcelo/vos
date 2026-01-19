#ifndef IDT_H
#define IDT_H

#include "types.h"

// IDT entry structure
struct idt_entry {
    uint16_t base_low;    // Lower 16 bits of handler address
    uint16_t selector;    // Kernel segment selector
    uint8_t  zero;        // Always zero
    uint8_t  flags;       // Flags
    uint16_t base_high;   // Upper 16 bits of handler address
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;       // Size of IDT - 1
    uint32_t base;        // Address of IDT
} __attribute__((packed));

// Initialize the IDT
void idt_init(void);

// Set an IDT gate
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);

// Assembly functions (defined in boot.asm)
extern void idt_flush(uint32_t);
extern void isr_default(void);
extern void isr_timer(void);
extern void isr_keyboard(void);

#endif
