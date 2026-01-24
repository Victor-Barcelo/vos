#include "syscall.h"
#include "screen.h"
#include "task.h"
#include "usercopy.h"
#include "timer.h"
#include "rtc.h"
#include "vfs.h"
#include "kerrno.h"
#include "statusbar.h"
#include "system.h"
#include "kheap.h"
#include "string.h"

enum {
    SYS_WRITE = 0,
    SYS_EXIT = 1,
    SYS_YIELD = 2,
    SYS_SLEEP = 3,
    SYS_WAIT  = 4,
    SYS_KILL  = 5,
    SYS_SBRK  = 6,
    SYS_READFILE = 7,
    SYS_OPEN = 8,
    SYS_READ = 9,
    SYS_CLOSE = 10,
    SYS_LSEEK = 11,
    SYS_FSTAT = 12,
    SYS_STAT = 13,
    SYS_MKDIR = 14,
    SYS_READDIR = 15,
    SYS_CHDIR = 16,
    SYS_GETCWD = 17,
    SYS_IOCTL = 18,
    SYS_UNLINK = 19,
    SYS_RENAME = 20,
    SYS_RMDIR = 21,
    SYS_TRUNCATE = 22,
    SYS_FTRUNCATE = 23,
    SYS_FSYNC = 24,
    SYS_DUP = 25,
    SYS_DUP2 = 26,
    SYS_PIPE = 27,
    SYS_GETPID = 28,
    SYS_SPAWN = 29,
    SYS_UPTIME_MS = 30,
    SYS_RTC_GET = 31,
    SYS_RTC_SET = 32,
    SYS_TASK_COUNT = 33,
    SYS_TASK_INFO = 34,
    SYS_SCREEN_IS_FB = 35,
    SYS_GFX_CLEAR = 36,
    SYS_GFX_PSET = 37,
    SYS_GFX_LINE = 38,
    SYS_MEM_TOTAL_KB = 39,
    SYS_CPU_VENDOR = 40,
    SYS_CPU_BRAND = 41,
    SYS_VFS_FILE_COUNT = 42,
    SYS_FONT_COUNT = 43,
    SYS_FONT_GET = 44,
    SYS_FONT_INFO = 45,
    SYS_FONT_SET = 46,
    SYS_GFX_BLIT_RGBA = 47,
    SYS_MMAP = 48,
    SYS_MUNMAP = 49,
    SYS_MPROTECT = 50,
    SYS_GETUID = 51,
    SYS_SETUID = 52,
    SYS_GETGID = 53,
    SYS_SETGID = 54,
    SYS_SIGNAL = 55,
    SYS_SIGRETURN = 56,
    SYS_SIGPROCMASK = 57,
    SYS_GETPPID = 58,
    SYS_GETPGRP = 59,
    SYS_SETPGID = 60,
    SYS_FCNTL = 61,
    SYS_ALARM = 62,
    SYS_LSTAT = 63,
    SYS_SYMLINK = 64,
    SYS_READLINK = 65,
    SYS_CHMOD = 66,
    SYS_FCHMOD = 67,
    SYS_FORK = 68,
    SYS_EXECVE = 69,
    SYS_WAITPID = 70,
};

typedef struct vos_task_info_user {
    uint32_t pid;
    uint32_t user;
    uint32_t state;
    uint32_t cpu_ticks;
    uint32_t eip;
    uint32_t esp;
    int32_t exit_code;
    uint32_t wake_tick;
    uint32_t wait_pid;
    char name[16];
} vos_task_info_user_t;

typedef struct vos_font_info_user {
    char name[32];
    uint32_t width;
    uint32_t height;
} vos_font_info_user_t;

static int32_t copy_kernel_string_to_user(char* dst_user, uint32_t cap, const char* src) {
    if (cap == 0) {
        return -EINVAL;
    }
    if (!dst_user) {
        return -EFAULT;
    }
    if (!src) {
        src = "";
    }

    uint32_t n = (uint32_t)strlen(src);
    if (n >= cap) {
        n = cap - 1u;
    }

    if (n != 0 && !copy_to_user(dst_user, src, n)) {
        return -EFAULT;
    }
    char z = '\0';
    if (!copy_to_user(dst_user + n, &z, 1u)) {
        return -EFAULT;
    }
    return 0;
}

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

    int32_t pending_exit = 0;
    if ((frame->cs & 3u) == 3u && tasking_current_should_exit(&pending_exit)) {
        return tasking_exit(frame, pending_exit);
    }

    uint32_t num = frame->eax;
    switch (num) {
        case SYS_WRITE: {
            int32_t fd = (int32_t)frame->ebx;
            const void* buf_user = (const void*)frame->ecx;
            uint32_t len = frame->edx;
            int32_t n = tasking_fd_write(fd, buf_user, len);
            frame->eax = (uint32_t)n;
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
        case SYS_WAITPID: {
            int32_t pid = (int32_t)frame->ebx;
            void* status_user = (void*)frame->ecx;
            int32_t options = (int32_t)frame->edx;
            return tasking_waitpid(frame, pid, status_user, options);
        }
        case SYS_KILL: {
            int32_t pid = (int32_t)frame->ebx;
            int32_t sig = (int32_t)frame->ecx;
            int32_t rc = tasking_kill(pid, sig);
            frame->eax = (uint32_t)rc;
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
        case SYS_OPEN: {
            const char* path_user = (const char*)frame->ebx;
            uint32_t flags = frame->ecx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-1;
                return frame;
            }

            int32_t fd = tasking_fd_open(path, flags);
            frame->eax = (uint32_t)fd;
            return frame;
        }
        case SYS_READ: {
            int32_t fd = (int32_t)frame->ebx;
            void* dst_user = (void*)frame->ecx;
            uint32_t len = frame->edx;
            int32_t n = tasking_fd_read(fd, dst_user, len);
            frame->eax = (uint32_t)n;
            if ((frame->cs & 3u) == 3u && tasking_current_should_exit(&pending_exit)) {
                return tasking_exit(frame, pending_exit);
            }
            return frame;
        }
        case SYS_CLOSE: {
            int32_t fd = (int32_t)frame->ebx;
            int32_t rc = tasking_fd_close(fd);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_LSEEK: {
            int32_t fd = (int32_t)frame->ebx;
            int32_t offset = (int32_t)frame->ecx;
            int32_t whence = (int32_t)frame->edx;
            int32_t rc = tasking_fd_lseek(fd, offset, whence);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_FSTAT: {
            int32_t fd = (int32_t)frame->ebx;
            void* st_user = (void*)frame->ecx;
            int32_t rc = tasking_fd_fstat(fd, st_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_STAT: {
            const char* path_user = (const char*)frame->ebx;
            void* st_user = (void*)frame->ecx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_stat(path, st_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_LSTAT: {
            const char* path_user = (const char*)frame->ebx;
            void* st_user = (void*)frame->ecx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_lstat(path, st_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_MKDIR: {
            const char* path_user = (const char*)frame->ebx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_mkdir(path);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_READDIR: {
            int32_t fd = (int32_t)frame->ebx;
            void* de_user = (void*)frame->ecx;
            int32_t rc = tasking_readdir(fd, de_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_CHDIR: {
            const char* path_user = (const char*)frame->ebx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_chdir(path);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_GETCWD: {
            void* buf_user = (void*)frame->ebx;
            uint32_t len = frame->ecx;
            int32_t rc = tasking_getcwd(buf_user, len);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_IOCTL: {
            int32_t fd = (int32_t)frame->ebx;
            uint32_t req = frame->ecx;
            void* argp_user = (void*)frame->edx;
            int32_t rc = tasking_fd_ioctl(fd, req, argp_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_UNLINK: {
            const char* path_user = (const char*)frame->ebx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_unlink(path);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_RENAME: {
            const char* old_user = (const char*)frame->ebx;
            const char* new_user = (const char*)frame->ecx;

            char oldp[128];
            char newp[128];
            if (!copy_user_cstring(oldp, sizeof(oldp), old_user) || !copy_user_cstring(newp, sizeof(newp), new_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_rename(oldp, newp);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_RMDIR: {
            const char* path_user = (const char*)frame->ebx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_rmdir(path);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_TRUNCATE: {
            const char* path_user = (const char*)frame->ebx;
            uint32_t new_size = frame->ecx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_truncate(path, new_size);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_FTRUNCATE: {
            int32_t fd = (int32_t)frame->ebx;
            uint32_t new_size = frame->ecx;
            int32_t rc = tasking_fd_ftruncate(fd, new_size);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_FSYNC: {
            int32_t fd = (int32_t)frame->ebx;
            int32_t rc = tasking_fd_fsync(fd);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_SYMLINK: {
            const char* target_user = (const char*)frame->ebx;
            const char* linkpath_user = (const char*)frame->ecx;

            char target[256];
            char linkpath[256];
            if (!copy_user_cstring(target, sizeof(target), target_user) ||
                !copy_user_cstring(linkpath, sizeof(linkpath), linkpath_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_symlink(target, linkpath);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_READLINK: {
            const char* path_user = (const char*)frame->ebx;
            void* buf_user = (void*)frame->ecx;
            uint32_t cap = frame->edx;

            char path[256];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_readlink(path, buf_user, cap);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_CHMOD: {
            const char* path_user = (const char*)frame->ebx;
            uint32_t mode = frame->ecx;

            char path[256];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_chmod(path, (uint16_t)mode);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_FCHMOD: {
            int32_t fd = (int32_t)frame->ebx;
            uint32_t mode = frame->ecx;
            int32_t rc = tasking_fd_fchmod(fd, (uint16_t)mode);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_DUP: {
            int32_t oldfd = (int32_t)frame->ebx;
            int32_t rc = tasking_fd_dup(oldfd);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_DUP2: {
            int32_t oldfd = (int32_t)frame->ebx;
            int32_t newfd = (int32_t)frame->ecx;
            int32_t rc = tasking_fd_dup2(oldfd, newfd);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_PIPE: {
            void* pipefds_user = (void*)frame->ebx;
            int32_t rc = tasking_pipe(pipefds_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_FCNTL: {
            int32_t fd = (int32_t)frame->ebx;
            int32_t cmd = (int32_t)frame->ecx;
            int32_t arg = (int32_t)frame->edx;
            int32_t rc = tasking_fd_fcntl(fd, cmd, arg);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_ALARM: {
            uint32_t seconds = frame->ebx;
            int32_t rc = tasking_alarm(seconds);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_GETPID: {
            frame->eax = tasking_current_pid();
            return frame;
        }
        case SYS_FORK: {
            int32_t pid = tasking_fork(frame);
            frame->eax = (uint32_t)pid;
            return frame;
        }
        case SYS_EXECVE: {
            const char* path_user = (const char*)frame->ebx;
            const char* const* argv_user = (const char* const*)frame->ecx;
            uint32_t argc = frame->edx;

            if (argc > 32u) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            const char* kargv[32];
            char argv_buf[32][128];

            if (argc != 0 && argv_user) {
                for (uint32_t i = 0; i < argc; i++) {
                    const char* argp_user = NULL;
                    if (!copy_from_user(&argp_user, argv_user + i, (uint32_t)sizeof(argp_user))) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }

                    if (!argp_user) {
                        argv_buf[i][0] = '\0';
                        kargv[i] = argv_buf[i];
                        continue;
                    }

                    if (!copy_user_cstring(argv_buf[i], sizeof(argv_buf[i]), argp_user)) {
                        frame->eax = (uint32_t)-ENAMETOOLONG;
                        return frame;
                    }
                    kargv[i] = argv_buf[i];
                }
            }

            int32_t rc = tasking_execve(frame, path, (argc != 0 && argv_user) ? kargv : NULL, argc);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_SPAWN: {
            const char* path_user = (const char*)frame->ebx;
            const char* const* argv_user = (const char* const*)frame->ecx;
            uint32_t argc = frame->edx;

            if (argc > 32u) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            const char* kargv[32];
            char argv_buf[32][128];

            if (argc != 0 && argv_user) {
                for (uint32_t i = 0; i < argc; i++) {
                    const char* argp_user = NULL;
                    if (!copy_from_user(&argp_user, argv_user + i, (uint32_t)sizeof(argp_user))) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }

                    if (!argp_user) {
                        argv_buf[i][0] = '\0';
                        kargv[i] = argv_buf[i];
                        continue;
                    }

                    if (!copy_user_cstring(argv_buf[i], sizeof(argv_buf[i]), argp_user)) {
                        frame->eax = (uint32_t)-ENAMETOOLONG;
                        return frame;
                    }
                    kargv[i] = argv_buf[i];
                }
            }

            int32_t pid = tasking_spawn_exec(path, (argc != 0 && argv_user) ? kargv : NULL, argc);
            frame->eax = (uint32_t)pid;
            return frame;
        }
        case SYS_UPTIME_MS: {
            frame->eax = timer_uptime_ms();
            return frame;
        }
        case SYS_RTC_GET: {
            void* dt_user = (void*)frame->ebx;
            if (!dt_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            rtc_datetime_t dt;
            if (!rtc_read_datetime(&dt)) {
                frame->eax = (uint32_t)-EIO;
                return frame;
            }

            if (!copy_to_user(dt_user, &dt, (uint32_t)sizeof(dt))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            frame->eax = 0;
            return frame;
        }
        case SYS_RTC_SET: {
            const void* dt_user = (const void*)frame->ebx;
            if (!dt_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            rtc_datetime_t dt;
            if (!copy_from_user(&dt, dt_user, (uint32_t)sizeof(dt))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            if (!rtc_set_datetime(&dt)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            statusbar_refresh();
            frame->eax = 0;
            return frame;
        }
        case SYS_TASK_COUNT: {
            frame->eax = tasking_task_count();
            return frame;
        }
        case SYS_TASK_INFO: {
            uint32_t index = frame->ebx;
            void* out_user = (void*)frame->ecx;
            if (!out_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            task_info_t info;
            if (!tasking_get_task_info(index, &info)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            vos_task_info_user_t out;
            out.pid = info.pid;
            out.user = info.user ? 1u : 0u;
            out.state = (uint32_t)info.state;
            out.cpu_ticks = info.cpu_ticks;
            out.eip = info.eip;
            out.esp = info.esp;
            out.exit_code = info.exit_code;
            out.wake_tick = info.wake_tick;
            out.wait_pid = info.wait_pid;
            for (uint32_t i = 0; i < (uint32_t)sizeof(out.name); i++) {
                out.name[i] = info.name[i];
            }

            if (!copy_to_user(out_user, &out, (uint32_t)sizeof(out))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            frame->eax = 0;
            return frame;
        }
        case SYS_SCREEN_IS_FB: {
            frame->eax = screen_is_framebuffer() ? 1u : 0u;
            return frame;
        }
        case SYS_GFX_CLEAR: {
            uint32_t bg = frame->ebx & 0xFFu;
            if (!screen_is_framebuffer()) {
                frame->eax = (uint32_t)-ENODEV;
                return frame;
            }
            (void)screen_graphics_clear((uint8_t)bg);
            frame->eax = 0;
            return frame;
        }
        case SYS_GFX_PSET: {
            int32_t x = (int32_t)frame->ebx;
            int32_t y = (int32_t)frame->ecx;
            uint32_t c = frame->edx & 0xFFu;
            if (!screen_is_framebuffer()) {
                frame->eax = (uint32_t)-ENODEV;
                return frame;
            }
            bool ok = screen_graphics_putpixel(x, y, (uint8_t)c);
            frame->eax = ok ? 0u : (uint32_t)-EINVAL;
            return frame;
        }
        case SYS_GFX_LINE: {
            int32_t x0 = (int32_t)frame->ebx;
            int32_t y0 = (int32_t)frame->ecx;
            int32_t x1 = (int32_t)frame->edx;
            int32_t y1 = (int32_t)frame->esi;
            uint32_t c = frame->edi & 0xFFu;
            if (!screen_is_framebuffer()) {
                frame->eax = (uint32_t)-ENODEV;
                return frame;
            }
            (void)screen_graphics_line(x0, y0, x1, y1, (uint8_t)c);
            frame->eax = 0;
            return frame;
        }
        case SYS_GFX_BLIT_RGBA: {
            int32_t x = (int32_t)frame->ebx;
            int32_t y = (int32_t)frame->ecx;
            uint32_t w = frame->edx;
            uint32_t h = frame->esi;
            const void* src_user = (const void*)frame->edi;

            if (!screen_is_framebuffer()) {
                frame->eax = (uint32_t)-ENODEV;
                return frame;
            }
            if (!src_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            if (w == 0 || h == 0) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }
            if (x < 0 || y < 0) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }
            if (w > 0x3FFFFFFFu) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }
            uint32_t fb_w = screen_framebuffer_width();
            uint32_t fb_h = screen_framebuffer_height();
            if ((uint32_t)x + w > fb_w || (uint32_t)y + h > fb_h) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            uint32_t row_bytes = w * 4u;
            uint8_t* row = (uint8_t*)kmalloc(row_bytes);
            if (!row) {
                frame->eax = (uint32_t)-ENOMEM;
                return frame;
            }

            for (uint32_t yy = 0; yy < h; yy++) {
                const uint8_t* src_row = (const uint8_t*)src_user + yy * row_bytes;
                if (!copy_from_user(row, src_row, row_bytes)) {
                    kfree(row);
                    frame->eax = (uint32_t)-EFAULT;
                    return frame;
                }
                (void)screen_graphics_blit_rgba(x, y + (int32_t)yy, w, 1u, row, row_bytes);
            }

            kfree(row);
            frame->eax = 0;
            return frame;
        }
        case SYS_MEM_TOTAL_KB: {
            frame->eax = system_mem_total_kb();
            return frame;
        }
        case SYS_CPU_VENDOR: {
            char* buf_user = (char*)frame->ebx;
            uint32_t len = frame->ecx;
            int32_t rc = copy_kernel_string_to_user(buf_user, len, system_cpu_vendor());
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_CPU_BRAND: {
            char* buf_user = (char*)frame->ebx;
            uint32_t len = frame->ecx;
            int32_t rc = copy_kernel_string_to_user(buf_user, len, system_cpu_brand());
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_VFS_FILE_COUNT: {
            frame->eax = vfs_file_count();
            return frame;
        }
        case SYS_FONT_COUNT: {
            frame->eax = (uint32_t)screen_font_count();
            return frame;
        }
        case SYS_FONT_GET: {
            frame->eax = (uint32_t)screen_font_get_current();
            return frame;
        }
        case SYS_FONT_INFO: {
            uint32_t idx = frame->ebx;
            vos_font_info_user_t* out_user = (vos_font_info_user_t*)frame->ecx;

            screen_font_info_t info;
            int rc = screen_font_get_info((int)idx, &info);
            if (rc < 0) {
                frame->eax = (uint32_t)rc;
                return frame;
            }

            vos_font_info_user_t out;
            memset(&out, 0, sizeof(out));
            strncpy(out.name, info.name, sizeof(out.name) - 1u);
            out.width = info.width;
            out.height = info.height;

            if (!copy_to_user(out_user, &out, (uint32_t)sizeof(out))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_FONT_SET: {
            uint32_t idx = frame->ebx;
            frame->eax = (uint32_t)screen_font_set((int)idx);
            return frame;
        }
        case SYS_MMAP: {
            uint32_t addr_hint = frame->ebx;
            uint32_t length = frame->ecx;
            uint32_t prot = frame->edx;
            uint32_t flags = frame->esi;
            int32_t fd = (int32_t)frame->edi;

            uint32_t out_addr = 0;
            int32_t rc = tasking_mmap(addr_hint, length, prot, flags, fd, 0, &out_addr);
            frame->eax = (rc < 0) ? (uint32_t)rc : out_addr;
            return frame;
        }
        case SYS_MUNMAP: {
            uint32_t addr = frame->ebx;
            uint32_t length = frame->ecx;
            int32_t rc = tasking_munmap(addr, length);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_MPROTECT: {
            uint32_t addr = frame->ebx;
            uint32_t length = frame->ecx;
            uint32_t prot = frame->edx;
            int32_t rc = tasking_mprotect(addr, length, prot);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_GETUID:
            frame->eax = tasking_getuid();
            return frame;
        case SYS_SETUID: {
            uint32_t uid = frame->ebx;
            int32_t rc = tasking_setuid(uid);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_GETGID:
            frame->eax = tasking_getgid();
            return frame;
        case SYS_SETGID: {
            uint32_t gid = frame->ebx;
            int32_t rc = tasking_setgid(gid);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_SIGNAL: {
            int32_t sig = (int32_t)frame->ebx;
            uint32_t handler = frame->ecx;
            uint32_t old = 0;
            int32_t rc = tasking_signal_set_handler(sig, handler, &old);
            frame->eax = (uint32_t)((rc < 0) ? rc : (int32_t)old);
            return frame;
        }
        case SYS_SIGPROCMASK: {
            int32_t how = (int32_t)frame->ebx;
            const void* set_user = (const void*)frame->ecx;
            void* old_user = (void*)frame->edx;
            int32_t rc = tasking_sigprocmask(how, set_user, old_user);
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_SIGRETURN:
            return tasking_sigreturn(frame);
        case SYS_GETPPID:
            frame->eax = tasking_current_ppid();
            return frame;
        case SYS_GETPGRP:
            frame->eax = tasking_getpgrp();
            return frame;
        case SYS_SETPGID: {
            int32_t pid = (int32_t)frame->ebx;
            int32_t pgid = (int32_t)frame->ecx;
            frame->eax = (uint32_t)tasking_setpgid(pid, pgid);
            return frame;
        }
        default:
            frame->eax = (uint32_t)-1;
            return frame;
    }
}
