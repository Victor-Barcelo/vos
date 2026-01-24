# Chapter 26: Newlib Integration

Newlib is a C library designed for embedded systems. VOS uses Newlib to provide standard C library functions to user programs.

## What is Newlib?

Newlib provides:
- Standard C library (stdio, stdlib, string, etc.)
- Math library (libm)
- POSIX compatibility layer
- Designed for small footprint

### Why Newlib?

- **Portable**: Works with many architectures
- **Modular**: Only link what you use
- **Documented**: Well-specified syscall interface
- **Lightweight**: Smaller than glibc

## Syscall Stubs

Newlib requires the OS to implement "syscall stubs" that bridge to kernel services.

### Required Stubs

```c
// Process control
void _exit(int status);
int _execve(const char *name, char *const argv[], char *const envp[]);
int _fork(void);
int _wait(int *status);
int _kill(int pid, int sig);
int _getpid(void);

// File operations
int _open(const char *name, int flags, int mode);
int _close(int fd);
int _read(int fd, void *buf, size_t count);
int _write(int fd, const void *buf, size_t count);
int _lseek(int fd, int offset, int whence);
int _fstat(int fd, struct stat *st);
int _stat(const char *path, struct stat *st);
int _link(const char *old, const char *new);
int _unlink(const char *name);
int _isatty(int fd);

// Memory
void *_sbrk(ptrdiff_t incr);

// Time
int _gettimeofday(struct timeval *tv, void *tz);
int _times(struct tms *buf);
```

### VOS Implementation

```c
// user/newlib_syscalls.c

#include <sys/stat.h>
#include <sys/times.h>
#include <errno.h>

// Syscall wrappers
static inline int32_t syscall3(int num, int a, int b, int c) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
    );
    return ret;
}

void _exit(int status) {
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

int _open(const char *name, int flags, int mode) {
    int ret = syscall3(SYS_OPEN, (int)name, flags, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int _read(int fd, void *buf, size_t count) {
    int ret = syscall3(SYS_READ, fd, (int)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int _write(int fd, const void *buf, size_t count) {
    int ret = syscall3(SYS_WRITE, fd, (int)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

void *_sbrk(ptrdiff_t incr) {
    int ret = syscall1(SYS_SBRK, incr);
    if (ret < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }
    return (void *)ret;
}

int _fork(void) {
    int ret = syscall0(SYS_FORK);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int _execve(const char *name, char *const argv[], char *const envp[]) {
    int ret = syscall3(SYS_EXECVE, (int)name, (int)argv, (int)envp);
    // execve only returns on error
    errno = -ret;
    return -1;
}

int _wait(int *status) {
    return _waitpid(-1, status, 0);
}

int _waitpid(int pid, int *status, int options) {
    int ret = syscall3(SYS_WAITPID, pid, (int)status, options);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}
```

## Building Newlib for VOS

### Cross-Compiler Setup

```bash
export TARGET=i686-vos
export PREFIX=/usr/local/cross

# Build binutils
cd binutils-2.40
./configure --target=$TARGET --prefix=$PREFIX
make && make install

# Build GCC (stage 1)
cd gcc-13.2.0
./configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c \
    --without-headers
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

### Building Newlib

```bash
export PATH=$PREFIX/bin:$PATH

cd newlib-4.3.0.20230120
./configure --target=$TARGET --prefix=$PREFIX \
    --disable-multilib \
    --enable-newlib-io-long-long \
    --enable-newlib-register-fini \
    --disable-newlib-supplied-syscalls

make
make install
```

### Linking with Newlib

```makefile
CFLAGS = -I$(SYSROOT)/usr/include
LDFLAGS = -L$(SYSROOT)/usr/lib -nostdlib

# Link order matters!
LIBS = -lc -lm -lgcc

user_program: main.o syscalls.o
    $(CC) $(LDFLAGS) -o $@ crt0.o main.o syscalls.o $(LIBS)
```

## Header Files

### VOS-Specific Headers

```c
// user/sys/stat.h - VOS stat structure
struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    time_t    st_atime;
    time_t    st_mtime;
    time_t    st_ctime;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
};
```

```c
// user/sys/utsname.h
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};
```

## Startup Code

### crt0.asm

```nasm
section .text
global _start
extern main
extern exit
extern _init_signal

_start:
    ; Clear frame pointer for stack traces
    xor ebp, ebp

    ; Get argc, argv, envp from stack
    pop eax             ; argc
    mov ebx, esp        ; argv
    lea ecx, [ebx + eax*4 + 4]  ; envp

    ; Initialize signal handlers
    push ecx
    push ebx
    push eax
    call _init_signal
    pop eax
    pop ebx
    pop ecx

    ; Call main(argc, argv, envp)
    push ecx
    push ebx
    push eax
    call main
    add esp, 12

    ; Call exit(return_value)
    push eax
    call exit

    ; Should never reach here
    hlt
```

## Using Standard Functions

### stdio.h

```c
#include <stdio.h>

int main() {
    printf("Hello, %s!\n", "World");

    FILE *f = fopen("/etc/motd", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            fputs(line, stdout);
        }
        fclose(f);
    }

    return 0;
}
```

### stdlib.h

```c
#include <stdlib.h>

int main() {
    // Memory allocation
    char *buf = malloc(1024);
    free(buf);

    // String conversion
    int n = atoi("42");
    char str[20];
    itoa(n * 2, str, 10);

    // Environment
    char *home = getenv("HOME");
    setenv("FOO", "bar", 1);

    return 0;
}
```

### string.h

```c
#include <string.h>

int main() {
    char buf[100];

    strcpy(buf, "Hello");
    strcat(buf, " World");

    size_t len = strlen(buf);

    char *found = strstr(buf, "World");

    memset(buf, 0, sizeof(buf));

    return 0;
}
```

## Math Library

```c
#include <math.h>

int main() {
    double x = sin(3.14159 / 2);
    double y = sqrt(2.0);
    double z = pow(2.0, 10.0);

    int exp;
    double m = frexp(1024.0, &exp);

    return 0;
}
```

## Limitations

### Missing Features

Some Newlib functions require OS support VOS doesn't have:

- `popen()`/`pclose()` - Need shell pipe support
- `system()` - Needs `/bin/sh`
- `getpwnam()`/`getpwuid()` - No password database
- `gethostbyname()` - No networking

### Workarounds

```c
// Stub for missing functions
char *getlogin(void) {
    return "user";
}

struct passwd *getpwuid(uid_t uid) {
    static struct passwd pw = {
        .pw_name = "user",
        .pw_uid = 0,
        .pw_gid = 0,
        .pw_dir = "/home/user",
        .pw_shell = "/bin/sh"
    };
    return &pw;
}
```

## Compiling Programs

### With Cross-Compiler

```bash
i686-vos-gcc -o program program.c
```

### With TCC (in VOS)

```bash
tcc -o program program.c
```

TCC in VOS uses a pre-built library:
- `/usr/lib/libc.a` - C library
- `/usr/lib/libm.a` - Math library
- `/usr/lib/crt0.o` - Startup code

## Summary

Newlib integration provides:

1. **Standard C library** for user programs
2. **Syscall stubs** bridging to VOS kernel
3. **Header files** matching VOS structures
4. **Startup code** for program initialization
5. **Math library** for numeric operations

This enables portable C programs to run on VOS.

---

*Previous: [Chapter 25: Shell and Commands](25_shell.md)*
*Next: [Chapter 27: Graphics Programming](27_graphics.md)*
