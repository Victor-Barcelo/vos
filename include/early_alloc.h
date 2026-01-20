#ifndef EARLY_ALLOC_H
#define EARLY_ALLOC_H

#include "types.h"

void early_alloc_init(uint32_t start_addr);
void* early_alloc(size_t size, size_t align);
uint32_t early_alloc_start(void);
uint32_t early_alloc_current(void);

#endif
