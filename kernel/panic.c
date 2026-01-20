#include "panic.h"
#include "io.h"
#include "screen.h"
#include "serial.h"

static void print_kv_hex(const char* key, uint32_t value) {
    screen_print("  ");
    screen_print(key);
    screen_print(": ");
    screen_print_hex(value);
    screen_putchar('\n');
}

__attribute__((noreturn)) void panic(const char* message) {
    cli();
    screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
    screen_println("\n=== KERNEL PANIC ===");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("Reason: ");
    screen_println(message ? message : "(null)");
    screen_println("System halted.");

    // Ensure something hits the serial port even if VGA output is disabled.
    serial_write_string("\n=== KERNEL PANIC ===\n");
    serial_write_string("Reason: ");
    serial_write_string(message ? message : "(null)");
    serial_write_string("\nSystem halted.\n");

    for (;;) {
        hlt();
    }
}

static uint32_t read_cr2(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

__attribute__((noreturn)) void panic_with_frame(const char* message, const interrupt_frame_t* frame) {
    cli();
    screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
    screen_println("\n=== KERNEL PANIC ===");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("Exception: ");
    screen_println(message ? message : "(null)");

    if (frame) {
        print_kv_hex("int_no", frame->int_no);
        print_kv_hex("err_code", frame->err_code);
        print_kv_hex("eip", frame->eip);
        print_kv_hex("cs", frame->cs);
        print_kv_hex("eflags", frame->eflags);

        print_kv_hex("eax", frame->eax);
        print_kv_hex("ebx", frame->ebx);
        print_kv_hex("ecx", frame->ecx);
        print_kv_hex("edx", frame->edx);
        print_kv_hex("esi", frame->esi);
        print_kv_hex("edi", frame->edi);
        print_kv_hex("ebp", frame->ebp);
        print_kv_hex("esp", frame->esp);

        print_kv_hex("ds", frame->ds);
        print_kv_hex("es", frame->es);
        print_kv_hex("fs", frame->fs);
        print_kv_hex("gs", frame->gs);

        if (frame->int_no == 14) {
            print_kv_hex("cr2", read_cr2());
        }
    }

    screen_println("System halted.");
    serial_write_string("\n=== KERNEL PANIC ===\n");
    serial_write_string("System halted.\n");

    for (;;) {
        hlt();
    }
}
