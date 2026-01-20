#include "gdt.h"
#include "string.h"

typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

static gdt_entry_t gdt[6];
static gdt_ptr_t gdtp;
static tss_entry_t tss;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (uint16_t)(base & 0xFFFFu);
    gdt[num].base_middle = (uint8_t)((base >> 16) & 0xFFu);
    gdt[num].base_high = (uint8_t)((base >> 24) & 0xFFu);

    gdt[num].limit_low = (uint16_t)(limit & 0xFFFFu);
    gdt[num].granularity = (uint8_t)((limit >> 16) & 0x0Fu);
    gdt[num].granularity |= (uint8_t)(gran & 0xF0u);

    gdt[num].access = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = ss0;
    tss.esp0 = esp0;
    tss.iomap_base = (uint16_t)sizeof(tss);

    uint32_t base = (uint32_t)&tss;
    uint32_t limit = (uint32_t)sizeof(tss) - 1u;
    gdt_set_gate(num, base, limit, 0x89, 0x00);
}

void tss_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}

void gdt_init(void) {
    gdtp.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdtp.base = (uint32_t)&gdt;

    // 0: null
    gdt_set_gate(0, 0, 0, 0, 0);
    // 1: kernel code
    gdt_set_gate(1, 0, 0xFFFFFu, 0x9Au, 0xCFu);
    // 2: kernel data
    gdt_set_gate(2, 0, 0xFFFFFu, 0x92u, 0xCFu);
    // 3: user code
    gdt_set_gate(3, 0, 0xFFFFFu, 0xFAu, 0xCFu);
    // 4: user data
    gdt_set_gate(4, 0, 0xFFFFFu, 0xF2u, 0xCFu);
    // 5: TSS
    write_tss(5, 0x10, 0);

    gdt_flush((uint32_t)&gdtp);
    tss_flush(0x28);
}
