# Chapter 9: Memory Management

Memory management in kernels is layered. VOS builds up capabilities in stages:

1. **Early Allocator** - Bootstrap allocations before real allocator exists
2. **Paging** - Virtual memory translation
3. **Physical Memory Manager (PMM)** - Track and allocate physical frames
4. **Kernel Heap** - Dynamic allocations for kernel use

## Memory Layout Overview

VOS uses several important address ranges:

```
Virtual Address Space (4 GB)
+-----------------------------------+ 0xFFFFFFFF
|         (Unmapped)                |
+-----------------------------------+ 0xF0000000
|         Kernel Stacks             |
+-----------------------------------+ 0xE0000000
|                                   |
|         Kernel Heap               |
|         (grows upward)            |
+-----------------------------------+ 0xD0000000
|         (Reserved/Unmapped)       |
+-----------------------------------+ 0xC0000000
|                                   |
|         User Space                |
|         (process memory)          |
|                                   |
+-----------------------------------+ 0x08048000
|         (Reserved for NULL)       |
+-----------------------------------+ 0x00100000
|         Kernel Code/Data          |
|         (identity mapped)         |
+-----------------------------------+ 0x00000000
```

### Key Addresses

| Address | Purpose |
|---------|---------|
| 0x00100000 (1 MB) | Kernel load address |
| 0x08048000 | User program start |
| 0x02000000 | User stack region |
| 0xC0000000 | Kernel/user boundary |
| 0xD0000000 | Kernel heap start |

## Early Allocator

The early allocator is a simple bump-pointer allocator used during boot, before PMM and heap are ready.

### The Bootstrap Problem

When the kernel starts:
- We need memory for page tables
- We need memory for PMM bitmap
- But we don't have an allocator yet

Solution: A simple allocator that only moves forward.

### Computing the Safe Start

```c
uint32_t compute_early_start(uint32_t kernel_end, multiboot_info_t *mboot) {
    uint32_t early_start = kernel_end;

    // Don't overwrite Multiboot info
    if (mboot) {
        early_start = max(early_start, (uint32_t)mboot + sizeof(*mboot));
    }

    // Don't overwrite memory map
    if (mboot && (mboot->flags & (1 << 6))) {
        early_start = max(early_start,
                         mboot->mmap_addr + mboot->mmap_length);
    }

    // Don't overwrite modules (initramfs)
    if (mboot && (mboot->flags & (1 << 3))) {
        multiboot_module_t *mods = (multiboot_module_t *)mboot->mods_addr;
        for (uint32_t i = 0; i < mboot->mods_count; i++) {
            early_start = max(early_start, mods[i].mod_end);
        }
    }

    // Align to page boundary
    return (early_start + 0xFFF) & ~0xFFF;
}
```

### Early Allocator Implementation

```c
static uint32_t early_ptr;

void early_alloc_init(uint32_t start) {
    early_ptr = start;
}

void* early_alloc(size_t size, size_t align) {
    // Align pointer
    if (align > 1) {
        early_ptr = (early_ptr + align - 1) & ~(align - 1);
    }

    void *result = (void *)early_ptr;
    early_ptr += size;

    // Zero the memory
    memset(result, 0, size);

    return result;
}
```

### Characteristics

- Never frees memory
- Simple and predictable
- Used only during boot
- Allocates page tables, PMM bitmap, early structures

## Paging

Paging provides virtual memory by translating virtual addresses to physical addresses through page tables.

### x86 32-bit Paging Structure

```
Virtual Address (32 bits)
+----------+----------+----------+
| PDE (10) | PTE (10) | Offset(12)|
+----------+----------+----------+
     |          |          |
     v          v          v
  Page Dir   Page Tab   4KB Page
  (1024)     (1024)
```

- **Page Directory**: 1024 entries, each points to a page table
- **Page Table**: 1024 entries, each points to a 4KB physical frame
- **Offset**: 12 bits = 4096 bytes per page

### Page Entry Flags

```c
#define PAGE_PRESENT    0x001   // Page is present in memory
#define PAGE_RW         0x002   // Page is writable
#define PAGE_USER       0x004   // Page is accessible from ring 3
#define PAGE_PWT        0x008   // Page write-through
#define PAGE_PCD        0x010   // Page cache disable
#define PAGE_ACCESSED   0x020   // CPU has accessed this page
#define PAGE_DIRTY      0x040   // Page has been written to
#define PAGE_SIZE       0x080   // 4MB page (in PDE)
#define PAGE_GLOBAL     0x100   // Don't flush from TLB
```

### Enabling Paging

```c
void paging_init(multiboot_info_t *mboot) {
    // Allocate page directory
    page_directory = early_alloc(4096, 4096);

    // Identity map first 16 MB (kernel + boot data)
    for (uint32_t addr = 0; addr < 0x01000000; addr += 0x1000) {
        paging_map_page(addr, addr, PAGE_PRESENT | PAGE_RW);
    }

    // Map framebuffer if present
    if (mboot->flags & (1 << 12)) {
        uint32_t fb_addr = mboot->framebuffer_addr;
        uint32_t fb_size = mboot->framebuffer_pitch *
                          mboot->framebuffer_height;
        for (uint32_t off = 0; off < fb_size; off += 0x1000) {
            paging_map_page(fb_addr + off, fb_addr + off,
                          PAGE_PRESENT | PAGE_RW);
        }
    }

    // Load CR3 and enable paging
    __asm__ volatile(
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0"
        : : "r"((uint32_t)page_directory)
        : "eax"
    );
}
```

### Mapping a Page

```c
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;

    // Get or create page table
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        uint32_t *new_table = early_alloc(4096, 4096);
        page_directory[pd_index] = (uint32_t)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    uint32_t *page_table = (uint32_t *)(page_directory[pd_index] & ~0xFFF);
    page_table[pt_index] = (phys & ~0xFFF) | flags;

    // Invalidate TLB entry
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
```

## Physical Memory Manager (PMM)

The PMM tracks which physical frames are free or used using a bitmap.

### Bitmap Structure

```c
// 1 bit per 4KB frame
// 4GB / 4KB = 1M frames = 128KB bitmap
static uint8_t *pmm_bitmap;
static uint32_t pmm_total_frames;
static uint32_t pmm_used_frames;
```

### PMM Initialization

```c
void pmm_init(uint32_t magic, multiboot_info_t *mboot, uint32_t kernel_end) {
    // Calculate total memory
    uint32_t total_memory = 0;
    if (mboot->flags & (1 << 6)) {
        // Parse memory map
        mmap_entry_t *mmap = (mmap_entry_t *)mboot->mmap_addr;
        while ((uint32_t)mmap < mboot->mmap_addr + mboot->mmap_length) {
            if (mmap->type == 1) {  // Available
                total_memory = max(total_memory,
                                  mmap->base_addr + mmap->length);
            }
            mmap = (mmap_entry_t *)((uint32_t)mmap + mmap->size + 4);
        }
    }

    pmm_total_frames = total_memory / 0x1000;
    uint32_t bitmap_size = (pmm_total_frames + 7) / 8;

    // Allocate bitmap
    pmm_bitmap = early_alloc(bitmap_size, 16);

    // Mark all frames as used initially
    memset(pmm_bitmap, 0xFF, bitmap_size);
    pmm_used_frames = pmm_total_frames;

    // Mark available regions as free
    parse_memory_map_free(mboot);

    // Reserve critical regions
    pmm_reserve_region(0, 0x100000);               // Low memory
    pmm_reserve_region(0x100000, kernel_end - 0x100000);  // Kernel
    pmm_reserve_multiboot(mboot);                   // Multiboot data
    pmm_reserve_early_alloc();                      // Early allocations
}
```

### Allocating a Frame

```c
uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < pmm_total_frames; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;

        if (!(pmm_bitmap[byte] & (1 << bit))) {
            // Frame is free
            pmm_bitmap[byte] |= (1 << bit);
            pmm_used_frames++;
            return i * 0x1000;
        }
    }

    panic("PMM: Out of memory!");
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / 0x1000;
    uint32_t byte = frame / 8;
    uint32_t bit = frame % 8;

    pmm_bitmap[byte] &= ~(1 << bit);
    pmm_used_frames--;
}
```

## Kernel Heap

The kernel heap provides `kmalloc`/`kfree` for dynamic allocations.

### Heap Structure

```c
#define KHEAP_START     0xD0000000
#define KHEAP_MAX       0xE0000000

static uint32_t kheap_current = KHEAP_START;
```

### Simple Bump Allocator

```c
void* kmalloc(size_t size) {
    // Align to 16 bytes
    size = (size + 15) & ~15;

    // Check if we need more pages
    uint32_t new_end = kheap_current + size;
    while (new_end > kheap_mapped_end) {
        // Map a new page
        uint32_t phys = pmm_alloc_frame();
        paging_map_page(kheap_mapped_end, phys, PAGE_PRESENT | PAGE_RW);
        kheap_mapped_end += 0x1000;
    }

    void *result = (void *)kheap_current;
    kheap_current = new_end;

    return result;
}

void* kcalloc(size_t count, size_t size) {
    void *ptr = kmalloc(count * size);
    memset(ptr, 0, count * size);
    return ptr;
}
```

### Free List Allocator (Advanced)

A more sophisticated allocator tracks freed blocks:

```c
typedef struct free_block {
    size_t size;
    struct free_block *next;
} free_block_t;

static free_block_t *free_list = NULL;

void kfree(void *ptr) {
    if (!ptr) return;

    // Add to free list
    free_block_t *block = (free_block_t *)ptr;
    block->next = free_list;
    free_list = block;
}

void* kmalloc(size_t size) {
    // First, check free list
    free_block_t **prev = &free_list;
    free_block_t *block = free_list;

    while (block) {
        if (block->size >= size) {
            *prev = block->next;
            return (void *)block;
        }
        prev = &block->next;
        block = block->next;
    }

    // No suitable block, allocate new
    return kmalloc_new(size);
}
```

## User Memory Management

VOS tracks user memory with Virtual Memory Areas (VMAs):

```c
typedef struct vm_area {
    uint32_t start;
    uint32_t end;
    uint32_t flags;
    struct vm_area *next;
} vm_area_t;
```

### sbrk Syscall

```c
int32_t sys_sbrk(int32_t increment) {
    task_t *task = current_task;
    uint32_t old_brk = task->brk;

    if (increment > 0) {
        // Extend heap
        uint32_t new_brk = old_brk + increment;

        // Map new pages
        for (uint32_t addr = old_brk & ~0xFFF; addr < new_brk; addr += 0x1000) {
            if (!paging_is_mapped(addr)) {
                uint32_t phys = pmm_alloc_frame();
                paging_map_page(addr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            }
        }

        task->brk = new_brk;
    }

    return old_brk;
}
```

## Summary

VOS memory management provides:

1. **Early allocator** for bootstrap allocations
2. **Paging** for virtual memory
3. **PMM** for physical frame management
4. **Kernel heap** for dynamic allocations
5. **User memory** via sbrk and mmap

This layered approach allows incremental initialization and clean separation of concerns.

---

*Previous: [Chapter 8: Programmable Interrupt Controller](08_pic.md)*
*Next: [Chapter 10: Timekeeping](10_timekeeping.md)*
