#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Force "C"/ASCII collation for newlib's regex implementation.
int __collate_load_error = 1;

ssize_t getdelim(char** lineptr, size_t* n, int delim, FILE* stream) {
    return __getdelim(lineptr, n, delim, stream);
}

ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
    return __getline(lineptr, n, stream);
}

// Minimal POSIX basename/dirname for userland ports (e.g. sbase).
char* basename(char* path) {
    static char dot[] = ".";
    static char slash[] = "/";
    if (!path || !*path) {
        return dot;
    }

    char* end = path + strlen(path) - 1;
    while (end > path && *end == '/') {
        *end-- = '\0';
    }
    if (end == path && *end == '/') {
        return slash;
    }

    char* base = end;
    while (base > path && *(base - 1) != '/') {
        base--;
    }
    return base;
}

char* dirname(char* path) {
    static char dot[] = ".";
    static char slash[] = "/";
    if (!path || !*path) {
        return dot;
    }

    char* end = path + strlen(path) - 1;
    while (end > path && *end == '/') {
        *end-- = '\0';
    }
    if (end == path && *end == '/') {
        return slash;
    }

    char* last = strrchr(path, '/');
    if (!last) {
        return dot;
    }
    if (last == path) {
        return slash;
    }

    *last = '\0';
    while (last > path && *(last - 1) == '/') {
        *(last - 1) = '\0';
        last--;
    }
    if (!*path) {
        return slash;
    }
    return path;
}

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
    SYS_UPTIME_MS = 30,
    SYS_RTC_GET = 31,
};

typedef struct vos_stat {
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
    unsigned short wtime;
    unsigned short wdate;
} vos_stat_t;

typedef struct vos_rtc_datetime {
    unsigned short year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
} vos_rtc_datetime_t;

#define VOS_NAME_MAX 64
typedef struct vos_dirent {
    char name[VOS_NAME_MAX];
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
    unsigned short wtime;
    unsigned short wdate;
} vos_dirent_t;

static inline int vos_sys_sleep(unsigned int ms) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SLEEP), "b"(ms)
        : "memory"
    );
    return ret;
}

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

static inline int vos_sys_readdir(int fd, vos_dirent_t* ent) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READDIR), "b"(fd), "c"(ent)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_rtc_get(vos_rtc_datetime_t* dt) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RTC_GET), "b"(dt)
        : "memory"
    );
    return ret;
}

static inline unsigned int vos_sys_uptime_ms(void) {
    unsigned int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_UPTIME_MS)
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

static int is_leap(int year) {
    if ((year % 4) != 0) return 0;
    if ((year % 100) != 0) return 1;
    return (year % 400) == 0;
}

static time_t ymdhms_to_epoch(int year, int month, int day, int hour, int minute, int second) {
    if (year < 1970) {
        return (time_t)0;
    }
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    if (day < 1) day = 1;
    if (day > 31) day = 31;
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;
    if (second < 0) second = 0;
    if (second > 59) second = 59;

    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long days = 0;
    for (int y = 1970; y < year; y++) {
        days += is_leap(y) ? 366 : 365;
    }
    for (int m = 1; m < month; m++) {
        days += mdays[m - 1];
        if (m == 2 && is_leap(year)) {
            days += 1;
        }
    }
    days += (day - 1);

    long sec = days * 86400L + hour * 3600L + minute * 60L + second;
    return (time_t)sec;
}

static time_t fat_ts_to_epoch(unsigned short wdate, unsigned short wtime) {
    if (wdate == 0) {
        return (time_t)0;
    }
    int year = 1980 + ((wdate >> 9) & 0x7F);
    int month = (wdate >> 5) & 0x0F;
    int day = wdate & 0x1F;
    int hour = (wtime >> 11) & 0x1F;
    int minute = (wtime >> 5) & 0x3F;
    int second = (wtime & 0x1F) * 2;
    return ymdhms_to_epoch(year, month, day, hour, minute, second);
}

static void fill_stat_common(struct stat* st, const vos_stat_t* vst) {
    if (!st || !vst) {
        return;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = vst->is_dir ? S_IFDIR : S_IFREG;
    st->st_nlink = 1;
    st->st_size = (off_t)vst->size;
    st->st_blksize = 512;
    st->st_blocks = (blkcnt_t)((vst->size + 511u) / 512u);

    time_t t = fat_ts_to_epoch(vst->wdate, vst->wtime);
    st->st_mtim.tv_sec = t;
    st->st_mtim.tv_nsec = 0;
    st->st_atim = st->st_mtim;
    st->st_ctim = st->st_mtim;
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
            memset(st, 0, sizeof(*st));
            st->st_mode = S_IFCHR;
            st->st_nlink = 1;
            st->st_blksize = 512;
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

    fill_stat_common(st, &vst);
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

    fill_stat_common(st, &vst);
    return 0;
}

int lstat(const char* path, struct stat* st) {
    // VOS currently doesn't support symlinks, so lstat == stat.
    return stat(path, st);
}

unsigned int sleep(unsigned int seconds) {
    (void)vos_sys_sleep(seconds * 1000u);
    return 0;
}

int usleep(useconds_t usec) {
    (void)vos_sys_sleep((unsigned int)((usec + 999u) / 1000u));
    return 0;
}

int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    if (!tv) {
        errno = EINVAL;
        return -1;
    }

    vos_rtc_datetime_t dt;
    int rc = vos_sys_rtc_get(&dt);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    tv->tv_sec = ymdhms_to_epoch((int)dt.year, (int)dt.month, (int)dt.day, (int)dt.hour, (int)dt.minute, (int)dt.second);
    tv->tv_usec = 0;
    return 0;
}

time_t time(time_t* tloc) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        if (tloc) *tloc = (time_t)-1;
        return (time_t)-1;
    }
    if (tloc) *tloc = tv.tv_sec;
    return tv.tv_sec;
}

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
    if (!tp) {
        errno = EINVAL;
        return -1;
    }

#ifdef CLOCK_MONOTONIC
    if (clock_id == CLOCK_MONOTONIC) {
        unsigned int ms = vos_sys_uptime_ms();
        tp->tv_sec = (time_t)(ms / 1000u);
        tp->tv_nsec = (long)((ms % 1000u) * 1000000u);
        return 0;
    }
#endif

    if (clock_id == CLOCK_REALTIME) {
        struct timeval tv;
        if (gettimeofday(&tv, NULL) < 0) {
            return -1;
        }
        tp->tv_sec = tv.tv_sec;
        tp->tv_nsec = (long)tv.tv_usec * 1000L;
        return 0;
    }

    errno = EINVAL;
    return -1;
}

DIR* opendir(const char* name) {
    if (!name) {
        errno = EINVAL;
        return NULL;
    }
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return NULL;
    }
    DIR* d = (DIR*)malloc(sizeof(DIR));
    if (!d) {
        int saved = errno;
        close(fd);
        errno = saved ? saved : ENOMEM;
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->fd = fd;
    d->eof = 0;
    return d;
}

DIR* fdopendir(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return NULL;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    DIR* d = (DIR*)malloc(sizeof(DIR));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->fd = fd;
    d->eof = 0;
    return d;
}

int closedir(DIR* dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }
    int fd = dirp->fd;
    free(dirp);
    if (fd >= 0) {
        return close(fd);
    }
    return 0;
}

struct dirent* readdir(DIR* dirp) {
    if (!dirp) {
        errno = EINVAL;
        return NULL;
    }
    if (dirp->eof) {
        return NULL;
    }

    vos_dirent_t de;
    int rc = vos_sys_readdir(dirp->fd, &de);
    if (rc < 0) {
        errno = -rc;
        return NULL;
    }
    if (rc == 0) {
        dirp->eof = 1;
        return NULL;
    }

    memset(&dirp->de, 0, sizeof(dirp->de));
    dirp->de.d_ino = 0;
    dirp->de.d_reclen = (unsigned short)sizeof(struct dirent);
    dirp->de.d_type = de.is_dir ? DT_DIR : DT_REG;
    strncpy(dirp->de.d_name, de.name, sizeof(dirp->de.d_name) - 1u);
    dirp->de.d_name[sizeof(dirp->de.d_name) - 1u] = '\0';
    return &dirp->de;
}

void rewinddir(DIR* dirp) {
    // Our VFS doesn't support seekable directory streams yet.
    // Best-effort: just mark "not EOF" and rely on reopening if needed.
    if (!dirp) {
        return;
    }
    dirp->eof = 0;
}

int dirfd(DIR* dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }
    return dirp->fd;
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
