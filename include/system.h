#ifndef SYSTEM_H
#define SYSTEM_H

#include "types.h"

void system_init(uint32_t multiboot_magic, uint32_t* mboot_info);

uint32_t system_mem_total_kb(void);
const char* system_cpu_vendor(void);
const char* system_cpu_brand(void);

#endif
