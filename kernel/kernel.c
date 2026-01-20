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

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

static void keyboard_irq_handler(interrupt_frame_t* frame) {
    (void)frame;
    keyboard_handler();
}

// Kernel main entry point
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    // Initialize serial early for logging/debugging (COM1).
    serial_init();

    // Initialize the screen (VGA text or Multiboot framebuffer).
    screen_init(magic, mboot_info);

    // Display boot message
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("========================================");
    screen_println("          VOS - Minimal Kernel          ");
    screen_println("========================================");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("");

    // Verify multiboot
    if (magic == MULTIBOOT_MAGIC) {
        screen_set_color(VGA_WHITE, VGA_BLUE);
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

    idt_init();
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("IDT initialized");

    timer_init(1000);
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Timer initialized");

    // Route IRQ1 (keyboard) through the common IRQ handler.
    irq_register_handler(1, keyboard_irq_handler);

    // Initialize keyboard (flush controller)
    keyboard_init();
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Keyboard initialized");

    // Enable interrupts
    sti();
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("[OK] ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
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
