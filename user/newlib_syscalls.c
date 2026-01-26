#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <dirent.h>
#include <signal.h>
#include <reent.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// poll.h is not in newlib, define here
#ifndef POLLIN
#define POLLIN   0x001
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020
#endif

#ifndef _POLL_H
struct pollfd {
    int fd;
    short events;
    short revents;
};
typedef unsigned int nfds_t;
#endif

// Force "C"/ASCII collation for newlib's regex implementation.
int __collate_load_error = 1;

ssize_t getdelim(char** lineptr, size_t* n, int delim, FILE* stream) {
    return __getdelim(lineptr, n, delim, stream);
}

ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
    return __getline(lineptr, n, stream);
}

// ---------------------------------------------------------------------------
// Minimal POSIX compatibility helpers (for ports like sbase)
// ---------------------------------------------------------------------------

#ifndef VOS_PATH_MAX
#define VOS_PATH_MAX 256
#endif

#ifndef VOS_MAX_TRACK_FDS
#define VOS_MAX_TRACK_FDS 64
#endif

#ifndef VOS_EXEC_MAX_ARGS
#define VOS_EXEC_MAX_ARGS 4096u
#endif

// Keep these in sync with the kernel execve/spawn marshalling limits.
#ifndef VOS_EXEC_ARG_MAXBYTES
#define VOS_EXEC_ARG_MAXBYTES (128u * 1024u)
#endif

long sysconf(int name) {
#ifdef _SC_ARG_MAX
    if (name == _SC_ARG_MAX) {
        return (long)VOS_EXEC_ARG_MAXBYTES;
    }
#endif
#ifdef _SC_OPEN_MAX
    if (name == _SC_OPEN_MAX) {
        return 64;
    }
#endif
#ifdef _SC_PAGESIZE
    if (name == _SC_PAGESIZE) {
        return 4096;
    }
#endif
#ifdef _SC_PAGE_SIZE
    if (name == _SC_PAGE_SIZE) {
        return 4096;
    }
#endif
#ifdef _SC_CLK_TCK
    if (name == _SC_CLK_TCK) {
        return 100;
    }
#endif

    errno = EINVAL;
    return -1;
}

static char g_fd_paths[VOS_MAX_TRACK_FDS][VOS_PATH_MAX];
static unsigned char g_fd_path_valid[VOS_MAX_TRACK_FDS];

static void fd_path_clear(int fd) {
    if (fd < 0 || fd >= (int)VOS_MAX_TRACK_FDS) {
        return;
    }
    g_fd_path_valid[fd] = 0;
    g_fd_paths[fd][0] = '\0';
}

static void fd_path_set(int fd, const char* abs_path) {
    if (fd < 0 || fd >= (int)VOS_MAX_TRACK_FDS) {
        return;
    }
    if (!abs_path) {
        fd_path_clear(fd);
        return;
    }
    strncpy(g_fd_paths[fd], abs_path, sizeof(g_fd_paths[fd]) - 1u);
    g_fd_paths[fd][sizeof(g_fd_paths[fd]) - 1u] = '\0';
    g_fd_path_valid[fd] = 1;
}

static const char* fd_path_get(int fd) {
    if (fd < 0 || fd >= (int)VOS_MAX_TRACK_FDS) {
        return NULL;
    }
    if (!g_fd_path_valid[fd]) {
        return NULL;
    }
    if (g_fd_paths[fd][0] == '\0') {
        return NULL;
    }
    return g_fd_paths[fd];
}

static void fd_path_copy(int newfd, int oldfd) {
    if (newfd < 0 || newfd >= (int)VOS_MAX_TRACK_FDS) {
        return;
    }
    const char* p = fd_path_get(oldfd);
    if (!p) {
        fd_path_clear(newfd);
        return;
    }
    fd_path_set(newfd, p);
}

static int path_is_abs(const char* path) {
    return path && path[0] == '/';
}

static int path_join(char out[VOS_PATH_MAX], const char* base, const char* rel) {
    if (!out || !base || !rel) {
        errno = EINVAL;
        return -1;
    }
    if (rel[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (rel[0] == '/') {
        if (strlen(rel) >= VOS_PATH_MAX) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(out, rel, VOS_PATH_MAX - 1u);
        out[VOS_PATH_MAX - 1u] = '\0';
        return 0;
    }

    size_t blen = strlen(base);
    size_t rlen = strlen(rel);
    size_t need = blen + (blen && base[blen - 1u] != '/' ? 1u : 0u) + rlen + 1u;
    if (need > VOS_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    strncpy(out, base, VOS_PATH_MAX - 1u);
    out[VOS_PATH_MAX - 1u] = '\0';
    if (blen && out[blen - 1u] != '/') {
        out[blen] = '/';
        out[blen + 1u] = '\0';
    }
    strncat(out, rel, VOS_PATH_MAX - 1u - strlen(out));
    return 0;
}

static int path_make_abs(char out[VOS_PATH_MAX], const char* path) {
    if (!out || !path) {
        errno = EINVAL;
        return -1;
    }
    if (path_is_abs(path)) {
        if (strlen(path) >= VOS_PATH_MAX) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(out, path, VOS_PATH_MAX - 1u);
        out[VOS_PATH_MAX - 1u] = '\0';
        return 0;
    }

    char cwd[VOS_PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }
    return path_join(out, cwd, path);
}

static dev_t dev_from_path(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return 0;
    }
    if (!strncmp(abs_path, "/disk", 5) || !strncmp(abs_path, "/usr", 4) || !strncmp(abs_path, "/etc", 4) ||
        !strncmp(abs_path, "/home", 5) || !strncmp(abs_path, "/var", 4)) {
        return 1;
    }
    if (!strncmp(abs_path, "/ram", 4) || !strncmp(abs_path, "/tmp", 4)) {
        return 2;
    }
    return 0;
}

static ino_t ino_from_path(const char* abs_path) {
    if (!abs_path) {
        return 0;
    }
    uint32_t h = 2166136261u; // FNV-1a 32-bit offset
    for (const unsigned char* p = (const unsigned char*)abs_path; *p; p++) {
        h ^= (uint32_t)(*p);
        h *= 16777619u;
    }
    if (h == 0) {
        h = 1;
    }
    return (ino_t)h;
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

int access(const char* path, int mode) {
    (void)mode; // VOS currently has no per-file permission enforcement.

    if (!path || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    struct stat st;
    if (stat(path, &st) == 0) {
        return 0;
    }
    return -1;
}

// Minimal fnmatch() implementation: supports '*', '?', and character classes.
static int fnmatch_range_match(const char** pat, char c) {
    const char* p = *pat;
    int negate = 0;
    if (*p == '!' || *p == '^') {
        negate = 1;
        p++;
    }

    int ok = 0;
    for (; *p && *p != ']'; p++) {
        if (p[1] == '-' && p[2] && p[2] != ']') {
            char a = p[0];
            char b = p[2];
            if (a > b) {
                char tmp = a;
                a = b;
                b = tmp;
            }
            if (c >= a && c <= b) {
                ok = 1;
            }
            p += 2;
            continue;
        }
        if (*p == c) {
            ok = 1;
        }
    }

    // Move past closing bracket if present.
    while (*p && *p != ']') p++;
    if (*p == ']') p++;
    *pat = p;

    return negate ? !ok : ok;
}

int fnmatch(const char* pattern, const char* string, int flags) {
    if (!pattern || !string) {
        return FNM_NOMATCH;
    }

    const char* p = pattern;
    const char* s = string;

    while (*p) {
        if (*p == '*') {
            while (*p == '*') p++;
            if (*p == '\0') {
                return 0;
            }
            for (const char* t = s; *t; t++) {
                if ((flags & FNM_PATHNAME) && *t == '/') {
                    break;
                }
                if (fnmatch(p, t, flags) == 0) {
                    return 0;
                }
            }
            return FNM_NOMATCH;
        }

        if (*s == '\0') {
            return FNM_NOMATCH;
        }

        if (*p == '?') {
            if ((flags & FNM_PATHNAME) && *s == '/') {
                return FNM_NOMATCH;
            }
            if ((flags & FNM_PERIOD) && *s == '.' && (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                return FNM_NOMATCH;
            }
            p++;
            s++;
            continue;
        }

        if (*p == '[') {
            p++;
            if ((flags & FNM_PATHNAME) && *s == '/') {
                return FNM_NOMATCH;
            }
            if (!fnmatch_range_match(&p, *s)) {
                return FNM_NOMATCH;
            }
            s++;
            continue;
        }

        if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1]) {
            p++;
        }

        if (*p != *s) {
            return FNM_NOMATCH;
        }
        p++;
        s++;
    }

    return (*s == '\0') ? 0 : FNM_NOMATCH;
}

static struct passwd g_pw;
static char g_pw_name[64];
static char g_pw_passwd[64];
static char g_pw_dir[128];
static char g_pw_shell[128];
static char g_login_name[64];

static struct passwd* passwd_lookup_name(const char* name) {
    if (!name || name[0] == '\0') {
        return NULL;
    }

    FILE* f = fopen("/etc/passwd", "r");
    if (!f) {
        return NULL;
    }

    char line[256];
    struct passwd* out = NULL;

    while (fgets(line, sizeof(line), f)) {
        // name:pass:uid:gid:home:shell
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char* fields[6] = {0};
        int nf = 0;
        char* p = line;
        for (; nf < 6; nf++) {
            fields[nf] = p;
            char* c = strchr(p, ':');
            if (!c) break;
            *c = '\0';
            p = c + 1;
        }

        if (!fields[0] || strcmp(fields[0], name) != 0) {
            continue;
        }

        g_pw.pw_name = g_pw_name;
        g_pw.pw_passwd = g_pw_passwd;
        g_pw.pw_comment = NULL;
        g_pw.pw_gecos = NULL;
        g_pw.pw_dir = g_pw_dir;
        g_pw.pw_shell = g_pw_shell;

        strncpy(g_pw_name, fields[0], sizeof(g_pw_name) - 1u);
        g_pw_name[sizeof(g_pw_name) - 1u] = '\0';
        strncpy(g_pw_passwd, (fields[1] ? fields[1] : ""), sizeof(g_pw_passwd) - 1u);
        g_pw_passwd[sizeof(g_pw_passwd) - 1u] = '\0';

        g_pw.pw_uid = (uid_t)(fields[2] ? strtoul(fields[2], NULL, 10) : 0);
        g_pw.pw_gid = (gid_t)(fields[3] ? strtoul(fields[3], NULL, 10) : 0);

        strncpy(g_pw_dir, (fields[4] ? fields[4] : "/"), sizeof(g_pw_dir) - 1u);
        g_pw_dir[sizeof(g_pw_dir) - 1u] = '\0';
        strncpy(g_pw_shell, (fields[5] ? fields[5] : "/bin/sh"), sizeof(g_pw_shell) - 1u);
        g_pw_shell[sizeof(g_pw_shell) - 1u] = '\0';

        out = &g_pw;
        break;
    }

    fclose(f);
    return out;
}

struct passwd* getpwnam(const char* name) {
    return passwd_lookup_name(name);
}

struct passwd* getpwuid(uid_t uid) {
    FILE* f = fopen("/etc/passwd", "r");
    if (!f) {
        return NULL;
    }

    char line[256];
    struct passwd* out = NULL;

    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char* fields[6] = {0};
        int nf = 0;
        char* p = line;
        for (; nf < 6; nf++) {
            fields[nf] = p;
            char* c = strchr(p, ':');
            if (!c) break;
            *c = '\0';
            p = c + 1;
        }

        if (!fields[2]) {
            continue;
        }
        uid_t file_uid = (uid_t)strtoul(fields[2], NULL, 10);
        if (file_uid != uid) {
            continue;
        }

        out = passwd_lookup_name(fields[0]);
        break;
    }

    fclose(f);
    return out;
}

char* getlogin(void) {
    struct passwd* pw = getpwuid(geteuid());
    if (!pw || !pw->pw_name) {
        errno = ENOENT;
        return NULL;
    }
    strncpy(g_login_name, pw->pw_name, sizeof(g_login_name) - 1u);
    g_login_name[sizeof(g_login_name) - 1u] = '\0';
    return g_login_name;
}

int getlogin_r(char* buf, size_t buflen) {
    if (!buf || buflen == 0) {
        return ERANGE;
    }
    char* login = getlogin();
    if (!login) {
        return errno ? errno : ENOENT;
    }
    size_t n = strlen(login) + 1u;
    if (n > buflen) {
        return ERANGE;
    }
    memcpy(buf, login, n);
    return 0;
}

static struct group g_gr;
static char g_gr_name[64];
static char g_gr_passwd[64];
static char* g_gr_mem[1];

static struct group* group_lookup_name(const char* name) {
    if (!name || name[0] == '\0') {
        return NULL;
    }

    FILE* f = fopen("/etc/group", "r");
    if (!f) {
        return NULL;
    }

    char line[256];
    struct group* out = NULL;

    while (fgets(line, sizeof(line), f)) {
        // name:pass:gid:members
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char* fields[4] = {0};
        int nf = 0;
        char* p = line;
        for (; nf < 4; nf++) {
            fields[nf] = p;
            char* c = strchr(p, ':');
            if (!c) break;
            *c = '\0';
            p = c + 1;
        }

        if (!fields[0] || strcmp(fields[0], name) != 0) {
            continue;
        }

        g_gr.gr_name = g_gr_name;
        g_gr.gr_passwd = g_gr_passwd;
        g_gr.gr_mem = g_gr_mem;
        g_gr_mem[0] = NULL;

        strncpy(g_gr_name, fields[0], sizeof(g_gr_name) - 1u);
        g_gr_name[sizeof(g_gr_name) - 1u] = '\0';
        strncpy(g_gr_passwd, (fields[1] ? fields[1] : ""), sizeof(g_gr_passwd) - 1u);
        g_gr_passwd[sizeof(g_gr_passwd) - 1u] = '\0';

        g_gr.gr_gid = (gid_t)(fields[2] ? strtoul(fields[2], NULL, 10) : 0);
        out = &g_gr;
        break;
    }

    fclose(f);
    return out;
}

struct group* getgrnam(const char* name) {
    return group_lookup_name(name);
}

struct group* getgrgid(gid_t gid) {
    FILE* f = fopen("/etc/group", "r");
    if (!f) {
        return NULL;
    }

    char line[256];
    struct group* out = NULL;

    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char* fields[4] = {0};
        int nf = 0;
        char* p = line;
        for (; nf < 4; nf++) {
            fields[nf] = p;
            char* c = strchr(p, ':');
            if (!c) break;
            *c = '\0';
            p = c + 1;
        }

        if (!fields[2]) {
            continue;
        }
        gid_t file_gid = (gid_t)strtoul(fields[2], NULL, 10);
        if (file_gid != gid) {
            continue;
        }

        out = group_lookup_name(fields[0]);
        break;
    }

    fclose(f);
    return out;
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
    SYS_SELECT = 79,
};

// For select() syscall
#define VOS_FD_SETSIZE 64
typedef struct vos_fd_set_internal {
    unsigned int bits[VOS_FD_SETSIZE / 32];
} vos_fd_set_internal_t;

typedef struct vos_timeval_internal {
    int tv_sec;
    int tv_usec;
} vos_timeval_internal_t;

typedef struct vos_stat {
    unsigned char is_dir;
    unsigned char is_symlink;
    unsigned short mode;
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
    unsigned char is_symlink;
    unsigned short mode;
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

static inline int vos_sys_alarm(unsigned int seconds) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_ALARM), "b"(seconds)
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

static inline int vos_sys_rtc_set(const vos_rtc_datetime_t* dt) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RTC_SET), "b"(dt)
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

static inline void* vos_sys_mmap(void* addr, unsigned int length, unsigned int prot, unsigned int flags, int fd) {
    void* ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MMAP), "b"(addr), "c"(length), "d"(prot), "S"(flags), "D"(fd)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_munmap(void* addr, unsigned int length) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MUNMAP), "b"(addr), "c"(length)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_mprotect(void* addr, unsigned int length, unsigned int prot) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MPROTECT), "b"(addr), "c"(length), "d"(prot)
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

static inline int vos_sys_lstat(const char* path, vos_stat_t* st) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_LSTAT), "b"(path), "c"(st)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_symlink(const char* target, const char* linkpath) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SYMLINK), "b"(target), "c"(linkpath)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_readlink(const char* path, char* buf, unsigned int cap) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READLINK), "b"(path), "c"(buf), "d"(cap)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_chmod(const char* path, unsigned int mode) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CHMOD), "b"(path), "c"(mode)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_fchmod(int fd, unsigned int mode) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FCHMOD), "b"(fd), "c"(mode)
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

static inline int vos_sys_fcntl(int fd, int cmd, int arg) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FCNTL), "b"(fd), "c"(cmd), "d"(arg)
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

static inline int vos_sys_getppid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPPID)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_getpgrp(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPGRP)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_setpgid(int pid, int pgid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETPGID), "b"(pid), "c"(pgid)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_wait(int pid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAIT), "b"(pid)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_waitpid(int pid, int* status, int options) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAITPID), "b"(pid), "c"(status), "d"(options)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_fork(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FORK)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_execve(const char* path, const char* const* argv, unsigned int argc) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_EXECVE), "b"(path), "c"(argv), "d"(argc)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_spawn(const char* path, const char* const* argv, unsigned int argc) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SPAWN), "b"(path), "c"(argv), "d"(argc)
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

static inline intptr_t vos_sys_signal(int sig, uintptr_t handler) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SIGNAL), "b"(sig), "c"(handler)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SIGPROCMASK), "b"(how), "c"(set), "d"(oldset)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_getuid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETUID)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_getgid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETGID)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_setuid(int uid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETUID), "b"(uid)
        : "memory"
    );
    return ret;
}

static inline int vos_sys_setgid(int gid) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SETGID), "b"(gid)
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

static void epoch_to_ymdhms(time_t t, int* year, int* month, int* day, int* hour, int* minute, int* second) {
    if (!year || !month || !day || !hour || !minute || !second) {
        return;
    }

    long sec = (long)t;
    if (sec < 0) {
        sec = 0;
    }

    long days = sec / 86400L;
    long rem = sec % 86400L;
    if (rem < 0) {
        rem += 86400L;
        days -= 1;
    }

    *hour = (int)(rem / 3600L);
    rem %= 3600L;
    *minute = (int)(rem / 60L);
    *second = (int)(rem % 60L);

    int y = 1970;
    for (;;) {
        int ydays = is_leap(y) ? 366 : 365;
        if (days >= ydays) {
            days -= ydays;
            y++;
            continue;
        }
        break;
    }

    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 1;
    for (int i = 0; i < 12; i++) {
        int dim = mdays[i];
        if (i == 1 && is_leap(y)) {
            dim++;
        }
        if (days >= dim) {
            days -= dim;
            m++;
            continue;
        }
        break;
    }

    *year = y;
    *month = m;
    *day = (int)days + 1;
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

static void fill_stat_common(struct stat* st, const vos_stat_t* vst, const char* abs_path) {
    if (!st || !vst) {
        return;
    }

    memset(st, 0, sizeof(*st));
    mode_t perm = (mode_t)(vst->mode & 07777u);
    if (vst->is_symlink) {
        st->st_mode = S_IFLNK | perm;
    } else if (vst->is_dir) {
        st->st_mode = S_IFDIR | perm;
    } else {
        st->st_mode = S_IFREG | perm;
    }
    st->st_nlink = 1;
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_size = (off_t)vst->size;
    st->st_blksize = 512;
    st->st_blocks = (blkcnt_t)((vst->size + 511u) / 512u);

    time_t t = fat_ts_to_epoch(vst->wdate, vst->wtime);
    st->st_mtim.tv_sec = t;
    st->st_mtim.tv_nsec = 0;
    st->st_atim = st->st_mtim;
    st->st_ctim = st->st_mtim;

    if (abs_path && abs_path[0] == '/') {
        st->st_dev = dev_from_path(abs_path);
        st->st_ino = ino_from_path(abs_path);
    }
}

int open(const char* name, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    (void)va_arg(ap, int);
    va_end(ap);

    if (!name) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = name;
    if (path_make_abs(abs, name) == 0) {
        eff = abs;
    }

    int rc = vos_sys_open(eff, flags);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    fd_path_set(rc, eff);
    return rc;
}

int close(int file) {
    int rc = vos_sys_close(file);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    fd_path_clear(file);
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

    fill_stat_common(st, &vst, fd_path_get(file));
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
    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (path_make_abs(abs, path) == 0) {
        eff = abs;
    }

    int rc = vos_sys_stat(eff, &vst);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    fill_stat_common(st, &vst, eff);
    return 0;
}

int lstat(const char* path, struct stat* st) {
    if (!path || !st) {
        errno = EINVAL;
        return -1;
    }

    vos_stat_t vst;
    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (path_make_abs(abs, path) == 0) {
        eff = abs;
    }

    int rc = vos_sys_lstat(eff, &vst);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    fill_stat_common(st, &vst, eff);
    return 0;
}

int fstatat(int dirfd, const char* path, struct stat* st, int flags) {
    if (!path || !st) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (!path_is_abs(path)) {
        if (dirfd == AT_FDCWD) {
            if (path_make_abs(abs, path) < 0) {
                return -1;
            }
        } else {
            const char* base = fd_path_get(dirfd);
            if (!base) {
                errno = EBADF;
                return -1;
            }
            if (path_join(abs, base, path) < 0) {
                return -1;
            }
        }
        eff = abs;
    }

    if ((flags & AT_SYMLINK_NOFOLLOW) != 0) {
        return lstat(eff, st);
    }
    return stat(eff, st);
}

int openat(int dirfd, const char* path, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    int mode = va_arg(ap, int);
    va_end(ap);
    (void)mode;

    if (!path) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (!path_is_abs(path)) {
        if (dirfd == AT_FDCWD) {
            if (path_make_abs(abs, path) < 0) {
                return -1;
            }
        } else {
            const char* base = fd_path_get(dirfd);
            if (!base) {
                errno = EBADF;
                return -1;
            }
            if (path_join(abs, base, path) < 0) {
                return -1;
            }
        }
        eff = abs;
    }

    return open(eff, flags, 0);
}

int creat(const char* path, mode_t mode) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int utimensat(int dirfd, const char* path, const struct timespec times[2], int flags) {
    (void)times;
    (void)flags;
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (!path_is_abs(path)) {
        if (dirfd == AT_FDCWD) {
            if (path_make_abs(abs, path) < 0) {
                return -1;
            }
        } else {
            const char* base = fd_path_get(dirfd);
            if (!base) {
                errno = EBADF;
                return -1;
            }
            if (path_join(abs, base, path) < 0) {
                return -1;
            }
        }
        eff = abs;
    }

    // Timestamp writes aren't supported yet. Succeed if the path exists.
    struct stat st;
    if (stat(eff, &st) < 0) {
        return -1;
    }
    return 0;
}

int faccessat(int dirfd, const char* path, int mode, int flags) {
    (void)flags;
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (!path_is_abs(path)) {
        if (dirfd == AT_FDCWD) {
            if (path_make_abs(abs, path) < 0) {
                return -1;
            }
        } else {
            const char* base = fd_path_get(dirfd);
            if (!base) {
                errno = EBADF;
                return -1;
            }
            if (path_join(abs, base, path) < 0) {
                return -1;
            }
        }
        eff = abs;
    }

    return access(eff, mode);
}

int unlinkat(int dirfd, const char* path, int flags) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (!path_is_abs(path)) {
        if (dirfd == AT_FDCWD) {
            if (path_make_abs(abs, path) < 0) {
                return -1;
            }
        } else {
            const char* base = fd_path_get(dirfd);
            if (!base) {
                errno = EBADF;
                return -1;
            }
            if (path_join(abs, base, path) < 0) {
                return -1;
            }
        }
        eff = abs;
    }

    if ((flags & AT_REMOVEDIR) != 0) {
        return rmdir(eff);
    }
    return unlink(eff);
}

int futimens(int fd, const struct timespec times[2]) {
    (void)times;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    // Timestamp writes aren't supported yet. Treat as success.
    return 0;
}

int chmod(const char* path, mode_t mode) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (path_make_abs(abs, path) == 0) {
        eff = abs;
    }

    int rc = vos_sys_chmod(eff, (unsigned int)mode);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int chown(const char* path, uid_t owner, gid_t group) {
    (void)path;
    (void)owner;
    (void)group;
    // VOS does not currently persist ownership. Treat as success.
    return 0;
}

int lchown(const char* path, uid_t owner, gid_t group) {
    return chown(path, owner, group);
}

int mknod(const char* path, mode_t mode, dev_t dev) {
    (void)path;
    (void)mode;
    (void)dev;
    errno = ENOSYS;
    return -1;
}

int symlink(const char* target, const char* linkpath) {
    if (!target || !linkpath) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff_link = linkpath;
    if (path_make_abs(abs, linkpath) == 0) {
        eff_link = abs;
    }

    int rc = vos_sys_symlink(target, eff_link);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

ssize_t readlink(const char* path, char* buf, size_t bufsize) {
    if (!path || (bufsize != 0 && !buf)) {
        errno = EINVAL;
        return -1;
    }

    char abs[VOS_PATH_MAX];
    const char* eff = path;
    if (path_make_abs(abs, path) == 0) {
        eff = abs;
    }

    int rc = vos_sys_readlink(eff, buf, (unsigned int)bufsize);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return (ssize_t)rc;
}

unsigned int alarm(unsigned int seconds) {
    int rc = vos_sys_alarm(seconds);
    if (rc < 0) {
        errno = -rc;
        return 0;
    }
    return (unsigned int)rc;
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

int clock_settime(clockid_t clock_id, const struct timespec* tp) {
    if (!tp) {
        errno = EINVAL;
        return -1;
    }
    if (clock_id != CLOCK_REALTIME) {
        errno = EINVAL;
        return -1;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    epoch_to_ymdhms(tp->tv_sec, &year, &month, &day, &hour, &minute, &second);

    vos_rtc_datetime_t dt;
    dt.year = (unsigned short)year;
    dt.month = (unsigned char)month;
    dt.day = (unsigned char)day;
    dt.hour = (unsigned char)hour;
    dt.minute = (unsigned char)minute;
    dt.second = (unsigned char)second;

    int rc = vos_sys_rtc_set(&dt);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    return 0;
}

int uname(struct utsname* buf) {
    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    memset(buf, 0, sizeof(*buf));

    // Keep these stable to make ports deterministic. When VOS grows a real
    // kernel versioning facility, these can be wired up to it.
    strncpy(buf->sysname, "VOS", sizeof(buf->sysname) - 1u);
    strncpy(buf->nodename, "vos", sizeof(buf->nodename) - 1u);
    strncpy(buf->release, "0.1.0", sizeof(buf->release) - 1u);
    strncpy(buf->version, "VOS kernel", sizeof(buf->version) - 1u);
    strncpy(buf->machine, "i386", sizeof(buf->machine) - 1u);

    return 0;
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

int fcntl(int fd, int cmd, ...) {
    int arg = 0;
    if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC || cmd == F_SETFD || cmd == F_SETFL) {
        va_list ap;
        va_start(ap, cmd);
        arg = va_arg(ap, int);
        va_end(ap);
    }

    int rc = vos_sys_fcntl(fd, cmd, arg);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
        fd_path_copy(rc, fd);
    }
    return rc;
}

int dup(int oldfd) {
    int rc = vos_sys_dup(oldfd);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    fd_path_copy(rc, oldfd);
    return rc;
}

int dup2(int oldfd, int newfd) {
    int rc = vos_sys_dup2(oldfd, newfd);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    fd_path_copy(newfd, oldfd);
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
    fd_path_clear(fds[0]);
    fd_path_clear(fds[1]);
    return 0;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)offset; // file-backed mappings aren't supported yet
    void* p = vos_sys_mmap(addr, (unsigned int)length, (unsigned int)prot, (unsigned int)flags, fd);
    if ((uintptr_t)p >= 0xFFFFF000u) {
        errno = -(int)(intptr_t)p;
        return MAP_FAILED;
    }
    return p;
}

int munmap(void* addr, size_t length) {
    int rc = vos_sys_munmap(addr, (unsigned int)length);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int mprotect(void* addr, size_t length, int prot) {
    int rc = vos_sys_mprotect(addr, (unsigned int)length, (unsigned int)prot);
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

// Use "used" attribute to ensure these override newlib's weak stubs.
// TCC doesn't support weak, so we rely on link order (libvosposix.a before libc.a).
__attribute__((used))
_sig_func_ptr signal(int sig, _sig_func_ptr handler) {
    intptr_t rc = vos_sys_signal(sig, (uintptr_t)handler);
    if (rc < 0) {
        errno = (int)-rc;
        return SIG_ERR;
    }
    return (_sig_func_ptr)(uintptr_t)rc;
}

__attribute__((used))
_sig_func_ptr _signal_r(struct _reent* r, int sig, _sig_func_ptr handler) {
    (void)r;
    return signal(sig, handler);
}

__attribute__((used))
int raise(int sig) {
    return kill(getpid(), sig);
}

__attribute__((used))
int _raise_r(struct _reent* r, int sig) {
    (void)r;
    return raise(sig);
}

__attribute__((used))
int _init_signal_r(struct _reent* r) {
    (void)r;
    return 0;
}

__attribute__((used))
int _init_signal(void) {
    return 0;
}

__attribute__((used))
int __sigtramp_r(struct _reent* r, int sig) {
    (void)r;
    (void)sig;
    return 0;
}

__attribute__((used))
int __sigtramp(int sig) {
    (void)sig;
    return 0;
}

int sigaction(int sig, const struct sigaction* act, struct sigaction* oact) {
    if (sig <= 0) {
        errno = EINVAL;
        return -1;
    }

    _sig_func_ptr new_handler = SIG_DFL;
    if (act) {
        new_handler = act->sa_handler;
    }

    intptr_t old = vos_sys_signal(sig, (uintptr_t)new_handler);
    if (old < 0) {
        errno = (int)-old;
        return -1;
    }

    if (oact) {
        oact->sa_handler = (_sig_func_ptr)(uintptr_t)old;
        oact->sa_mask = 0;
        oact->sa_flags = 0;
    }
    return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    int rc = vos_sys_sigprocmask(how, set, oldset);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int siginterrupt(int sig, int flag) {
    (void)sig;
    (void)flag;
    // VOS currently always interrupts blocking syscalls when a signal is pending.
    return 0;
}

int getpid(void) {
    return vos_sys_getpid();
}

pid_t fork(void) {
    int rc = vos_sys_fork();
    if (rc < 0) {
        errno = -rc;
        return (pid_t)-1;
    }
    return (pid_t)rc;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    (void)envp;
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    unsigned int argc = 0;
    if (argv) {
        for (; argc < VOS_EXEC_MAX_ARGS && argv[argc]; argc++) {
        }
        if (argc == VOS_EXEC_MAX_ARGS && argv[argc]) {
            errno = E2BIG;
            return -1;
        }
    }

    int rc = vos_sys_execve(path, (const char* const*)argv, argc);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int execvp(const char* file, char* const argv[]) {
    if (!file || file[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/')) {
        return execve(file, argv, NULL);
    }

    // Try as-is first.
    execve(file, argv, NULL);

    // Then try /bin and /usr/bin.
    char path[VOS_PATH_MAX];

    snprintf(path, sizeof(path), "/bin/%s", file);
    execve(path, argv, NULL);

    snprintf(path, sizeof(path), "/usr/bin/%s", file);
    execve(path, argv, NULL);

    return -1;
}

pid_t waitpid(pid_t pid, int* status, int options) {
    int rc = vos_sys_waitpid((int)pid, status, options);
    if (rc < 0) {
        errno = -rc;
        return (pid_t)-1;
    }
    return (pid_t)rc;
}

pid_t wait(int* status) {
    return waitpid((pid_t)-1, status, 0);
}

int getppid(void) {
    int rc = vos_sys_getppid();
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int getpgrp(void) {
    int rc = vos_sys_getpgrp();
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int setpgid(pid_t pid, pid_t pgid) {
    int rc = vos_sys_setpgid((int)pid, (int)pgid);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    (void)fd;
    (void)pgrp;
    // VOS doesn't support job control, but return success to not break apps
    return 0;
}

uid_t getuid(void) {
    int rc = vos_sys_getuid();
    if (rc < 0) {
        errno = -rc;
        return (uid_t)-1;
    }
    return (uid_t)rc;
}

gid_t getgid(void) {
    int rc = vos_sys_getgid();
    if (rc < 0) {
        errno = -rc;
        return (gid_t)-1;
    }
    return (gid_t)rc;
}

uid_t geteuid(void) {
    return getuid();
}

gid_t getegid(void) {
    return getgid();
}

int setuid(uid_t uid) {
    int rc = vos_sys_setuid((int)uid);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int setgid(gid_t gid) {
    int rc = vos_sys_setgid((int)gid);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

mode_t umask(mode_t mask) {
    (void)mask;
    return 0;
}

int fchmod(int fd, mode_t mode) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }

    int rc = vos_sys_fchmod(fd, (unsigned int)mode);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

static int vos_system_run(const char* command) {
    if (!command) {
        return 1;
    }

    const char* argv[3];
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = command;

    int child = vos_sys_spawn("/bin/sh", argv, 3);
    if (child < 0) {
        errno = -child;
        return -1;
    }

    int old_fg = 0;
    (void)ioctl(0, TIOCGPGRP, &old_fg);

    int fg = child;
    (void)ioctl(0, TIOCSPGRP, &fg);

    int code = vos_sys_wait(child);

    (void)ioctl(0, TIOCSPGRP, &old_fg);
    return code;
}

int _system_r(struct _reent* r, const char* command) {
    int rc = vos_system_run(command);
    if (rc < 0 && r) {
        r->_errno = errno;
    }
    return rc;
}

int system(const char* command) {
    return vos_system_run(command);
}

__attribute__((noreturn)) void _exit(int code) {
    vos_sys_exit(code);
}

// select() implementation
static inline int vos_sys_select(int nfds, vos_fd_set_internal_t* readfds,
                                  vos_fd_set_internal_t* writefds,
                                  vos_fd_set_internal_t* exceptfds,
                                  vos_timeval_internal_t* timeout) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SELECT), "b"(nfds), "c"(readfds), "d"(writefds), "S"(exceptfds), "D"(timeout)
        : "memory"
    );
    return ret;
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    vos_fd_set_internal_t r = {0}, w = {0}, e = {0};
    vos_timeval_internal_t tv = {0, 0};

    // Convert fd_set to our internal format
    // fd_set is typically an array of longs, we need to handle this portably
    if (readfds) {
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (FD_ISSET(i, readfds)) {
                r.bits[i / 32] |= (1u << (i % 32));
            }
        }
    }
    if (writefds) {
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (FD_ISSET(i, writefds)) {
                w.bits[i / 32] |= (1u << (i % 32));
            }
        }
    }
    if (exceptfds) {
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (FD_ISSET(i, exceptfds)) {
                e.bits[i / 32] |= (1u << (i % 32));
            }
        }
    }

    if (timeout) {
        tv.tv_sec = (int)timeout->tv_sec;
        tv.tv_usec = (int)timeout->tv_usec;
    }

    int rc = vos_sys_select(nfds,
                            readfds ? &r : NULL,
                            writefds ? &w : NULL,
                            exceptfds ? &e : NULL,
                            timeout ? &tv : NULL);

    if (rc < 0) {
        errno = -rc;
        return -1;
    }

    // Convert back
    if (readfds) {
        FD_ZERO(readfds);
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (r.bits[i / 32] & (1u << (i % 32))) {
                FD_SET(i, readfds);
            }
        }
    }
    if (writefds) {
        FD_ZERO(writefds);
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (w.bits[i / 32] & (1u << (i % 32))) {
                FD_SET(i, writefds);
            }
        }
    }
    if (exceptfds) {
        FD_ZERO(exceptfds);
        for (int i = 0; i < nfds && i < VOS_FD_SETSIZE; i++) {
            if (e.bits[i / 32] & (1u << (i % 32))) {
                FD_SET(i, exceptfds);
            }
        }
    }

    return rc;
}

// poll() implementation using select()
int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    if (!fds || nfds == 0) {
        if (timeout > 0) {
            usleep((unsigned int)timeout * 1000);
        }
        return 0;
    }

    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    int maxfd = -1;
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        if (fds[i].fd > maxfd) maxfd = fds[i].fd;
        if (fds[i].events & POLLIN) FD_SET(fds[i].fd, &readfds);
        if (fds[i].events & POLLOUT) FD_SET(fds[i].fd, &writefds);
    }

    struct timeval tv;
    struct timeval* tvp = NULL;
    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    int rc = select(maxfd + 1, &readfds, &writefds, NULL, tvp);
    if (rc < 0) {
        return -1;
    }

    int nready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].fd < 0) continue;
        if ((fds[i].events & POLLIN) && FD_ISSET(fds[i].fd, &readfds)) {
            fds[i].revents |= POLLIN;
        }
        if ((fds[i].events & POLLOUT) && FD_ISSET(fds[i].fd, &writefds)) {
            fds[i].revents |= POLLOUT;
        }
        if (fds[i].revents) nready++;
    }

    return nready;
}
