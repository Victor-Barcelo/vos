#include "timer.h"
#include "interrupts.h"
#include "io.h"

#define PIT_BASE_HZ 1193182u
#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43

static volatile uint32_t timer_ticks = 0;
static uint32_t timer_hz = 0;

static void pit_irq_handler(interrupt_frame_t* frame) {
    (void)frame;
    timer_ticks++;
}

void timer_init(uint32_t hz) {
    if (hz == 0) {
        hz = 100;
    }

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 65535) {
        divisor = 65535;
    }

    timer_ticks = 0;
    timer_hz = PIT_BASE_HZ / divisor;

    irq_register_handler(0, pit_irq_handler);

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_hz(void) {
    return timer_hz;
}

uint32_t timer_get_ticks(void) {
    uint32_t flags = irq_save();
    uint32_t ticks = timer_ticks;
    irq_restore(flags);
    return ticks;
}

uint32_t timer_uptime_ms(void) {
    uint32_t hz = timer_get_hz();
    if (hz == 0) {
        return 0;
    }

    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / hz;
    uint32_t rem = ticks % hz;
    uint32_t ms = (rem * 1000u) / hz;
    return seconds * 1000u + ms;
}

void timer_sleep_ms(uint32_t ms) {
    if (ms == 0) {
        return;
    }

    uint32_t hz = timer_get_hz();
    if (hz == 0) {
        return;
    }

    // Calculate ticks with overflow protection
    // Formula: ticks = (ms * hz + 999) / 1000, but check for overflow first
    uint32_t ticks_to_wait;
    if (ms > 0xFFFFFFFFu / hz) {
        // Would overflow 32 bits - cap at max reasonable tick count
        ticks_to_wait = 0x7FFFFFFFu;
    } else {
        uint32_t product = ms * hz;
        // Check if adding 999 would overflow
        if (product > 0xFFFFFFFFu - 999u) {
            ticks_to_wait = (product / 1000u) + 1u;
        } else {
            ticks_to_wait = (product + 999u) / 1000u;
        }
        // Cap at max to avoid wrap issues with target calculation
        if (ticks_to_wait > 0x7FFFFFFFu) {
            ticks_to_wait = 0x7FFFFFFFu;
        }
    }

    uint32_t target = timer_get_ticks() + ticks_to_wait;

    bool were_enabled = irq_are_enabled();
    if (!were_enabled) {
        sti();
    }

    while ((int32_t)(timer_get_ticks() - target) < 0) {
        hlt();
    }

    if (!were_enabled) {
        cli();
    }
}
