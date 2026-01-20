#ifndef GDT_H
#define GDT_H

#include "types.h"

void gdt_init(void);
void tss_set_kernel_stack(uint32_t stack_top);

extern void gdt_flush(uint32_t gdt_ptr);
extern void tss_flush(uint16_t tss_selector);

#endif

