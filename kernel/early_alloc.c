#include "early_alloc.h"

static uint32_t early_base = 0;
static uint32_t early_ptr = 0;

void early_alloc_init(uint32_t start_addr) {
    early_base = start_addr;
    early_ptr = start_addr;
}

void* early_alloc(size_t size, size_t align) {
    if (align == 0) {
        align = 1;
    }
    uint32_t aligned = (early_ptr + (uint32_t)(align - 1u)) & ~(uint32_t)(align - 1u);
    early_ptr = aligned + (uint32_t)size;
    return (void*)aligned;
}

uint32_t early_alloc_current(void) {
    return early_ptr;
}

uint32_t early_alloc_start(void) {
    return early_base;
}
