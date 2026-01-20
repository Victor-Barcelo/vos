#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

typedef unsigned int uint32_t;
typedef int int32_t;

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

static inline int sys_write(const char* buf, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(buf), "c"(len)
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

#endif
