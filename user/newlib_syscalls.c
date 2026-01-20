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
    SYS_LSEEK = 11,
    SYS_FSTAT = 12,
    SYS_STAT = 13,
    SYS_MKDIR = 14,
    SYS_READDIR = 15,
    SYS_CHDIR = 16,
    SYS_GETCWD = 17,
};

typedef struct vos_stat {
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
} vos_stat_t;

#define VOS_NAME_MAX 64
typedef struct vos_dirent {
    char name[VOS_NAME_MAX];
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
} vos_dirent_t;

static inline int vos_sys_write(int fd, const void* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
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

static inline int vos_sys_lseek(int fd, int offset, int whence) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_LSEEK), "b"(fd), "c"(offset), "d"(whence)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_fstat(int fd, vos_stat_t* st) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FSTAT), "b"(fd), "c"(st)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_stat(const char* path, vos_stat_t* st) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_STAT), "b"(path), "c"(st)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_mkdir(const char* path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MKDIR), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_chdir(const char* path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CHDIR), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_getcwd(char* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETCWD), "b"(buf), "c"(len)
        : "memory"
    );
    return ret;
}

int open(const char* name, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    (void)va_arg(ap, int);
    va_end(ap);

    int rc = vos_sys_open(name, flags);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int close(int file) {
    int rc = vos_sys_close(file);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int read(int file, void* ptr, size_t len) {
    if (len == 0) {
        return 0;
    }
    int n = vos_sys_read(file, ptr, (unsigned int)len);
    if (n < 0) {
        errno = -n;
        return -1;
    }
    return n;
}

int write(int file, const void* ptr, size_t len) {
    if (len == 0) {
        return 0;
    }
    int n = vos_sys_write(file, ptr, (unsigned int)len);
    if (n < 0) {
        errno = -n;
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
    if (!st) {
        errno = EINVAL;
        return -1;
    }

    if (file >= 0 && file <= 2) {
        st->st_mode = S_IFCHR;
        st->st_size = 0;
        return 0;
    }

    vos_stat_t vst;
    int rc = vos_sys_fstat(file, &vst);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    st->st_mode = vst.is_dir ? S_IFDIR : S_IFREG;
    st->st_size = (off_t)vst.size;
    return 0;
}

int isatty(int file) {
    return (file >= 0 && file <= 2) ? 1 : 0;
}

off_t lseek(int file, off_t ptr, int dir) {
    int rc = vos_sys_lseek(file, (int)ptr, dir);
    if (rc < 0) {
        errno = -rc;
        return (off_t)-1;
    }
    return (off_t)rc;
}

int stat(const char* path, struct stat* st) {
    if (!path || !st) {
        errno = EINVAL;
        return -1;
    }

    vos_stat_t vst;
    int rc = vos_sys_stat(path, &vst);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    st->st_mode = vst.is_dir ? S_IFDIR : S_IFREG;
    st->st_size = (off_t)vst.size;
    return 0;
}

int mkdir(const char* path, mode_t mode) {
    (void)mode;
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_mkdir(path);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int chdir(const char* path) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_chdir(path);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

char* getcwd(char* buf, size_t size) {
    if (!buf || size == 0) {
        errno = EINVAL;
        return NULL;
    }
    int rc = vos_sys_getcwd(buf, (unsigned int)size);
    if (rc < 0) {
        errno = -rc;
        return NULL;
    }
    return buf;
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
