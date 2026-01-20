#include "interrupts.h"
#include "io.h"
#include "panic.h"
#include "syscall.h"
#include "task.h"

static irq_handler_t irq_handlers[16] = {0};

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

interrupt_frame_t* interrupt_handler(interrupt_frame_t* frame) {
    if (!frame) {
        panic("interrupt_handler: NULL frame");
    }

    if (frame->int_no < 32) {
        panic_with_frame(exception_names[frame->int_no], frame);
    }

    if (frame->int_no == 0x80) {
        return syscall_handle(frame);
    }

    if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = (uint8_t)(frame->int_no - 32);
        irq_handler_t handler = irq_handlers[irq];
        if (handler) {
            handler(frame);
        }
        pic_send_eoi(irq);
        if (irq == 0) {
            return tasking_on_timer_tick(frame);
        }
        return frame;
    }

    return frame;
}
