#ifndef SYSCALL_H
#define SYSCALL_H

#include "interrupts.h"

interrupt_frame_t* syscall_handle(interrupt_frame_t* frame);

#endif
