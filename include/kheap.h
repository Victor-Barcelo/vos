#ifndef KHEAP_H
#define KHEAP_H

#include "types.h"

void kheap_init(void);
void* kmalloc(size_t size);
void* kcalloc(size_t nmemb, size_t size);
void kfree(void* ptr);

// Introspection for sysview
void kheap_get_info(uint32_t* out_base, uint32_t* out_end,
                    uint32_t* out_free_bytes, uint32_t* out_free_blocks);

// Debug counters for heap allocation tracking
uint32_t kheap_alloc_count(void);
uint32_t kheap_free_count(void);
uint32_t kheap_fail_count(void);

#endif

