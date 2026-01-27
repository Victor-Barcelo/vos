#include "kheap.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"
#include "string.h"

#define HEAP_BASE 0xD0000000u
#define HEAP_INITIAL_SIZE (64u * 1024u)

typedef struct block_header {
    uint32_t size;                 // total block size (header + payload + footer)
    uint32_t used;                 // 1 = allocated, 0 = free
    struct block_header* next_free;
    struct block_header* prev_free;
} block_header_t;

static uint32_t heap_base = 0;
static uint32_t heap_end = 0;
static uint32_t heap_mapped_end = 0;
static block_header_t* free_list = NULL;

// Debug counters for heap allocation tracking
static uint32_t heap_alloc_count = 0;   // successful allocations
static uint32_t heap_free_count = 0;    // successful frees
static uint32_t heap_fail_count = 0;    // allocation failures

static uint32_t align_up(uint32_t v, uint32_t a) {
    return (v + a - 1u) & ~(a - 1u);
}

static uint32_t block_overhead(void) {
    return (uint32_t)sizeof(block_header_t) + (uint32_t)sizeof(uint32_t);
}

static uint32_t block_min_size(void) {
    return align_up(block_overhead() + 16u, 16u);
}

static void write_footer(block_header_t* b) {
    uint32_t* footer = (uint32_t*)((uint8_t*)b + b->size - sizeof(uint32_t));
    *footer = b->size;
}

static block_header_t* next_block(block_header_t* b) {
    return (block_header_t*)((uint8_t*)b + b->size);
}

static block_header_t* prev_block(block_header_t* b) {
    if (!b) {
        return NULL;
    }
    uint32_t addr = (uint32_t)b;
    if (addr <= heap_base + sizeof(uint32_t)) {
        return NULL;
    }
    uint32_t prev_size = *(uint32_t*)((uint8_t*)b - sizeof(uint32_t));
    if (prev_size < block_min_size() || (prev_size & 0xFu) != 0) {
        return NULL;
    }
    if (addr < heap_base + prev_size) {
        return NULL;
    }
    uint32_t prev_addr = addr - prev_size;
    if (prev_addr < heap_base) {
        return NULL;
    }
    return (block_header_t*)prev_addr;
}

static void free_list_remove(block_header_t* b) {
    if (!b) {
        return;
    }
    if (b->prev_free) {
        b->prev_free->next_free = b->next_free;
    } else {
        if (free_list == b) {
            free_list = b->next_free;
        }
    }
    if (b->next_free) {
        b->next_free->prev_free = b->prev_free;
    }
    b->next_free = NULL;
    b->prev_free = NULL;
}

static void free_list_insert(block_header_t* b) {
    if (!b) {
        return;
    }
    b->next_free = free_list;
    b->prev_free = NULL;
    if (free_list) {
        free_list->prev_free = b;
    }
    free_list = b;
}

static void map_more(uint32_t new_end) {
    uint32_t target = align_up(new_end, PAGE_SIZE);

    // Allocate any required page tables first so the physical frames backing those
    // tables are not accidentally allocated for heap pages.
    paging_prepare_range(heap_mapped_end, target - heap_mapped_end, PAGE_PRESENT | PAGE_RW);

    while (heap_mapped_end < target) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return;
        }
        paging_map_page(heap_mapped_end, frame, PAGE_PRESENT | PAGE_RW);
        heap_mapped_end += PAGE_SIZE;
    }
}

static block_header_t* coalesce(block_header_t* b) {
    if (!b) {
        return NULL;
    }

    // Merge with next.
    block_header_t* n = next_block(b);
    if ((uint32_t)n < heap_end && n->used == 0 && n->size >= block_min_size()) {
        free_list_remove(n);
        b->size += n->size;
        write_footer(b);
    }

    // Merge with previous.
    block_header_t* p = prev_block(b);
    if (p && (uint32_t)p >= heap_base && (uint32_t)p < heap_end && p->used == 0 && p->size >= block_min_size()) {
        free_list_remove(p);
        p->size += b->size;
        write_footer(p);
        b = p;
    }

    return b;
}

static bool heap_grow(uint32_t min_extra) {
    if (min_extra == 0) {
        min_extra = PAGE_SIZE;
    }

    uint32_t old_end = heap_end;
    uint32_t new_end = heap_end + min_extra;
    if (new_end < old_end) {
        return false;
    }
    new_end = align_up(new_end, PAGE_SIZE);

    map_more(new_end);
    if (heap_mapped_end < new_end) {
        return false;
    }

    heap_end = new_end;

    uint32_t block_size = new_end - old_end;
    if (block_size < block_min_size()) {
        return true;
    }

    block_header_t* b = (block_header_t*)old_end;
    b->size = block_size;
    b->used = 0;
    b->next_free = NULL;
    b->prev_free = NULL;
    write_footer(b);

    b = coalesce(b);
    free_list_insert(b);
    return true;
}

void kheap_init(void) {
    heap_base = HEAP_BASE;
    heap_end = heap_base;
    heap_mapped_end = heap_base;
    free_list = NULL;

    // Map an initial heap region and expose it as a single free block.
    if (!heap_grow(HEAP_INITIAL_SIZE)) {
        panic("kheap: initial grow failed");
    }

    // Minimal allocator sanity check (coalescing must work).
    void* a = kmalloc(20000u);
    void* b = kmalloc(20000u);
    void* c = kmalloc(20000u);
    if (!a || !b || !c) {
        panic("kheap: self-test alloc failed");
    }

    kfree(b);
    kfree(a);

    void* d = kmalloc(35000u);
    if (!d) {
        panic("kheap: self-test coalesce failed");
    }

    kfree(c);
    kfree(d);
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint32_t want = align_up((uint32_t)size, 16u);
    uint32_t total = want + block_overhead();
    total = align_up(total, 16u);
    if (total < block_min_size()) {
        total = block_min_size();
    }

    for (;;) {
        block_header_t* b = free_list;
        while (b) {
            if (b->used == 0 && b->size >= total) {
                free_list_remove(b);

                uint32_t remaining = b->size - total;
                if (remaining >= block_min_size()) {
                    b->size = total;
                    write_footer(b);

                    block_header_t* split = (block_header_t*)((uint8_t*)b + total);
                    split->size = remaining;
                    split->used = 0;
                    split->next_free = NULL;
                    split->prev_free = NULL;
                    write_footer(split);
                    free_list_insert(split);
                }

                b->used = 1;
                write_footer(b);
                heap_alloc_count++;
                return (uint8_t*)b + sizeof(block_header_t);
            }
            b = b->next_free;
        }

        // No free block large enough; grow the heap and retry.
        if (!heap_grow(total)) {
            heap_fail_count++;
            return NULL;
        }
    }
}

void* kcalloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    // Check for overflow BEFORE multiplication using 64-bit arithmetic
    uint64_t total64 = (uint64_t)nmemb * (uint64_t)size;
    if (total64 > 0xFFFFFFFFu) {
        return NULL;  // Overflow: result doesn't fit in 32 bits
    }
    uint32_t total = (uint32_t)total64;
    void* p = kmalloc(total);
    if (!p) {
        return NULL;
    }
    memset(p, 0, total);
    return p;
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }

    uint32_t addr = (uint32_t)ptr;
    if (addr < heap_base + sizeof(block_header_t) || addr >= heap_end) {
        return;
    }

    block_header_t* b = (block_header_t*)(addr - sizeof(block_header_t));
    if (b->used == 0 || b->size < block_min_size() || (b->size & 0xFu) != 0) {
        return;
    }

    b->used = 0;
    write_footer(b);
    b = coalesce(b);
    free_list_insert(b);
    heap_free_count++;
}

void kheap_get_info(uint32_t* out_base, uint32_t* out_end,
                    uint32_t* out_free_bytes, uint32_t* out_free_blocks) {
    if (out_base) {
        *out_base = heap_base;
    }
    if (out_end) {
        *out_end = heap_end;
    }

    uint32_t free_bytes = 0;
    uint32_t free_blocks = 0;
    block_header_t* b = free_list;
    while (b) {
        free_blocks++;
        free_bytes += b->size;
        b = b->next_free;
    }

    if (out_free_bytes) {
        *out_free_bytes = free_bytes;
    }
    if (out_free_blocks) {
        *out_free_blocks = free_blocks;
    }
}

uint32_t kheap_alloc_count(void) {
    return heap_alloc_count;
}

uint32_t kheap_free_count(void) {
    return heap_free_count;
}

uint32_t kheap_fail_count(void) {
    return heap_fail_count;
}
