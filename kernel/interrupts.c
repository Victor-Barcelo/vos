#include "interrupts.h"
#include "io.h"
#include "panic.h"
#include "syscall.h"
#include "task.h"
#include "screen.h"
#include "usercopy.h"
#include "string.h"

static irq_handler_t irq_handlers[16] = {0};
static uint32_t irq_counts[16] = {0};

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

static const char* const exception_names[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

static bool frame_from_user(const interrupt_frame_t* frame) {
    if (!frame) {
        return false;
    }
    return (frame->cs & 0x3u) == 0x3u;
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

    screen_print("  ");
    screen_print(present ? "P=1" : "P=0");
    screen_print(" ");
    screen_print(write ? "W=1" : "W=0");
    screen_print(" ");
    screen_print(user ? "U=1" : "U=0");
    if (rsvd) screen_print(" RSVD=1");
    if (instr) screen_print(" I=1");
    screen_putchar('\n');
}

static void print_user_backtrace(uint32_t ebp) {
    if (ebp == 0) {
        return;
    }

    screen_println("  backtrace (user EBP chain):");

    for (int depth = 0; depth < 16; depth++) {
        uint32_t pair[2] = {0, 0};
        if (!copy_from_user(pair, (const void*)ebp, (uint32_t)sizeof(pair))) {
            break;
        }

        uint32_t next = pair[0];
        uint32_t ret = pair[1];

        screen_print("    #");
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

interrupt_frame_t* interrupt_handler(interrupt_frame_t* frame) {
    if (!frame) {
        panic("interrupt_handler: NULL frame");
    }

    if (frame->int_no < 32) {
        if (frame_from_user(frame)) {
            screen_set_color(VGA_YELLOW, VGA_BLUE);
            screen_print("\n[USER EXCEPTION] ");
            screen_set_color(VGA_WHITE, VGA_BLUE);
            screen_println(exception_names[frame->int_no]);

            if (frame->int_no == 14) {
                screen_print("  cr2=");
                screen_print_hex(read_cr2());
                screen_putchar('\n');
                print_page_fault_decode(frame->err_code);
            }

            screen_print("  eip=");
            screen_print_hex(frame->eip);
            screen_print(" err=");
            screen_print_hex(frame->err_code);
            screen_putchar('\n');

            print_user_backtrace(frame->ebp);

            screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
            screen_println("  -> killing user task");
            screen_set_color(VGA_WHITE, VGA_BLUE);
            return tasking_exit(frame, -(int32_t)frame->int_no);
        }

        panic_with_frame(exception_names[frame->int_no], frame);
    }

    if (frame->int_no == 0x80) {
        frame = syscall_handle(frame);
        return tasking_deliver_pending_signals(frame);
    }

    if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = (uint8_t)(frame->int_no - 32);
        irq_counts[irq]++;
        irq_handler_t handler = irq_handlers[irq];
        if (handler) {
            handler(frame);
        }
        pic_send_eoi(irq);
        if (irq == 0) {
            frame = tasking_on_timer_tick(frame);
            return tasking_deliver_pending_signals(frame);
        }
        return tasking_deliver_pending_signals(frame);
    }

    return tasking_deliver_pending_signals(frame);
}

void irq_get_counts(uint32_t out[16]) {
    if (out) {
        memcpy(out, irq_counts, sizeof(irq_counts));
    }
}
