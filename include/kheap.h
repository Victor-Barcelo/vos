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

#endif

