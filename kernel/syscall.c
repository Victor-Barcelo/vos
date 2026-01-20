#include "syscall.h"
#include "screen.h"
#include "task.h"
#include "usercopy.h"
#include "timer.h"
#include "vfs.h"

enum {
    SYS_WRITE = 0,
    SYS_EXIT = 1,
    SYS_YIELD = 2,
    SYS_SLEEP = 3,
    SYS_WAIT  = 4,
    SYS_KILL  = 5,
    SYS_SBRK  = 6,
    SYS_READFILE = 7,
};

static bool copy_user_cstring(char* dst, uint32_t dst_cap, const char* src_user) {
    if (!dst || dst_cap == 0) {
        return false;
    }
    if (!src_user) {
        return false;
    }

    for (uint32_t i = 0; i < dst_cap; i++) {
        char c = 0;
        if (!copy_from_user(&c, src_user + i, 1u)) {
            return false;
        }
        dst[i] = c;
        if (c == '\0') {
            return true;
        }
    }

    dst[dst_cap - 1u] = '\0';
    return false; // unterminated / too long
}

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
        case SYS_READFILE: {
            const char* path_user = (const char*)frame->ebx;
            void* dst_user = (void*)frame->ecx;
            uint32_t dst_len = frame->edx;
            uint32_t offset = frame->esi;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            const uint8_t* data = NULL;
            uint32_t size = 0;
            if (!vfs_read_file(path, &data, &size) || !data) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            if (offset >= size) {
                frame->eax = 0;
                return frame;
            }
            uint32_t avail = size - offset;
            uint32_t to_copy = dst_len;
            if (to_copy > avail) {
                to_copy = avail;
            }
            if (to_copy == 0) {
                frame->eax = 0;
                return frame;
            }
            if (!dst_user) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            uint32_t remaining = to_copy;
            const uint8_t* src = data + offset;
            uint8_t* dst = (uint8_t*)dst_user;
            while (remaining) {
                uint32_t chunk = remaining;
                if (chunk > 256u) {
                    chunk = 256u;
                }
                if (!copy_to_user(dst, src, chunk)) {
                    frame->eax = (uint32_t)-1;
                    return frame;
                }
                dst += chunk;
                src += chunk;
                remaining -= chunk;
            }

            frame->eax = to_copy;
            return frame;
        }
        default:
            frame->eax = (uint32_t)-1;
            return frame;
    }
}
