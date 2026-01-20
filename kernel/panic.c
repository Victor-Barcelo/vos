#include "panic.h"
#include "io.h"
#include "screen.h"
#include "serial.h"

static void print_line(const char* s) {
    screen_print("  ");
    screen_println(s);
}

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

static void print_page_fault_decode(uint32_t err_code) {
    bool present = (err_code & 0x1u) != 0;
    bool write = (err_code & 0x2u) != 0;
    bool user = (err_code & 0x4u) != 0;
    bool rsvd = (err_code & 0x8u) != 0;
    bool instr = (err_code & 0x10u) != 0;

    screen_println("Page fault details:");
    print_line(present ? "P=1 protection violation" : "P=0 non-present page");
    print_line(write ? "W/R=1 write access" : "W/R=0 read access");
    print_line(user ? "U/S=1 user-mode access" : "U/S=0 supervisor access");
    if (rsvd) {
        print_line("RSVD=1 reserved-bit violation");
    }
    if (instr) {
        print_line("I/D=1 instruction fetch");
    }
}

static void print_backtrace(uint32_t ebp) {
    if (ebp == 0) {
        return;
    }

    screen_println("Backtrace (EBP chain):");

    for (int depth = 0; depth < 16; depth++) {
        if ((ebp & 0x3u) != 0) {
            break;
        }
        if (ebp < 0x1000u) {
            break;
        }

        uint32_t* bp = (uint32_t*)ebp;
        uint32_t next = bp[0];
        uint32_t ret = bp[1];

        screen_print("  #");
        screen_print_dec(depth);
        screen_print(" ");
        screen_print_hex(ret);
        screen_putchar('\n');

        if (next == 0 || next <= ebp) {
            break;
        }
        if (next - ebp > 0x100000u) {
            break;
        }

        ebp = next;
    }
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
            uint32_t cr2 = read_cr2();
            print_kv_hex("cr2", cr2);
            print_page_fault_decode(frame->err_code);
        }

        print_backtrace(frame->ebp);
    }

    screen_println("System halted.");
    serial_write_string("\n=== KERNEL PANIC ===\n");
    serial_write_string("System halted.\n");

    for (;;) {
        hlt();
    }
}
