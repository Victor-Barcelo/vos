#include "elf.h"
#include "kheap.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "task.h"
#include "usercopy.h"

#define EI_NIDENT 16

#define ELFCLASS32 1
#define ELFDATA2LSB 1

#define ET_EXEC 2

#define EM_386 3

#define PT_LOAD 1

#define PF_X 0x1u
#define PF_W 0x2u
#define PF_R 0x4u

#define USER_BASE 0x02000000u
#define USER_LIMIT 0xC0000000u

// Place the initial user stack high enough to leave plenty of virtual space
// for the user heap (sbrk/malloc) and anonymous mmaps (needed by toolchains
// like tcc). The kernel lives at 0xC0000000, so keep the stack below that.
#define USER_STACK_TOP 0xBFF00000u
#define USER_STACK_PAGES 64u

#define ELF_ARG_MAX VOS_EXEC_MAX_ARGS

typedef struct elf32_ehdr {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

static uint32_t align_down(uint32_t v, uint32_t a) {
    return v & ~(a - 1u);
}

static uint32_t align_up(uint32_t v, uint32_t a) {
    return (v + a - 1u) & ~(a - 1u);
}

static bool elf32_validate_header(const elf32_ehdr_t* eh, uint32_t size) {
    if (!eh || size < sizeof(*eh)) {
        return false;
    }

    if (eh->e_ident[0] != 0x7Fu || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        return false;
    }
    if (eh->e_ident[4] != ELFCLASS32) {
        return false;
    }
    if (eh->e_ident[5] != ELFDATA2LSB) {
        return false;
    }

    if (eh->e_type != ET_EXEC) {
        return false;
    }
    if (eh->e_machine != EM_386) {
        return false;
    }

    if (eh->e_phoff == 0 || eh->e_phnum == 0) {
        return false;
    }
    if (eh->e_phentsize < sizeof(elf32_phdr_t)) {
        return false;
    }

    uint32_t ph_end = eh->e_phoff + (uint32_t)eh->e_phnum * (uint32_t)eh->e_phentsize;
    if (ph_end < eh->e_phoff || ph_end > size) {
        return false;
    }

    return true;
}

static bool map_user_stack(uint32_t* out_user_esp) {
    uint32_t stack_top = USER_STACK_TOP;
    uint32_t guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    uint32_t stack_bottom = guard_bottom + PAGE_SIZE;

    paging_prepare_range(stack_bottom, USER_STACK_PAGES * PAGE_SIZE, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    for (uint32_t va = stack_bottom; va < stack_top; va += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return false;
        }
        paging_map_page(va, frame, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        memset((void*)va, 0, PAGE_SIZE);
    }

    if (out_user_esp) {
        *out_user_esp = stack_top;
    }
    return true;
}

static uint32_t user_stack_bottom(void) {
    return USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE;
}

static bool push_u32(uint32_t* sp, uint32_t value) {
    if (!sp || *sp < 4u) {
        return false;
    }
    *sp -= 4u;
    return copy_to_user((void*)(*sp), &value, 4u);
}

bool elf_setup_user_stack(uint32_t* inout_user_esp, const char* const* argv, uint32_t argc,
                          const char* const* envp, uint32_t envc) {
    if (!inout_user_esp) {
        return false;
    }
    if (argc > 0 && !argv) {
        return false;
    }
    if (envc > 0 && !envp) {
        return false;
    }
    if (argc > ELF_ARG_MAX || envc > ELF_ARG_MAX) {
        return false;
    }

    uint32_t sp = *inout_user_esp;
    if (sp > USER_STACK_TOP) {
        sp = USER_STACK_TOP;
    }
    uint32_t stack_bot = user_stack_bottom();
    if (sp < stack_bot) {
        return false;
    }

    uint32_t* argv_ptrs = NULL;
    uint32_t* envp_ptrs = NULL;

    if (argc != 0) {
        argv_ptrs = (uint32_t*)kmalloc(argc * (uint32_t)sizeof(*argv_ptrs));
        if (!argv_ptrs) {
            return false;
        }
        for (uint32_t i = 0; i < argc; i++) {
            argv_ptrs[i] = 0;
        }
    }

    if (envc != 0) {
        envp_ptrs = (uint32_t*)kmalloc(envc * (uint32_t)sizeof(*envp_ptrs));
        if (!envp_ptrs) {
            if (argv_ptrs) kfree(argv_ptrs);
            return false;
        }
        for (uint32_t i = 0; i < envc; i++) {
            envp_ptrs[i] = 0;
        }
    }

    // Copy environment strings onto the stack (in reverse order).
    for (uint32_t i = 0; i < envc; i++) {
        const char* s = envp[envc - 1u - i];
        if (!s) {
            s = "";
        }
        uint32_t len = (uint32_t)strlen(s) + 1u;
        if (len == 0) {
            len = 1u;
        }
        if (sp < len || sp - len < stack_bot) {
            goto fail;
        }
        sp -= len;
        if (!copy_to_user((void*)sp, s, len)) {
            goto fail;
        }
        envp_ptrs[envc - 1u - i] = sp;
    }

    // Copy argument strings onto the stack (in reverse order).
    for (uint32_t i = 0; i < argc; i++) {
        const char* s = argv[argc - 1u - i];
        if (!s) {
            s = "";
        }
        uint32_t len = (uint32_t)strlen(s) + 1u;
        if (len == 0) {
            len = 1u;
        }
        if (sp < len || sp - len < stack_bot) {
            goto fail;
        }
        sp -= len;
        if (!copy_to_user((void*)sp, s, len)) {
            goto fail;
        }
        argv_ptrs[argc - 1u - i] = sp;
    }

    // Align to 4 bytes.
    sp &= ~3u;
    if (sp < stack_bot) {
        goto fail;
    }

    // Ensure final SP is 16-byte aligned after pushing argc/argv/envp.
    // Layout: argc, argv[0..argc-1], NULL, envp[0..envc-1], NULL
    uint32_t ptr_bytes = (1u + argc + 1u + envc + 1u) * 4u;
    if (sp < ptr_bytes || sp - ptr_bytes < stack_bot) {
        goto fail;
    }
    uint32_t sp_final = sp - ptr_bytes;
    uint32_t sp_aligned = sp_final & ~0xFu;
    uint32_t padding = sp_final - sp_aligned;
    if (padding) {
        if (sp < padding || sp - padding < stack_bot) {
            goto fail;
        }
        sp -= padding;
    }

    // envp terminator
    if (!push_u32(&sp, 0u)) {
        goto fail;
    }
    // envp pointers
    for (uint32_t i = 0; i < envc; i++) {
        uint32_t idx = envc - 1u - i;
        if (!push_u32(&sp, envp_ptrs[idx])) {
            goto fail;
        }
    }
    // argv terminator
    if (!push_u32(&sp, 0u)) {
        goto fail;
    }
    // argv pointers
    for (uint32_t i = 0; i < argc; i++) {
        uint32_t idx = argc - 1u - i;
        if (!push_u32(&sp, argv_ptrs[idx])) {
            goto fail;
        }
    }
    // argc
    if (!push_u32(&sp, argc)) {
        goto fail;
    }

    *inout_user_esp = sp;
    if (argv_ptrs) {
        kfree(argv_ptrs);
    }
    if (envp_ptrs) {
        kfree(envp_ptrs);
    }
    return true;

fail:
    if (argv_ptrs) {
        kfree(argv_ptrs);
    }
    if (envp_ptrs) {
        kfree(envp_ptrs);
    }
    return false;
}

// Helper to free user pages in a range on ELF load failure
static void elf_cleanup_range(uint32_t start, uint32_t end) {
    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        uint32_t paddr = 0;
        if (paging_unmap_page(va, &paddr) && paddr) {
            pmm_free_frame(paddr);
        }
    }
}

bool elf_load_user_image(const uint8_t* image, uint32_t size, uint32_t* out_entry, uint32_t* out_user_esp, uint32_t* out_brk) {
    if (!image || size < sizeof(elf32_ehdr_t)) {
        return false;
    }

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)image;
    if (!elf32_validate_header(eh, size)) {
        serial_write_string("[ELF] invalid header\n");
        return false;
    }

    uint32_t max_end = USER_BASE;

    // Track mapped ranges for cleanup on failure
    uint32_t mapped_min = 0xFFFFFFFF;
    uint32_t mapped_max = 0;

    // Load PT_LOAD segments.
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        uint32_t off = eh->e_phoff + (uint32_t)i * (uint32_t)eh->e_phentsize;
        const elf32_phdr_t* ph = (const elf32_phdr_t*)(image + off);

        if (ph->p_type != PT_LOAD) {
            continue;
        }
        if (ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz) {
            serial_write_string("[ELF] segment filesz > memsz\n");
            goto fail_cleanup;
        }
        if (ph->p_offset + ph->p_filesz < ph->p_offset || ph->p_offset + ph->p_filesz > size) {
            serial_write_string("[ELF] segment out of bounds\n");
            goto fail_cleanup;
        }

        uint32_t seg_start = ph->p_vaddr;
        uint32_t seg_end = ph->p_vaddr + ph->p_memsz;
        if (seg_end < seg_start) {
            serial_write_string("[ELF] segment overflow\n");
            goto fail_cleanup;
        }
        if (seg_start < USER_BASE || seg_end > USER_LIMIT) {
            serial_write_string("[ELF] segment not in user range\n");
            goto fail_cleanup;
        }
        if (seg_end > max_end) {
            max_end = seg_end;
        }

        uint32_t map_flags = PAGE_PRESENT | PAGE_USER;
        if (ph->p_flags & PF_W) {
            map_flags |= PAGE_RW;
        }

        uint32_t map_start = align_down(seg_start, PAGE_SIZE);
        uint32_t map_end = align_up(seg_end, PAGE_SIZE);

        paging_prepare_range(map_start, map_end - map_start, map_flags);

        for (uint32_t va = map_start; va < map_end; va += PAGE_SIZE) {
            uint32_t frame = pmm_alloc_frame();
            if (frame == 0) {
                serial_write_string("[ELF] out of frames\n");
                // Free pages allocated in this segment so far
                elf_cleanup_range(map_start, va);
                goto fail_cleanup;
            }
            paging_map_page(va, frame, map_flags);
            memset((void*)va, 0, PAGE_SIZE);
        }

        // Track this segment's range for cleanup
        if (map_start < mapped_min) mapped_min = map_start;
        if (map_end > mapped_max) mapped_max = map_end;

        // Copy initialized data.
        memcpy((void*)seg_start, image + ph->p_offset, ph->p_filesz);

        // Zero BSS / remaining.
        uint32_t bss_start = seg_start + ph->p_filesz;
        uint32_t bss_len = ph->p_memsz - ph->p_filesz;
        if (bss_len) {
            memset((void*)bss_start, 0, bss_len);
        }
    }

    uint32_t user_esp = 0;
    if (!map_user_stack(&user_esp)) {
        serial_write_string("[ELF] failed to map user stack\n");
        goto fail_cleanup;
    }

    uint32_t brk = align_up(max_end, PAGE_SIZE);
    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    if (brk < USER_BASE || brk > stack_guard_bottom) {
        serial_write_string("[ELF] brk collides with stack\n");
        // Note: stack pages also need cleanup but map_user_stack handles its own cleanup on failure
        goto fail_cleanup;
    }

    if (out_entry) {
        *out_entry = eh->e_entry;
    }
    if (out_user_esp) {
        *out_user_esp = user_esp;
    }
    if (out_brk) {
        *out_brk = brk;
    }

    serial_write_string("[ELF] loaded entry=");
    serial_write_hex(eh->e_entry);
    serial_write_string(" user_esp=");
    serial_write_hex(user_esp);
    serial_write_string(" brk=");
    serial_write_hex(brk);
    serial_write_char('\n');

    return true;

fail_cleanup:
    // Free all pages that were mapped during this load attempt
    if (mapped_max > mapped_min) {
        elf_cleanup_range(mapped_min, mapped_max);
    }
    return false;
}
