#ifndef KHEAP_H
#define KHEAP_H

#include "types.h"

void kheap_init(void);
void* kmalloc(size_t size);
void* kcalloc(size_t nmemb, size_t size);
void kfree(void* ptr);

#endif

