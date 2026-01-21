#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
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

static inline int vos_sys_ioctl(int fd, unsigned int req, void* argp) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IOCTL), "b"(fd), "c"(req), "d"(argp)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_unlink(const char* path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_UNLINK), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_rename(const char* oldp, const char* newp) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RENAME), "b"(oldp), "c"(newp)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_rmdir(const char* path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RMDIR), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_truncate(const char* path, unsigned int size) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_TRUNCATE), "b"(path), "c"(size)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_ftruncate(int fd, unsigned int size) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FTRUNCATE), "b"(fd), "c"(size)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_fsync(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FSYNC), "b"(fd)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_dup(int oldfd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DUP), "b"(oldfd)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_pipe(int* fds) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PIPE), "b"(fds)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_getpid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_kill(int pid, int sig) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_KILL), "b"(pid), "c"(sig)
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

    // Prefer treating actual TTY fds as character devices, but allow stdin/out/err
    // to be redirected via dup2/open.
    if (file >= 0) {
        int saved = errno;
        struct termios t;
        if (ioctl(file, TCGETS, &t) == 0) {
            errno = saved;
            st->st_mode = S_IFCHR;
            st->st_size = 0;
            return 0;
        }
        errno = saved;
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
    if (file < 0) {
        return 0;
    }
    int saved = errno;
    struct termios t;
    int rc = ioctl(file, TCGETS, &t);
    errno = saved;
    return (rc == 0) ? 1 : 0;
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

int unlink(const char* path) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_unlink(path);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int rmdir(const char* path) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_rmdir(path);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int rename(const char* oldp, const char* newp) {
    if (!oldp || !newp) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_rename(oldp, newp);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int truncate(const char* path, off_t length) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    if (length < 0 || (unsigned long)length > 0xFFFFFFFFul) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_truncate(path, (unsigned int)length);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int ftruncate(int fd, off_t length) {
    if (length < 0 || (unsigned long)length > 0xFFFFFFFFul) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_ftruncate(fd, (unsigned int)length);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int fsync(int fd) {
    int rc = vos_sys_fsync(fd);
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

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void* argp = va_arg(ap, void*);
    va_end(ap);

    int rc = vos_sys_ioctl(fd, (unsigned int)request, argp);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int dup(int oldfd) {
    int rc = vos_sys_dup(oldfd);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int dup2(int oldfd, int newfd) {
    int rc = vos_sys_dup2(oldfd, newfd);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int pipe(int fds[2]) {
    if (!fds) {
        errno = EINVAL;
        return -1;
    }
    int rc = vos_sys_pipe(fds);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int tcgetattr(int fd, struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }

    unsigned long req = TCSETS;
    if (optional_actions == TCSADRAIN) {
        req = TCSETSW;
    } else if (optional_actions == TCSAFLUSH) {
        req = TCSETSF;
    }

    return ioctl(fd, req, termios_p);
}

void cfmakeraw(struct termios* t) {
    if (!t) {
        return;
    }

    t->c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t->c_oflag &= (tcflag_t) ~(OPOST);
    t->c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    t->c_cflag |= (tcflag_t)CS8;

    if (VMIN >= 0 && VMIN < NCCS) {
        t->c_cc[VMIN] = 1;
    }
    if (VTIME >= 0 && VTIME < NCCS) {
        t->c_cc[VTIME] = 0;
    }
}

int kill(int pid, int sig) {
    int rc = vos_sys_kill(pid, sig);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int getpid(void) {
    return vos_sys_getpid();
}

mode_t umask(mode_t mask) {
    (void)mask;
    return 0;
}

int fchmod(int fd, mode_t mode) {
    (void)fd;
    (void)mode;
    errno = ENOSYS;
    return -1;
}

__attribute__((noreturn)) void _exit(int code) {
    vos_sys_exit(code);
}
