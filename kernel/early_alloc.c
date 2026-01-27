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
    // Check for overflow on alignment
    if (aligned < early_ptr) {
        return NULL;
    }
    uint32_t new_ptr = aligned + (uint32_t)size;
    // Check for overflow on size addition
    if (new_ptr < aligned) {
        return NULL;
    }
    // Check we don't exceed kernel space
    if (new_ptr > 0xC0000000u) {
        return NULL;
    }
    early_ptr = new_ptr;
    return (void*)aligned;
}

uint32_t early_alloc_current(void) {
    return early_ptr;
}

uint32_t early_alloc_start(void) {
    return early_base;
}
