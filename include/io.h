#ifndef IO_H
#define IO_H

#include "types.h"

// Output a byte to a port
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Input a byte from a port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Output a word to a port
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

// Input a word from a port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// I/O wait (small delay)
static inline void io_wait(void) {
    outb(0x80, 0);
}

// Enable interrupts
static inline void sti(void) {
    __asm__ volatile ("sti");
}

// Disable interrupts
static inline void cli(void) {
    __asm__ volatile ("cli");
}

// Halt the CPU
static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

// Save EFLAGS and disable interrupts
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile (
        "pushf\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

// Restore EFLAGS
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile (
        "push %0\n\t"
        "popf"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

static inline bool irq_are_enabled(void) {
    uint32_t flags;
    __asm__ volatile (
        "pushf\n\t"
        "pop %0"
        : "=r"(flags)
        :
        : "memory"
    );
    return (flags & (1u << 9)) != 0;
}

#endif
