#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

typedef unsigned int uint32_t;

enum {
    SYS_WRITE = 0,
    SYS_EXIT = 1,
    SYS_YIELD = 2,
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

