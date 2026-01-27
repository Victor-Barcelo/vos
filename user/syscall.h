#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

typedef struct vos_rtc_datetime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} vos_rtc_datetime_t;

typedef struct vos_task_info {
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
} vos_task_info_t;

typedef struct vos_font_info {
    char name[32];
    uint32_t width;
    uint32_t height;
} vos_font_info_t;

typedef struct vos_statfs {
    uint32_t bsize;
    uint32_t blocks;
    uint32_t bfree;
    uint32_t bavail;
} vos_statfs_t;

// Sysview introspection structures
typedef struct vos_pmm_info {
    uint32_t total_frames;
    uint32_t free_frames;
    uint32_t page_size;
} vos_pmm_info_t;

typedef struct vos_heap_info {
    uint32_t heap_base;
    uint32_t heap_end;
    uint32_t total_free_bytes;
    uint32_t free_block_count;
} vos_heap_info_t;

typedef struct vos_timer_info {
    uint32_t ticks;
    uint32_t hz;
    uint32_t uptime_ms;
} vos_timer_info_t;

typedef struct vos_irq_stats {
    uint32_t counts[16];
} vos_irq_stats_t;

typedef struct vos_sched_stats {
    uint32_t context_switches;
    uint32_t task_count;
    uint32_t runnable;
    uint32_t sleeping;
    uint32_t waiting;
    uint32_t zombie;
} vos_sched_stats_t;

typedef struct vos_descriptor_info {
    uint32_t gdt_base;
    uint32_t gdt_entries;
    uint32_t idt_base;
    uint32_t idt_entries;
    uint32_t tss_esp0;
} vos_descriptor_info_t;

#define VOS_SYSCALL_STATS_MAX 80
typedef struct vos_syscall_stats {
    uint32_t num_syscalls;                  // Total number of syscalls supported
    uint32_t counts[VOS_SYSCALL_STATS_MAX]; // Count for each syscall
    char names[VOS_SYSCALL_STATS_MAX][16];  // Name of each syscall
} vos_syscall_stats_t;

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
    SYS_BEEP = 91,
    SYS_AUDIO_OPEN = 92,
    SYS_AUDIO_WRITE = 93,
    SYS_AUDIO_CLOSE = 94,
    SYS_CHOWN = 95,
    SYS_FCHOWN = 96,
    SYS_LCHOWN = 97,
};

// For select() syscall
typedef struct vos_timeval {
    int32_t tv_sec;
    int32_t tv_usec;
} vos_timeval_t;

#define VOS_FD_SETSIZE 64
typedef struct vos_fd_set {
    uint32_t bits[VOS_FD_SETSIZE / 32];
} vos_fd_set_t;

#define VOS_FD_ZERO(set)    do { (set)->bits[0] = 0; (set)->bits[1] = 0; } while(0)
#define VOS_FD_SET(fd, set) do { if ((fd) >= 0 && (fd) < VOS_FD_SETSIZE) (set)->bits[(fd)/32] |= (1u << ((fd)%32)); } while(0)
#define VOS_FD_CLR(fd, set) do { if ((fd) >= 0 && (fd) < VOS_FD_SETSIZE) (set)->bits[(fd)/32] &= ~(1u << ((fd)%32)); } while(0)
#define VOS_FD_ISSET(fd, set) (((fd) >= 0 && (fd) < VOS_FD_SETSIZE) ? ((set)->bits[(fd)/32] & (1u << ((fd)%32))) != 0 : 0)

// For clock_gettime / nanosleep
typedef struct vos_timespec {
    int32_t tv_sec;
    int32_t tv_nsec;
} vos_timespec_t;

// Clock IDs for clock_gettime
#define VOS_CLOCK_REALTIME  0
#define VOS_CLOCK_MONOTONIC 1

// For access() syscall
#define VOS_F_OK 0
#define VOS_R_OK 4
#define VOS_W_OK 2
#define VOS_X_OK 1

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

static inline int sys_write(int fd, const char* buf, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static inline void sys_yield(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_YIELD)
        : "memory"
    );
}

static inline int sys_sleep(uint32_t ms) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SLEEP), "b"(ms)
        : "memory"
    );
    return ret;
}

static inline int sys_wait(uint32_t pid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAIT), "b"(pid)
        : "memory"
    );
    return ret;
}

static inline int sys_waitpid(int pid, int* status, int options) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAITPID), "b"(pid), "c"(status), "d"(options)
        : "memory"
    );
    return ret;
}

static inline int sys_statfs(const char* path, vos_statfs_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_STATFS), "b"(path), "c"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_kill(int pid, int code) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_KILL), "b"(pid), "c"(code)
        : "memory"
    );
    return ret;
}

static inline int sys_getppid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPPID)
        : "memory"
    );
    return ret;
}

static inline int sys_getpgrp(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPGRP)
        : "memory"
    );
    return ret;
}

static inline int sys_setpgid(int pid, int pgid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETPGID), "b"(pid), "c"(pgid)
        : "memory"
    );
    return ret;
}

static inline void* sys_sbrk(int32_t increment) {
    void* ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SBRK), "b"(increment)
        : "memory"
    );
    return ret;
}

static inline int sys_readfile(const char* path, void* buf, uint32_t buf_len, uint32_t offset) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READFILE), "b"(path), "c"(buf), "d"(buf_len), "S"(offset)
        : "memory"
    );
    return ret;
}

static inline int sys_open(const char* path, uint32_t flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path), "c"(flags)
        : "memory"
    );
    return ret;
}

static inline int sys_read(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
    return ret;
}

__attribute__((noreturn)) static inline void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(code)
        : "memory"
    );
    for (;;) {
        __asm__ volatile ("pause");
    }
}

static inline int sys_fork(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FORK)
        : "memory"
    );
    return ret;
}

static inline int sys_execve(const char* path, const char* const* argv, uint32_t argc) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_EXECVE), "b"(path), "c"(argv), "d"(argc)
        : "memory"
    );
    return ret;
}

static inline int sys_spawn(const char* path, const char* const* argv, uint32_t argc) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SPAWN), "b"(path), "c"(argv), "d"(argc)
        : "memory"
    );
    return ret;
}

static inline uint32_t sys_uptime_ms(void) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_UPTIME_MS)
        : "memory"
    );
    return ret;
}

static inline int sys_rtc_get(vos_rtc_datetime_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RTC_GET), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_rtc_set(const vos_rtc_datetime_t* dt) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RTC_SET), "b"(dt)
        : "memory"
    );
    return ret;
}

static inline int sys_task_count(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_TASK_COUNT)
        : "memory"
    );
    return ret;
}

static inline int sys_task_info(uint32_t index, vos_task_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_TASK_INFO), "b"(index), "c"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_screen_is_fb(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SCREEN_IS_FB)
        : "memory"
    );
    return ret;
}

static inline int sys_gfx_clear(uint32_t bg) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GFX_CLEAR), "b"(bg)
        : "memory"
    );
    return ret;
}

static inline int sys_gfx_pset(int32_t x, int32_t y, uint32_t color) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GFX_PSET), "b"(x), "c"(y), "d"(color)
        : "memory"
    );
    return ret;
}

static inline int sys_gfx_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GFX_LINE), "b"(x0), "c"(y0), "d"(x1), "S"(y1), "D"(color)
        : "memory"
    );
    return ret;
}

static inline int sys_gfx_blit_rgba(int32_t x, int32_t y, uint32_t w, uint32_t h, const void* rgba) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GFX_BLIT_RGBA), "b"(x), "c"(y), "d"(w), "S"(h), "D"(rgba)
        : "memory"
    );
    return ret;
}

static inline uint32_t sys_mem_total_kb(void) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MEM_TOTAL_KB)
        : "memory"
    );
    return ret;
}

static inline int sys_cpu_vendor(char* buf, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CPU_VENDOR), "b"(buf), "c"(len)
        : "memory"
    );
    return ret;
}

static inline int sys_cpu_brand(char* buf, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CPU_BRAND), "b"(buf), "c"(len)
        : "memory"
    );
    return ret;
}

static inline int sys_vfs_file_count(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_VFS_FILE_COUNT)
        : "memory"
    );
    return ret;
}

static inline int sys_font_count(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_COUNT)
        : "memory"
    );
    return ret;
}

static inline int sys_font_get_current(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_GET)
        : "memory"
    );
    return ret;
}

static inline int sys_font_info(uint32_t index, vos_font_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_INFO), "b"(index), "c"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_font_set(uint32_t index) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_SET), "b"(index)
        : "memory"
    );
    return ret;
}

static inline void* sys_mmap(void* addr, uint32_t length, uint32_t prot, uint32_t flags, int32_t fd, uint32_t offset) {
    (void)offset; // file-backed mappings aren't supported yet
    void* ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MMAP), "b"(addr), "c"(length), "d"(prot), "S"(flags), "D"(fd)
        : "memory"
    );
    return ret;
}

static inline int sys_munmap(void* addr, uint32_t length) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MUNMAP), "b"(addr), "c"(length)
        : "memory"
    );
    return ret;
}

static inline int sys_mprotect(void* addr, uint32_t length, uint32_t prot) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MPROTECT), "b"(addr), "c"(length), "d"(prot)
        : "memory"
    );
    return ret;
}

static inline uint32_t sys_getuid(void) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETUID)
        : "memory"
    );
    return ret;
}

static inline uint32_t sys_getgid(void) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETGID)
        : "memory"
    );
    return ret;
}

static inline int sys_setuid(uint32_t uid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETUID), "b"(uid)
        : "memory"
    );
    return ret;
}

static inline int sys_setgid(uint32_t gid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETGID), "b"(gid)
        : "memory"
    );
    return ret;
}

// Sysview introspection syscalls
static inline int sys_pmm_info(vos_pmm_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PMM_INFO), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_heap_info(vos_heap_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_HEAP_INFO), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_timer_info(vos_timer_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_TIMER_INFO), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_irq_stats(vos_irq_stats_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IRQ_STATS), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_sched_stats(vos_sched_stats_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SCHED_STATS), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_descriptor_info(vos_descriptor_info_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DESCRIPTOR_INFO), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_syscall_stats(vos_syscall_stats_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SYSCALL_STATS), "b"(out)
        : "memory"
    );
    return ret;
}

static inline int sys_select(int nfds, vos_fd_set_t* readfds, vos_fd_set_t* writefds,
                             vos_fd_set_t* exceptfds, vos_timeval_t* timeout) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SELECT), "b"(nfds), "c"(readfds), "d"(writefds), "S"(exceptfds), "D"(timeout)
        : "memory"
    );
    return ret;
}

// Color theme syscalls
static inline int sys_theme_count(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_THEME_COUNT)
        : "memory"
    );
    return ret;
}

static inline int sys_theme_get_current(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_THEME_GET)
        : "memory"
    );
    return ret;
}

static inline int sys_theme_info(uint32_t index, char* name, uint32_t name_cap) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_THEME_INFO), "b"(index), "c"(name), "d"(name_cap)
        : "memory"
    );
    return ret;
}

static inline int sys_theme_set(uint32_t index) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_THEME_SET), "b"(index)
        : "memory"
    );
    return ret;
}

// New POSIX-like syscalls

static inline int sys_gettimeofday(vos_timeval_t* tv, void* tz) {
    (void)tz;  // timezone ignored
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETTIMEOFDAY), "b"(tv)
        : "memory"
    );
    return ret;
}

static inline int sys_clock_gettime(int clockid, vos_timespec_t* tp) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOCK_GETTIME), "b"(clockid), "c"(tp)
        : "memory"
    );
    return ret;
}

static inline int sys_nanosleep(const vos_timespec_t* req, vos_timespec_t* rem) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_NANOSLEEP), "b"(req), "c"(rem)
        : "memory"
    );
    return ret;
}

static inline int sys_access(const char* path, int mode) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_ACCESS), "b"(path), "c"(mode)
        : "memory"
    );
    return ret;
}

static inline int sys_isatty(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_ISATTY), "b"(fd)
        : "memory"
    );
    return ret;
}

static inline int sys_uname(vos_utsname_t* buf) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_UNAME), "b"(buf)
        : "memory"
    );
    return ret;
}

static inline int sys_poll(vos_pollfd_t* fds, uint32_t nfds, int timeout_ms) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_POLL), "b"(fds), "c"(nfds), "d"(timeout_ms)
        : "memory"
    );
    return ret;
}

// PC speaker beep syscall
static inline int sys_beep(uint32_t frequency, uint32_t duration_ms) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_BEEP), "b"(frequency), "c"(duration_ms)
        : "memory"
    );
    return ret;
}

// Audio syscalls (Sound Blaster 16)

// Open audio device and set format
// Returns handle (>0) on success, negative on error
static inline int sys_audio_open(uint32_t sample_rate, uint8_t bits, uint8_t channels) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_AUDIO_OPEN), "b"(sample_rate), "c"(bits), "d"(channels)
        : "memory"
    );
    return ret;
}

// Write PCM samples to audio device (blocking)
// Returns number of bytes written, or negative on error
static inline int sys_audio_write(int handle, const void* samples, uint32_t bytes) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_AUDIO_WRITE), "b"(handle), "c"(samples), "d"(bytes)
        : "memory"
    );
    return ret;
}

// Close audio device
static inline int sys_audio_close(int handle) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_AUDIO_CLOSE), "b"(handle)
        : "memory"
    );
    return ret;
}

#endif
