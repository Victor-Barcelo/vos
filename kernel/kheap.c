#include "kheap.h"
#include "paging.h"
#include "pmm.h"

static uint32_t heap_base = 0;
static uint32_t heap_brk = 0;
static uint32_t heap_mapped_end = 0;

static uint32_t align_up(uint32_t v, uint32_t a) {
    return (v + a - 1u) & ~(a - 1u);
}

static void map_more(uint32_t new_end) {
    uint32_t target = align_up(new_end, PAGE_SIZE);
    while (heap_mapped_end < target) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return;
        }
        paging_map_page(heap_mapped_end, frame, PAGE_PRESENT | PAGE_RW);
        heap_mapped_end += PAGE_SIZE;
    }
}

void kheap_init(void) {
    heap_base = 0xD0000000u;
    heap_brk = heap_base;
    heap_mapped_end = heap_base;

    // Map an initial 64 KiB for the heap.
    map_more(heap_base + 64u * 1024u);
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint32_t aligned = align_up((uint32_t)size, 16u);
    uint32_t start = heap_brk;
    uint32_t end = heap_brk + aligned;
    if (end < start) {
        return NULL;
    }

    if (end > heap_mapped_end) {
        map_more(end);
        if (end > heap_mapped_end) {
            return NULL;
        }
    }

    heap_brk = end;
    return (void*)start;
}

void* kcalloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    uint32_t total = (uint32_t)nmemb * (uint32_t)size;
    if (nmemb != 0 && (total / nmemb) != (uint32_t)size) {
        return NULL;
    }
    void* p = kmalloc(total);
    if (!p) {
        return NULL;
    }
    // Very small memset to avoid pulling in libc: inline loop.
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < total; i++) {
        b[i] = 0;
    }
    return p;
}

void kfree(void* ptr) {
    (void)ptr;
    // TODO: real free list / coalescing allocator.
}

