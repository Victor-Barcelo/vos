#include "syscall.h"
#include "screen.h"
#include "task.h"
#include "usercopy.h"
#include "timer.h"

enum {
    SYS_WRITE = 0,
    SYS_EXIT = 1,
    SYS_YIELD = 2,
    SYS_SLEEP = 3,
    SYS_WAIT  = 4,
    SYS_KILL  = 5,
    SYS_SBRK  = 6,
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
            if (len == 0) {
                frame->eax = 0;
                return frame;
            }
            if (!buf) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            char tmp[128];
            uint32_t remaining = len;
            const char* p = buf;
            while (remaining) {
                uint32_t chunk = remaining;
                if (chunk > (uint32_t)sizeof(tmp)) {
                    chunk = (uint32_t)sizeof(tmp);
                }
                if (!copy_from_user(tmp, p, chunk)) {
                    frame->eax = (uint32_t)-1;
                    return frame;
                }
                for (uint32_t i = 0; i < chunk; i++) {
                    screen_putchar(tmp[i]);
                }
                p += chunk;
                remaining -= chunk;
            }
            frame->eax = len;
            return frame;
        }
        case SYS_YIELD:
            frame->eax = 0;
            return tasking_yield(frame);
        case SYS_EXIT:
            frame->eax = 0;
            return tasking_exit(frame, (int32_t)frame->ebx);
        case SYS_SLEEP: {
            uint32_t ms = frame->ebx;
            if (ms == 0) {
                frame->eax = 0;
                return frame;
            }

            uint32_t hz = timer_get_hz();
            if (hz == 0) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            uint32_t ticks_to_wait = (ms * hz + 999u) / 1000u;
            if (ticks_to_wait == 0) {
                ticks_to_wait = 1;
            }
            uint32_t wake = timer_get_ticks() + ticks_to_wait;
            frame->eax = 0;
            return tasking_sleep_until(frame, wake);
        }
        case SYS_WAIT:
            return tasking_wait(frame, frame->ebx);
        case SYS_KILL: {
            uint32_t pid = frame->ebx;
            int32_t code = (int32_t)frame->ecx;
            bool ok = tasking_kill(pid, code);
            frame->eax = ok ? 0u : (uint32_t)-1;
            return frame;
        }
        case SYS_SBRK:
            return tasking_sbrk(frame, (int32_t)frame->ebx);
        default:
            frame->eax = (uint32_t)-1;
            return frame;
    }
}
