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
};

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

static inline int sys_kill(uint32_t pid, int code) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_KILL), "b"(pid), "c"(code)
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

#endif
