#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// VOS syscall numbers (kernel/syscall.c)
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
};

static inline int vos_sys_write(const void* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(buf), "c"(len)
        : "memory"
    );
    return ret;
}

__attribute__((noreturn)) static inline void vos_sys_exit(int code) {
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

static inline void* vos_sys_sbrk(int incr) {
    void* ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SBRK), "b"(incr)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_open(const char* path, int flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path), "c"(flags)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_read(int fd, void* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
    return ret;
}

int open(const char* name, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    (void)va_arg(ap, int);
    va_end(ap);

    int fd = vos_sys_open(name, flags);
    if (fd < 0) {
        errno = ENOENT;
        return -1;
    }
    return fd;
}

int close(int file) {
    int rc = vos_sys_close(file);
    if (rc < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int read(int file, void* ptr, size_t len) {
    if (len == 0) {
        return 0;
    }
    int n = vos_sys_read(file, ptr, (unsigned int)len);
    if (n < 0) {
        errno = EBADF;
        return -1;
    }
    return n;
}

int write(int file, const void* ptr, size_t len) {
    (void)file;
    if (len == 0) {
        return 0;
    }
    int n = vos_sys_write(ptr, (unsigned int)len);
    if (n < 0) {
        errno = EIO;
        return -1;
    }
    return n;
}

void* sbrk(ptrdiff_t incr) {
    void* p = vos_sys_sbrk((int)incr);
    if ((unsigned int)p == 0xFFFFFFFFu) {
        errno = ENOMEM;
        return (void*)-1;
    }
    return p;
}

int fstat(int file, struct stat* st) {
    (void)file;
    if (!st) {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int isatty(int file) {
    return (file >= 0 && file <= 2) ? 1 : 0;
}

off_t lseek(int file, off_t ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    errno = ESPIPE;
    return (off_t)-1;
}

int kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = ENOSYS;
    return -1;
}

int getpid(void) {
    return 1;
}

__attribute__((noreturn)) void _exit(int code) {
    vos_sys_exit(code);
}
