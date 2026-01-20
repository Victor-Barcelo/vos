#ifndef TASK_H
#define TASK_H

#include "interrupts.h"

void tasking_init(void);
bool tasking_is_enabled(void);

// Called from the IRQ0 (timer) path to perform a timeslice switch.
interrupt_frame_t* tasking_on_timer_tick(interrupt_frame_t* frame);

// Voluntary context switches (used by syscalls).
interrupt_frame_t* tasking_yield(interrupt_frame_t* frame);
interrupt_frame_t* tasking_exit(interrupt_frame_t* frame);

// Create a user-mode task that starts at `entry` with user stack pointer `user_esp`.
bool tasking_spawn_user(uint32_t entry, uint32_t user_esp);

#endif
