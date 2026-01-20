#ifndef PANIC_H
#define PANIC_H

#include "interrupts.h"

__attribute__((noreturn)) void panic(const char* message);
__attribute__((noreturn)) void panic_with_frame(const char* message, const interrupt_frame_t* frame);

#endif
