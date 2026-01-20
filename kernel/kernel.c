#include "types.h"
#include "serial.h"
#include "screen.h"
#include "keyboard.h"
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

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

extern uint8_t __kernel_end;
extern uint8_t stack_top;

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
        screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
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

    tasking_init();

    // Enable interrupts
    sti();
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Interrupts enabled");

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
