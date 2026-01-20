#include "syscall.h"
#include "screen.h"
#include "task.h"

enum {
    SYS_WRITE = 0,
    SYS_EXIT = 1,
    SYS_YIELD = 2,
};

interrupt_frame_t* syscall_handle(interrupt_frame_t* frame) {
    if (!frame) {
        return frame;
    }

    uint32_t num = frame->eax;
    switch (num) {
        case SYS_WRITE: {
            const char* buf = (const char*)frame->ebx;
            uint32_t len = frame->ecx;
            if (!buf) {
                frame->eax = (uint32_t)-1;
                return frame;
            }
            for (uint32_t i = 0; i < len; i++) {
                screen_putchar(buf[i]);
            }
            frame->eax = len;
            return frame;
        }
        case SYS_YIELD:
            frame->eax = 0;
            return tasking_yield(frame);
        case SYS_EXIT:
            frame->eax = 0;
            return tasking_exit(frame);
        default:
            frame->eax = (uint32_t)-1;
            return frame;
    }
}
