#include "types.h"
#include "serial.h"
#include "screen.h"
#include "keyboard.h"
#include "mouse.h"
#include "idt.h"
#include "shell.h"
#include "io.h"
#include "interrupts.h"
#include "timer.h"
#include "system.h"
#include "early_alloc.h"
#include "pmm.h"
#include "multiboot.h"
#include "paging.h"
#include "kheap.h"
#include "vfs.h"
#include "gdt.h"
#include "task.h"
#include "elf.h"
#include "fatdisk.h"
#include "string.h"
#include "statusbar.h"
#include "speaker.h"
#include "dma.h"
#include "sb16.h"

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

extern uint8_t __kernel_end;
extern uint8_t stack_top;
extern void stack_switch_and_call(uint32_t new_stack_top, void (*fn)(uint32_t, uint32_t*), uint32_t magic, uint32_t* mboot_info);

static uint32_t align_up_u32(uint32_t v, uint32_t a) {
    return (v + a - 1u) & ~(a - 1u);
}

static uint32_t compute_early_start(uint32_t kernel_end, const multiboot_info_t* mbi) {
    uint32_t high = kernel_end;
    if (!mbi) {
        return align_up_u32(high, 0x1000u);
    }

    uint32_t mbi_end = (uint32_t)mbi + (uint32_t)sizeof(*mbi);
    if (mbi_end > high) {
        high = mbi_end;
    }

    if ((mbi->flags & MULTIBOOT_INFO_MMAP) && mbi->mmap_addr && mbi->mmap_length) {
        uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
        if (mmap_end > high) {
            high = mmap_end;
        }
    }

    if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_addr && mbi->mods_count) {
        uint32_t mods_end = mbi->mods_addr + mbi->mods_count * (uint32_t)sizeof(multiboot_module_t);
        if (mods_end > high) {
            high = mods_end;
        }
        const multiboot_module_t* mods = (const multiboot_module_t*)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            if (mods[i].mod_end > high) {
                high = mods[i].mod_end;
            }
        }
    }

    return align_up_u32(high, 0x1000u);
}

static void keyboard_irq_handler(interrupt_frame_t* frame) {
    (void)frame;
    keyboard_handler();
}

static void kernel_idle_hook(void) {
    statusbar_tick();

    static bool cursor_on = true;
    static uint32_t next_toggle_tick = 0;

    uint32_t hz = timer_get_hz();
    if (hz == 0) {
        return;
    }

    uint32_t now = timer_get_ticks();
    if ((int32_t)(now - next_toggle_tick) < 0) {
        return;
    }

    cursor_on = !cursor_on;
    screen_cursor_set_enabled(cursor_on);

    uint32_t interval = hz / 2u;
    if (interval == 0) {
        interval = 1;
    }
    next_toggle_tick = now + interval;
}

static void try_start_init(void) {
    if (!vfs_is_ready()) {
        return;
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file("/bin/init", &data, &size) || !data || size == 0) {
        return;
    }

    uint32_t entry = 0;
    uint32_t user_esp = 0;
    uint32_t brk = 0;
    uint32_t* user_dir = paging_create_user_directory();
    if (!user_dir) {
        return;
    }

    uint32_t flags = irq_save();
    paging_switch_directory(user_dir);
    bool ok = elf_load_user_image(data, size, &entry, &user_esp, &brk);
    if (ok) {
        const char* const init_argv[] = {"/bin/init"};
        ok = elf_setup_user_stack(&user_esp, init_argv, 1, NULL, 0);
    }
    paging_switch_directory(paging_kernel_directory());
    irq_restore(flags);
    if (!ok) {
        return;
    }

    uint32_t pid = tasking_spawn_user_pid(entry, user_esp, user_dir, brk);
    if (pid == 0) {
        return;
    }

    serial_write_string("[INIT] spawned /bin/init\n");

    // Run init in the foreground to avoid interleaved console output.
    int exit_code = 0;
    __asm__ volatile ("int $0x80" : "=a"(exit_code) : "a"(4u), "b"(pid) : "memory");
}

static uint32_t alloc_guarded_stack(uint32_t base_vaddr, uint32_t size_bytes) {
    if (size_bytes == 0) {
        return 0;
    }
    if ((size_bytes & (PAGE_SIZE - 1u)) != 0) {
        size_bytes = (size_bytes + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
    }

    uint32_t stack_bottom = base_vaddr + PAGE_SIZE; // guard page below
    uint32_t stack_top_addr = stack_bottom + size_bytes;
    if (stack_top_addr < stack_bottom) {
        return 0;
    }

    paging_prepare_range(stack_bottom, size_bytes, PAGE_PRESENT | PAGE_RW);
    for (uint32_t va = stack_bottom; va < stack_top_addr; va += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return 0;
        }
        paging_map_page(va, frame, PAGE_PRESENT | PAGE_RW);
        memset((void*)va, 0, PAGE_SIZE);
    }

    return stack_top_addr;
}

static void kernel_main_continued(uint32_t magic, uint32_t* mboot_info) {
    (void)magic;
    (void)mboot_info;

    // Enable interrupts
    sti();
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Interrupts enabled");

    // UI helpers used across kernel + userland (status bar + blinking cursor).
    statusbar_init();
    keyboard_set_idle_hook(kernel_idle_hook);

    try_start_init();

    screen_println("");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Boot complete! Starting shell...");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("");

    // Run the shell
    shell_run();

    // If shell exits, halt
    screen_println("Shell exited. Halting...");
    cli();
    for (;;) {
        hlt();
    }
}

// Kernel main entry point
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    // Initialize serial early for logging/debugging (COM1).
    serial_init();

    // Initialize the screen (VGA text or Multiboot framebuffer).
    screen_init(magic, mboot_info);

    uint32_t kernel_end = (uint32_t)&__kernel_end;
    uint32_t early_start = compute_early_start(kernel_end, (const multiboot_info_t*)mboot_info);
    early_alloc_init(early_start);
    paging_init((const multiboot_info_t*)mboot_info);
    pmm_init(magic, (const multiboot_info_t*)mboot_info, kernel_end);
    kheap_init();
    vfs_init((const multiboot_info_t*)mboot_info);
    (void)fatdisk_init();

    // Display boot message
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("========================================");
    screen_println("          VOS - Minimal Kernel          ");
    screen_println("========================================");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("");

    // Verify multiboot
    if (magic == MULTIBOOT_MAGIC) {
        screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
        screen_print("[OK] ");
        screen_set_color(VGA_WHITE, VGA_BLUE);
        screen_println("Multiboot verified");
    } else {
        screen_set_color(VGA_YELLOW, VGA_BLUE);
        screen_print("[WARN] ");
        screen_set_color(VGA_WHITE, VGA_BLUE);
        screen_print("Unexpected boot magic: ");
        screen_print_hex(magic);
        screen_println("");
    }

    system_init(magic, mboot_info);

    gdt_init();
    tss_set_kernel_stack((uint32_t)&stack_top);

    idt_init();
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("IDT initialized");

    timer_init(1000);
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Timer initialized");

    // Route IRQ1 (keyboard) through the common IRQ handler.
    irq_register_handler(1, keyboard_irq_handler);

    // Initialize keyboard (flush controller)
    keyboard_init();
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Keyboard initialized");

    mouse_init();

    speaker_init();

    dma_init();
    sb16_init();

    tasking_init();

    // Switch to a guarded kernel stack for the long-running boot task (shell, etc.).
    uint32_t new_stack = alloc_guarded_stack(0xEF000000u, 64u * 1024u);
    if (new_stack) {
        tss_set_kernel_stack(new_stack);
        stack_switch_and_call(new_stack, kernel_main_continued, magic, mboot_info);
    }

    // Fallback: continue on the static boot stack if allocation fails.
    kernel_main_continued(magic, mboot_info);
}
