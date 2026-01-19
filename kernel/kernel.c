#include "types.h"
#include "screen.h"
#include "keyboard.h"
#include "idt.h"
#include "shell.h"
#include "io.h"

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

// Kernel main entry point
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    (void)mboot_info;  // Reserved for future use

    // Initialize the screen
    screen_init();

    // Display boot message
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("========================================");
    screen_println("          VOS - Minimal Kernel          ");
    screen_println("========================================");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("");

    // Verify multiboot
    if (magic == MULTIBOOT_MAGIC) {
        screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        screen_print("[OK] ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_println("Multiboot verified");
    } else {
        screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
        screen_print("[WARN] ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("Unexpected boot magic: ");
        screen_print_hex(magic);
        screen_println("");
    }

    // Initialize IDT (Interrupt Descriptor Table)
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("IDT initialized");
    idt_init();

    // Initialize keyboard
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("Keyboard initialized");
    keyboard_init();

    // Enable interrupts
    sti();
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("Interrupts enabled");

    screen_println("");
    screen_println("Boot complete! Starting shell...");
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
