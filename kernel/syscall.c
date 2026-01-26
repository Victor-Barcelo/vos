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
#include "pmm.h"
#include "interrupts.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "serial.h"

// Keep syscall argv marshalling bounded (argv strings are copied into
// kernel memory before switching address spaces for exec/spawn).
#define VOS_EXEC_ARG_STR_MAX 4096u
#define VOS_EXEC_ARG_MAXBYTES (128u * 1024u)

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
    SYS_STATFS = 71,
    SYS_PMM_INFO = 72,
    SYS_HEAP_INFO = 73,
    SYS_TIMER_INFO = 74,
    SYS_IRQ_STATS = 75,
    SYS_SCHED_STATS = 76,
    SYS_DESCRIPTOR_INFO = 77,
    SYS_SYSCALL_STATS = 78,
    SYS_SELECT = 79,
    SYS_THEME_COUNT = 80,
    SYS_THEME_GET = 81,
    SYS_THEME_INFO = 82,
    SYS_THEME_SET = 83,
    SYS_GETTIMEOFDAY = 84,
    SYS_CLOCK_GETTIME = 85,
    SYS_NANOSLEEP = 86,
    SYS_ACCESS = 87,
    SYS_ISATTY = 88,
    SYS_UNAME = 89,
    SYS_POLL = 90,
    SYS_MAX = 91,
};

// Syscall counters - track how many times each syscall is invoked
static uint32_t syscall_counts[SYS_MAX] = {0};

// Syscall name table for introspection
static const char* syscall_names[SYS_MAX] = {
    [SYS_WRITE] = "write",
    [SYS_EXIT] = "exit",
    [SYS_YIELD] = "yield",
    [SYS_SLEEP] = "sleep",
    [SYS_WAIT] = "wait",
    [SYS_KILL] = "kill",
    [SYS_SBRK] = "sbrk",
    [SYS_READFILE] = "readfile",
    [SYS_OPEN] = "open",
    [SYS_READ] = "read",
    [SYS_CLOSE] = "close",
    [SYS_LSEEK] = "lseek",
    [SYS_FSTAT] = "fstat",
    [SYS_STAT] = "stat",
    [SYS_MKDIR] = "mkdir",
    [SYS_READDIR] = "readdir",
    [SYS_CHDIR] = "chdir",
    [SYS_GETCWD] = "getcwd",
    [SYS_IOCTL] = "ioctl",
    [SYS_UNLINK] = "unlink",
    [SYS_RENAME] = "rename",
    [SYS_RMDIR] = "rmdir",
    [SYS_TRUNCATE] = "truncate",
    [SYS_FTRUNCATE] = "ftruncate",
    [SYS_FSYNC] = "fsync",
    [SYS_DUP] = "dup",
    [SYS_DUP2] = "dup2",
    [SYS_PIPE] = "pipe",
    [SYS_GETPID] = "getpid",
    [SYS_SPAWN] = "spawn",
    [SYS_UPTIME_MS] = "uptime_ms",
    [SYS_RTC_GET] = "rtc_get",
    [SYS_RTC_SET] = "rtc_set",
    [SYS_TASK_COUNT] = "task_count",
    [SYS_TASK_INFO] = "task_info",
    [SYS_SCREEN_IS_FB] = "screen_is_fb",
    [SYS_GFX_CLEAR] = "gfx_clear",
    [SYS_GFX_PSET] = "gfx_pset",
    [SYS_GFX_LINE] = "gfx_line",
    [SYS_MEM_TOTAL_KB] = "mem_total_kb",
    [SYS_CPU_VENDOR] = "cpu_vendor",
    [SYS_CPU_BRAND] = "cpu_brand",
    [SYS_VFS_FILE_COUNT] = "vfs_file_count",
    [SYS_FONT_COUNT] = "font_count",
    [SYS_FONT_GET] = "font_get",
    [SYS_FONT_INFO] = "font_info",
    [SYS_FONT_SET] = "font_set",
    [SYS_GFX_BLIT_RGBA] = "gfx_blit_rgba",
    [SYS_MMAP] = "mmap",
    [SYS_MUNMAP] = "munmap",
    [SYS_MPROTECT] = "mprotect",
    [SYS_GETUID] = "getuid",
    [SYS_SETUID] = "setuid",
    [SYS_GETGID] = "getgid",
    [SYS_SETGID] = "setgid",
    [SYS_SIGNAL] = "signal",
    [SYS_SIGRETURN] = "sigreturn",
    [SYS_SIGPROCMASK] = "sigprocmask",
    [SYS_GETPPID] = "getppid",
    [SYS_GETPGRP] = "getpgrp",
    [SYS_SETPGID] = "setpgid",
    [SYS_FCNTL] = "fcntl",
    [SYS_ALARM] = "alarm",
    [SYS_LSTAT] = "lstat",
    [SYS_SYMLINK] = "symlink",
    [SYS_READLINK] = "readlink",
    [SYS_CHMOD] = "chmod",
    [SYS_FCHMOD] = "fchmod",
    [SYS_FORK] = "fork",
    [SYS_EXECVE] = "execve",
    [SYS_WAITPID] = "waitpid",
    [SYS_STATFS] = "statfs",
    [SYS_PMM_INFO] = "pmm_info",
    [SYS_HEAP_INFO] = "heap_info",
    [SYS_TIMER_INFO] = "timer_info",
    [SYS_IRQ_STATS] = "irq_stats",
    [SYS_SCHED_STATS] = "sched_stats",
    [SYS_DESCRIPTOR_INFO] = "desc_info",
    [SYS_SYSCALL_STATS] = "syscall_stats",
    [SYS_SELECT] = "select",
    [SYS_THEME_COUNT] = "theme_count",
    [SYS_THEME_GET] = "theme_get",
    [SYS_THEME_INFO] = "theme_info",
    [SYS_THEME_SET] = "theme_set",
    [SYS_GETTIMEOFDAY] = "gettimeofday",
    [SYS_CLOCK_GETTIME] = "clock_gettime",
    [SYS_NANOSLEEP] = "nanosleep",
    [SYS_ACCESS] = "access",
    [SYS_ISATTY] = "isatty",
    [SYS_UNAME] = "uname",
    [SYS_POLL] = "poll",
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

// Sysview introspection structures
typedef struct vos_pmm_info_user {
    uint32_t total_frames;
    uint32_t free_frames;
    uint32_t page_size;
} vos_pmm_info_user_t;

typedef struct vos_heap_info_user {
    uint32_t heap_base;
    uint32_t heap_end;
    uint32_t total_free_bytes;
    uint32_t free_block_count;
} vos_heap_info_user_t;

typedef struct vos_timer_info_user {
    uint32_t ticks;
    uint32_t hz;
    uint32_t uptime_ms;
} vos_timer_info_user_t;

typedef struct vos_irq_stats_user {
    uint32_t counts[16];
} vos_irq_stats_user_t;

typedef struct vos_sched_stats_user {
    uint32_t context_switches;
    uint32_t task_count;
    uint32_t runnable;
    uint32_t sleeping;
    uint32_t waiting;
    uint32_t zombie;
} vos_sched_stats_user_t;

typedef struct vos_descriptor_info_user {
    uint32_t gdt_base;
    uint32_t gdt_entries;
    uint32_t idt_base;
    uint32_t idt_entries;
    uint32_t tss_esp0;
} vos_descriptor_info_user_t;

#define VOS_SYSCALL_STATS_MAX 80
typedef struct vos_syscall_stats_user {
    uint32_t num_syscalls;               // Total number of syscalls supported
    uint32_t counts[VOS_SYSCALL_STATS_MAX]; // Count for each syscall
    char names[VOS_SYSCALL_STATS_MAX][16];  // Name of each syscall (truncated)
} vos_syscall_stats_user_t;

// For select() syscall
typedef struct vos_timeval {
    int32_t tv_sec;
    int32_t tv_usec;
} vos_timeval_t;

// fd_set is a bitmask - 32 fds per word, we support up to 64 fds
#define VOS_FD_SETSIZE 64
typedef struct vos_fd_set {
    uint32_t bits[VOS_FD_SETSIZE / 32];
} vos_fd_set_t;

static inline bool fd_set_isset(const vos_fd_set_t* set, int fd) {
    if (!set || fd < 0 || fd >= VOS_FD_SETSIZE) return false;
    return (set->bits[fd / 32] & (1u << (fd % 32))) != 0;
}

static inline void fd_set_set(vos_fd_set_t* set, int fd) {
    if (!set || fd < 0 || fd >= VOS_FD_SETSIZE) return;
    set->bits[fd / 32] |= (1u << (fd % 32));
}

static inline void fd_set_clr(vos_fd_set_t* set, int fd) {
    if (!set || fd < 0 || fd >= VOS_FD_SETSIZE) return;
    set->bits[fd / 32] &= ~(1u << (fd % 32));
}

// For clock_gettime / nanosleep
typedef struct vos_timespec {
    int32_t tv_sec;
    int32_t tv_nsec;
} vos_timespec_t;

// Clock IDs for clock_gettime
#define VOS_CLOCK_REALTIME  0
#define VOS_CLOCK_MONOTONIC 1

// For access() syscall
#define VOS_F_OK 0  // Test for existence
#define VOS_R_OK 4  // Test for read permission
#define VOS_W_OK 2  // Test for write permission
#define VOS_X_OK 1  // Test for execute permission

// For uname() syscall
typedef struct vos_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
} vos_utsname_t;

// For poll() syscall
typedef struct vos_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
} vos_pollfd_t;

#define VOS_POLLIN   0x0001
#define VOS_POLLOUT  0x0004
#define VOS_POLLERR  0x0008
#define VOS_POLLHUP  0x0010
#define VOS_POLLNVAL 0x0020

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

static int32_t dup_user_cstring(const char* src_user, uint32_t max_len, char** out_str, uint32_t* out_bytes) {
    if (!out_str || max_len == 0) {
        return -EINVAL;
    }
    *out_str = NULL;
    if (out_bytes) {
        *out_bytes = 0;
    }

    // Treat NULL as empty string for robustness.
    if (!src_user) {
        char* s = (char*)kmalloc(1u);
        if (!s) {
            return -ENOMEM;
        }
        s[0] = '\0';
        *out_str = s;
        if (out_bytes) {
            *out_bytes = 1u;
        }
        return 0;
    }

    bool found = false;
    uint32_t len = 0;
    for (; len < max_len; len++) {
        char c = 0;
        if (!copy_from_user(&c, src_user + len, 1u)) {
            return -EFAULT;
        }
        if (c == '\0') {
            found = true;
            len++; // include terminator
            break;
        }
    }
    if (!found) {
        return -ENAMETOOLONG;
    }

    char* s = (char*)kmalloc(len);
    if (!s) {
        return -ENOMEM;
    }
    if (!copy_from_user(s, src_user, len)) {
        kfree(s);
        return -EFAULT;
    }

    *out_str = s;
    if (out_bytes) {
        *out_bytes = len;
    }
    return 0;
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

    // Track syscall invocation count
    if (num < SYS_MAX) {
        syscall_counts[num]++;
    }

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
        case SYS_STATFS: {
            const char* path_user = (const char*)frame->ebx;
            void* st_user = (void*)frame->ecx;

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            int32_t rc = tasking_statfs(path, st_user);
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

            int32_t rc = 0;
            const char** kargv = NULL;
            uint32_t argv_bytes = 0;

            if (argc > VOS_EXEC_MAX_ARGS) {
                rc = -EINVAL;
                goto out_execve;
            }
            if (argc != 0 && !argv_user) {
                rc = -EFAULT;
                goto out_execve;
            }

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                rc = -EFAULT;
                goto out_execve;
            }

            if (argc != 0) {
                kargv = (const char**)kmalloc(argc * (uint32_t)sizeof(*kargv));
                if (!kargv) {
                    rc = -ENOMEM;
                    goto out_execve;
                }

                for (uint32_t i = 0; i < argc; i++) {
                    kargv[i] = NULL;
                }

                for (uint32_t i = 0; i < argc; i++) {
                    const char* argp_user = NULL;
                    if (!copy_from_user(&argp_user, argv_user + i, (uint32_t)sizeof(argp_user))) {
                        rc = -EFAULT;
                        goto out_execve;
                    }

                    char* s = NULL;
                    uint32_t bytes = 0;
                    rc = dup_user_cstring(argp_user, VOS_EXEC_ARG_STR_MAX, &s, &bytes);
                    if (rc < 0) {
                        goto out_execve;
                    }
                    if (argv_bytes + bytes > VOS_EXEC_ARG_MAXBYTES) {
                        kfree(s);
                        rc = -E2BIG;
                        goto out_execve;
                    }
                    argv_bytes += bytes;
                    kargv[i] = s;
                }
            }

            rc = tasking_execve(frame, path, (argc != 0) ? kargv : NULL, argc);

        out_execve:
            if (kargv) {
                for (uint32_t i = 0; i < argc; i++) {
                    if (kargv[i]) {
                        kfree((void*)kargv[i]);
                    }
                }
                kfree(kargv);
            }
            frame->eax = (uint32_t)rc;
            return frame;
        }
        case SYS_SPAWN: {
            const char* path_user = (const char*)frame->ebx;
            const char* const* argv_user = (const char* const*)frame->ecx;
            uint32_t argc = frame->edx;

            int32_t pid = -1;
            int32_t rc = 0;
            const char** kargv = NULL;
            uint32_t argv_bytes = 0;

            if (argc > VOS_EXEC_MAX_ARGS) {
                rc = -EINVAL;
                goto out_spawn;
            }
            if (argc != 0 && !argv_user) {
                rc = -EFAULT;
                goto out_spawn;
            }

            char path[128];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                rc = -EFAULT;
                goto out_spawn;
            }

            if (argc != 0) {
                kargv = (const char**)kmalloc(argc * (uint32_t)sizeof(*kargv));
                if (!kargv) {
                    rc = -ENOMEM;
                    goto out_spawn;
                }

                for (uint32_t i = 0; i < argc; i++) {
                    kargv[i] = NULL;
                }

                for (uint32_t i = 0; i < argc; i++) {
                    const char* argp_user = NULL;
                    if (!copy_from_user(&argp_user, argv_user + i, (uint32_t)sizeof(argp_user))) {
                        rc = -EFAULT;
                        goto out_spawn;
                    }

                    char* s = NULL;
                    uint32_t bytes = 0;
                    rc = dup_user_cstring(argp_user, VOS_EXEC_ARG_STR_MAX, &s, &bytes);
                    if (rc < 0) {
                        goto out_spawn;
                    }
                    if (argv_bytes + bytes > VOS_EXEC_ARG_MAXBYTES) {
                        kfree(s);
                        rc = -E2BIG;
                        goto out_spawn;
                    }
                    argv_bytes += bytes;
                    kargv[i] = s;
                }
            }

            pid = tasking_spawn_exec(path, (argc != 0) ? kargv : NULL, argc);
            rc = 0;

        out_spawn:
            if (kargv) {
                for (uint32_t i = 0; i < argc; i++) {
                    if (kargv[i]) {
                        kfree((void*)kargv[i]);
                    }
                }
                kfree(kargv);
            }
            frame->eax = (uint32_t)((rc < 0) ? rc : pid);
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

        // Sysview introspection syscalls
        case SYS_PMM_INFO: {
            vos_pmm_info_user_t* info_user = (vos_pmm_info_user_t*)frame->ebx;
            if (!info_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_pmm_info_user_t info;
            info.total_frames = pmm_total_frames();
            info.free_frames = pmm_free_frames();
            info.page_size = 4096;
            if (!copy_to_user(info_user, &info, sizeof(info))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_HEAP_INFO: {
            vos_heap_info_user_t* info_user = (vos_heap_info_user_t*)frame->ebx;
            if (!info_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_heap_info_user_t info;
            kheap_get_info(&info.heap_base, &info.heap_end,
                           &info.total_free_bytes, &info.free_block_count);
            if (!copy_to_user(info_user, &info, sizeof(info))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_TIMER_INFO: {
            vos_timer_info_user_t* info_user = (vos_timer_info_user_t*)frame->ebx;
            if (!info_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_timer_info_user_t info;
            info.ticks = timer_get_ticks();
            info.hz = timer_get_hz();
            info.uptime_ms = timer_uptime_ms();
            if (!copy_to_user(info_user, &info, sizeof(info))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_IRQ_STATS: {
            vos_irq_stats_user_t* stats_user = (vos_irq_stats_user_t*)frame->ebx;
            if (!stats_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_irq_stats_user_t stats;
            irq_get_counts(stats.counts);
            if (!copy_to_user(stats_user, &stats, sizeof(stats))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_SCHED_STATS: {
            vos_sched_stats_user_t* stats_user = (vos_sched_stats_user_t*)frame->ebx;
            if (!stats_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_sched_stats_user_t stats;
            stats.context_switches = tasking_context_switch_count();
            stats.task_count = tasking_task_count();
            tasking_get_state_counts(&stats.runnable, &stats.sleeping,
                                     &stats.waiting, &stats.zombie);
            if (!copy_to_user(stats_user, &stats, sizeof(stats))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_DESCRIPTOR_INFO: {
            vos_descriptor_info_user_t* info_user = (vos_descriptor_info_user_t*)frame->ebx;
            if (!info_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_descriptor_info_user_t info;
            gdt_get_info(&info.gdt_base, &info.gdt_entries);
            idt_get_info(&info.idt_base, &info.idt_entries);
            info.tss_esp0 = tss_get_esp0();
            if (!copy_to_user(info_user, &info, sizeof(info))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_SYSCALL_STATS: {
            vos_syscall_stats_user_t* stats_user = (vos_syscall_stats_user_t*)frame->ebx;
            if (!stats_user) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            vos_syscall_stats_user_t stats;
            memset(&stats, 0, sizeof(stats));
            stats.num_syscalls = SYS_MAX;
            for (uint32_t i = 0; i < SYS_MAX && i < VOS_SYSCALL_STATS_MAX; i++) {
                stats.counts[i] = syscall_counts[i];
                if (syscall_names[i]) {
                    strncpy(stats.names[i], syscall_names[i], 15);
                    stats.names[i][15] = '\0';
                } else {
                    stats.names[i][0] = '\0';
                }
            }
            if (!copy_to_user(stats_user, &stats, sizeof(stats))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_SELECT: {
            // select(nfds, readfds, writefds, exceptfds, timeout)
            int32_t nfds = (int32_t)frame->ebx;
            vos_fd_set_t* readfds_user = (vos_fd_set_t*)frame->ecx;
            vos_fd_set_t* writefds_user = (vos_fd_set_t*)frame->edx;
            vos_fd_set_t* exceptfds_user = (vos_fd_set_t*)frame->esi;
            vos_timeval_t* timeout_user = (vos_timeval_t*)frame->edi;

            if (nfds < 0 || nfds > VOS_FD_SETSIZE) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            // Copy fd_sets from user space
            vos_fd_set_t readfds = {0}, writefds = {0}, exceptfds = {0};
            if (readfds_user && !copy_from_user(&readfds, readfds_user, sizeof(readfds))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            if (writefds_user && !copy_from_user(&writefds, writefds_user, sizeof(writefds))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            if (exceptfds_user && !copy_from_user(&exceptfds, exceptfds_user, sizeof(exceptfds))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            // Get timeout (NULL = block forever, {0,0} = poll)
            int32_t timeout_ms = -1;  // -1 = infinite
            if (timeout_user) {
                vos_timeval_t tv;
                if (!copy_from_user(&tv, timeout_user, sizeof(tv))) {
                    frame->eax = (uint32_t)-EFAULT;
                    return frame;
                }
                timeout_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                if (timeout_ms < 0) timeout_ms = 0;
            }

            // Calculate deadline
            uint32_t start_tick = timer_get_ticks();
            uint32_t hz = timer_get_hz();
            uint32_t deadline = 0;
            if (timeout_ms > 0 && hz > 0) {
                uint32_t timeout_ticks = ((uint32_t)timeout_ms * hz + 999u) / 1000u;
                deadline = start_tick + timeout_ticks;
            }

            // Main select loop
            for (;;) {
                vos_fd_set_t out_read = {0}, out_write = {0}, out_except = {0};
                int32_t nready = 0;

                // Check each fd
                for (int32_t fd = 0; fd < nfds; fd++) {
                    if (readfds_user && fd_set_isset(&readfds, fd)) {
                        int32_t r = tasking_fd_is_readable(fd);
                        if (r == 1) {
                            fd_set_set(&out_read, fd);
                            nready++;
                        } else if (r < 0) {
                            frame->eax = (uint32_t)-EBADF;
                            return frame;
                        }
                    }
                    if (writefds_user && fd_set_isset(&writefds, fd)) {
                        int32_t w = tasking_fd_is_writable(fd);
                        if (w == 1) {
                            fd_set_set(&out_write, fd);
                            nready++;
                        } else if (w < 0) {
                            frame->eax = (uint32_t)-EBADF;
                            return frame;
                        }
                    }
                    // exceptfds not really supported, just clear
                }

                if (nready > 0 || timeout_ms == 0) {
                    // Copy results back
                    if (readfds_user && !copy_to_user(readfds_user, &out_read, sizeof(out_read))) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }
                    if (writefds_user && !copy_to_user(writefds_user, &out_write, sizeof(out_write))) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }
                    if (exceptfds_user && !copy_to_user(exceptfds_user, &out_except, sizeof(out_except))) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }
                    frame->eax = (uint32_t)nready;
                    return frame;
                }

                // Check for timeout
                if (timeout_ms > 0) {
                    uint32_t now = timer_get_ticks();
                    if (now >= deadline) {
                        // Timed out, return 0
                        if (readfds_user) copy_to_user(readfds_user, &out_read, sizeof(out_read));
                        if (writefds_user) copy_to_user(writefds_user, &out_write, sizeof(out_write));
                        if (exceptfds_user) copy_to_user(exceptfds_user, &out_except, sizeof(out_except));
                        frame->eax = 0;
                        return frame;
                    }
                }

                // Check for signals
                if (tasking_current_should_interrupt()) {
                    frame->eax = (uint32_t)-EINTR;
                    return frame;
                }

                // Wait a bit before checking again (yield to other tasks)
                __asm__ volatile ("sti; hlt; cli");
            }
        }

        // Color theme syscalls
        case SYS_THEME_COUNT: {
            frame->eax = (uint32_t)screen_theme_count();
            return frame;
        }
        case SYS_THEME_GET: {
            frame->eax = (uint32_t)screen_theme_get_current();
            return frame;
        }
        case SYS_THEME_INFO: {
            uint32_t idx = frame->ebx;
            char* name_user = (char*)frame->ecx;
            uint32_t name_cap = frame->edx;

            char name[64];
            int32_t rc = screen_theme_get_info((int)idx, name, sizeof(name));
            if (rc < 0) {
                frame->eax = (uint32_t)rc;
                return frame;
            }

            if (name_user && name_cap > 0) {
                rc = copy_kernel_string_to_user(name_user, name_cap, name);
                if (rc < 0) {
                    frame->eax = (uint32_t)rc;
                    return frame;
                }
            }
            frame->eax = 0;
            return frame;
        }
        case SYS_THEME_SET: {
            uint32_t idx = frame->ebx;
            int32_t rc = screen_theme_set((int)idx);
            frame->eax = (uint32_t)rc;
            return frame;
        }

        // gettimeofday(tv, tz) - tz is ignored
        case SYS_GETTIMEOFDAY: {
            vos_timeval_t* tv_user = (vos_timeval_t*)frame->ebx;
            if (!tv_user) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            rtc_datetime_t dt;
            if (!rtc_read_datetime(&dt)) {
                frame->eax = (uint32_t)-EIO;
                return frame;
            }

            // Convert to Unix timestamp (seconds since 1970-01-01)
            // Simplified calculation - days since epoch
            int32_t days = 0;
            for (uint16_t y = 1970; y < dt.year; y++) {
                days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
            }
            static const int32_t mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
            days += mdays[dt.month - 1];
            if (dt.month > 2 && (dt.year % 4 == 0 && (dt.year % 100 != 0 || dt.year % 400 == 0))) {
                days++;
            }
            days += dt.day - 1;

            int32_t secs = days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;

            // Get microseconds from uptime (approximate sub-second precision)
            uint32_t uptime_ms = timer_uptime_ms();
            int32_t usec = (uptime_ms % 1000) * 1000;

            vos_timeval_t tv;
            tv.tv_sec = secs;
            tv.tv_usec = usec;

            if (!copy_to_user(tv_user, &tv, sizeof(tv))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }

        // clock_gettime(clockid, tp)
        case SYS_CLOCK_GETTIME: {
            int32_t clockid = (int32_t)frame->ebx;
            vos_timespec_t* tp_user = (vos_timespec_t*)frame->ecx;

            if (!tp_user) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            vos_timespec_t ts;

            if (clockid == VOS_CLOCK_MONOTONIC) {
                // Monotonic clock: uptime since boot
                uint32_t uptime_ms = timer_uptime_ms();
                ts.tv_sec = (int32_t)(uptime_ms / 1000);
                ts.tv_nsec = (int32_t)((uptime_ms % 1000) * 1000000);
            } else if (clockid == VOS_CLOCK_REALTIME) {
                // Real time: wall clock
                rtc_datetime_t dt;
                if (!rtc_read_datetime(&dt)) {
                    frame->eax = (uint32_t)-EIO;
                    return frame;
                }

                // Same Unix timestamp calculation as gettimeofday
                int32_t days = 0;
                for (uint16_t y = 1970; y < dt.year; y++) {
                    days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
                }
                static const int32_t mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
                days += mdays[dt.month - 1];
                if (dt.month > 2 && (dt.year % 4 == 0 && (dt.year % 100 != 0 || dt.year % 400 == 0))) {
                    days++;
                }
                days += dt.day - 1;

                ts.tv_sec = days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
                uint32_t uptime_ms = timer_uptime_ms();
                ts.tv_nsec = (int32_t)((uptime_ms % 1000) * 1000000);
            } else {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            if (!copy_to_user(tp_user, &ts, sizeof(ts))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }

        // nanosleep(req, rem)
        case SYS_NANOSLEEP: {
            const vos_timespec_t* req_user = (const vos_timespec_t*)frame->ebx;
            vos_timespec_t* rem_user = (vos_timespec_t*)frame->ecx;

            if (!req_user) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            vos_timespec_t req;
            if (!copy_from_user(&req, req_user, sizeof(req))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            if (req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= 1000000000) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            // Convert to milliseconds
            uint32_t ms = (uint32_t)(req.tv_sec * 1000 + req.tv_nsec / 1000000);
            if (ms == 0 && (req.tv_sec > 0 || req.tv_nsec > 0)) {
                ms = 1;  // At least 1ms if any sleep was requested
            }

            if (ms == 0) {
                frame->eax = 0;
                return frame;
            }

            uint32_t hz = timer_get_hz();
            if (hz == 0) {
                frame->eax = (uint32_t)-EIO;
                return frame;
            }

            uint32_t ticks_to_wait = (ms * hz + 999u) / 1000u;
            if (ticks_to_wait == 0) {
                ticks_to_wait = 1;
            }
            uint32_t wake = timer_get_ticks() + ticks_to_wait;

            // If interrupted, write remaining time to rem
            if (rem_user) {
                vos_timespec_t rem = {0, 0};
                (void)copy_to_user(rem_user, &rem, sizeof(rem));
            }

            frame->eax = 0;
            return tasking_sleep_until(frame, wake);
        }

        // access(path, mode)
        case SYS_ACCESS: {
            const char* path_user = (const char*)frame->ebx;
            int32_t mode = (int32_t)frame->ecx;

            char path[256];
            if (!copy_user_cstring(path, sizeof(path), path_user)) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            int32_t rc = tasking_access(path, mode);
            frame->eax = (uint32_t)rc;
            return frame;
        }

        // isatty(fd)
        case SYS_ISATTY: {
            int32_t fd = (int32_t)frame->ebx;
            int32_t rc = tasking_fd_isatty(fd);
            frame->eax = (uint32_t)rc;
            return frame;
        }

        // uname(buf)
        case SYS_UNAME: {
            vos_utsname_t* buf_user = (vos_utsname_t*)frame->ebx;
            if (!buf_user) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            vos_utsname_t uts;
            memset(&uts, 0, sizeof(uts));
            strncpy(uts.sysname, "VOS", sizeof(uts.sysname) - 1);
            strncpy(uts.nodename, "vos", sizeof(uts.nodename) - 1);
            strncpy(uts.release, "1.0.0", sizeof(uts.release) - 1);
            strncpy(uts.version, "VOS 1.0.0", sizeof(uts.version) - 1);
            strncpy(uts.machine, "i686", sizeof(uts.machine) - 1);

            if (!copy_to_user(buf_user, &uts, sizeof(uts))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
            frame->eax = 0;
            return frame;
        }

        // poll(fds, nfds, timeout_ms)
        case SYS_POLL: {
            vos_pollfd_t* fds_user = (vos_pollfd_t*)frame->ebx;
            uint32_t nfds = frame->ecx;
            int32_t timeout_ms = (int32_t)frame->edx;

            if (nfds > VOS_FD_SETSIZE) {
                frame->eax = (uint32_t)-EINVAL;
                return frame;
            }

            if (nfds == 0) {
                // Just sleep if timeout > 0
                if (timeout_ms > 0) {
                    uint32_t hz = timer_get_hz();
                    if (hz > 0) {
                        uint32_t ticks = ((uint32_t)timeout_ms * hz + 999u) / 1000u;
                        uint32_t wake = timer_get_ticks() + ticks;
                        frame->eax = 0;
                        return tasking_sleep_until(frame, wake);
                    }
                }
                frame->eax = 0;
                return frame;
            }

            // Copy pollfds from user
            vos_pollfd_t fds[VOS_FD_SETSIZE];
            uint32_t copy_size = nfds * sizeof(vos_pollfd_t);
            if (!copy_from_user(fds, fds_user, copy_size)) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }

            // Calculate deadline
            uint32_t start_tick = timer_get_ticks();
            uint32_t hz = timer_get_hz();
            uint32_t deadline = 0;
            if (timeout_ms > 0 && hz > 0) {
                deadline = start_tick + ((uint32_t)timeout_ms * hz + 999u) / 1000u;
            }

            for (;;) {
                int32_t nready = 0;

                // Check each fd
                for (uint32_t i = 0; i < nfds; i++) {
                    fds[i].revents = 0;
                    int32_t fd = fds[i].fd;

                    if (fd < 0) {
                        continue;  // Negative fd is ignored
                    }

                    // Check if fd is valid
                    int32_t readable = tasking_fd_is_readable(fd);
                    int32_t writable = tasking_fd_is_writable(fd);

                    if (readable < 0 && writable < 0) {
                        fds[i].revents = VOS_POLLNVAL;
                        nready++;
                        continue;
                    }

                    if ((fds[i].events & VOS_POLLIN) && readable > 0) {
                        fds[i].revents |= VOS_POLLIN;
                        nready++;
                    }
                    if ((fds[i].events & VOS_POLLOUT) && writable > 0) {
                        fds[i].revents |= VOS_POLLOUT;
                        nready++;
                    }
                }

                if (nready > 0 || timeout_ms == 0) {
                    // Copy results back
                    if (!copy_to_user(fds_user, fds, copy_size)) {
                        frame->eax = (uint32_t)-EFAULT;
                        return frame;
                    }
                    frame->eax = (uint32_t)nready;
                    return frame;
                }

                // Check for timeout
                if (timeout_ms > 0) {
                    uint32_t now = timer_get_ticks();
                    if (now >= deadline) {
                        // Copy results back (all revents should be 0)
                        if (!copy_to_user(fds_user, fds, copy_size)) {
                            frame->eax = (uint32_t)-EFAULT;
                            return frame;
                        }
                        frame->eax = 0;
                        return frame;
                    }
                }

                // Check for signals
                if (tasking_current_should_interrupt()) {
                    frame->eax = (uint32_t)-EINTR;
                    return frame;
                }

                // Wait a bit before checking again
                __asm__ volatile ("sti; hlt; cli");
            }
        }

        default:
            frame->eax = (uint32_t)-1;
            return frame;
    }
}
