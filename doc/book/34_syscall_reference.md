# Chapter 34: Syscall Quick Reference

This chapter provides a quick reference for all VOS system calls.

## Syscall Convention

- **Interrupt**: `int 0x80`
- **Syscall number**: `eax`
- **Arguments**: `ebx`, `ecx`, `edx`, `esi`, `edi`
- **Return value**: `eax` (negative = error)

```c
static inline int32_t syscall3(int num, int a, int b, int c) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
    );
    return ret;
}
```

## Process Management

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 1 | exit | status | - | Terminate process |
| 2 | fork | - | pid | Create child process |
| 3 | waitpid | pid, status*, options | pid | Wait for child |
| 6 | execve | path, argv, envp | -1/errno | Execute program |
| 7 | getpid | - | pid | Get process ID |
| 8 | getppid | - | pid | Get parent PID |
| 64 | setsid | - | sid | Create session |
| 65 | getsid | pid | sid | Get session ID |
| 66 | setpgid | pid, pgid | 0/-1 | Set process group |
| 67 | getpgrp | - | pgrp | Get process group |

### exit (1)

```c
void _exit(int status);
```

Terminates the calling process with exit status.

### fork (2)

```c
pid_t fork(void);
```

Creates a new process by duplicating the calling process.
- Returns 0 in child
- Returns child PID in parent
- Returns -1 on error

### waitpid (3)

```c
pid_t waitpid(pid_t pid, int *status, int options);
```

Waits for a child process to change state.
- `pid = -1`: Wait for any child
- `pid > 0`: Wait for specific child
- `options`: WNOHANG for non-blocking

### execve (6)

```c
int execve(const char *path, char *const argv[], char *const envp[]);
```

Replaces current process with new program.
Only returns on error.

**Limits:**
- Max arguments: 4096 (`VOS_EXEC_MAX_ARGS`)
- Max per-argument size: 4096 bytes
- Max total argument data: 128KB
- Returns `E2BIG` if limits exceeded

## File Operations

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 4 | read | fd, buf, count | bytes | Read from file |
| 5 | write | fd, buf, count | bytes | Write to file |
| 9 | open | path, flags, mode | fd | Open file |
| 10 | close | fd | 0/-1 | Close file |
| 11 | lseek | fd, offset, whence | pos | Seek in file |
| 12 | stat | path, stat* | 0/-1 | Get file info |
| 13 | fstat | fd, stat* | 0/-1 | Get file info by fd |
| 14 | unlink | path | 0/-1 | Delete file |
| 15 | mkdir | path, mode | 0/-1 | Create directory |
| 16 | rmdir | path | 0/-1 | Remove directory |
| 17 | rename | old, new | 0/-1 | Rename file |
| 18 | getcwd | buf, size | buf/-1 | Get working dir |
| 19 | chdir | path | 0/-1 | Change directory |
| 23 | dup | fd | newfd | Duplicate fd |
| 24 | dup2 | oldfd, newfd | newfd | Duplicate to specific fd |
| 25 | pipe | fds[2] | 0/-1 | Create pipe |
| 30 | truncate | path, length | 0/-1 | Truncate file |
| 31 | ftruncate | fd, length | 0/-1 | Truncate by fd |
| 35 | access | path, mode | 0/-1 | Check access |
| 38 | readdir | fd, dirent* | 0/1/-1 | Read directory entry |
| 47 | readlink | path, buf, size | len | Read symlink |
| 51 | openat | dirfd, path, flags | fd | Open relative to dir |

### open (9)

```c
int open(const char *path, int flags, mode_t mode);
```

Flags:
- `O_RDONLY`, `O_WRONLY`, `O_RDWR`
- `O_CREAT`, `O_TRUNC`, `O_APPEND`
- `O_EXCL`, `O_DIRECTORY`

### read (4)

```c
ssize_t read(int fd, void *buf, size_t count);
```

Returns number of bytes read, 0 at EOF, -1 on error.

### write (5)

```c
ssize_t write(int fd, const void *buf, size_t count);
```

Returns number of bytes written, -1 on error.

### lseek (11)

```c
off_t lseek(int fd, off_t offset, int whence);
```

Whence:
- `SEEK_SET`: Absolute position
- `SEEK_CUR`: Relative to current
- `SEEK_END`: Relative to end

## Memory Management

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 20 | brk | addr | 0/-1 | Set program break |
| 21 | sbrk | incr | addr | Increment break |
| 45 | mmap | addr, len, prot, flags, fd, off | addr | Map memory |
| 46 | munmap | addr, len | 0/-1 | Unmap memory |

### sbrk (21)

```c
void *sbrk(intptr_t increment);
```

Extends data segment by increment bytes.
Returns previous break address.

### mmap (45)

```c
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
```

Currently supports:
- `MAP_ANONYMOUS | MAP_PRIVATE`
- No file mapping yet

## Signals

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 26 | kill | pid, sig | 0/-1 | Send signal |
| 27 | signal | sig, handler | oldhandler | Set handler |
| 28 | sigaction | sig, act*, oldact* | 0/-1 | Signal action |
| 29 | sigprocmask | how, set*, oldset* | 0/-1 | Block signals |
| 44 | sigsuspend | mask* | -1 | Wait for signal |
| 60 | sigreturn | - | - | Return from handler |

### kill (26)

```c
int kill(pid_t pid, int sig);
```

- `pid > 0`: Send to specific process
- `pid = 0`: Send to process group
- `pid = -1`: Send to all (except init)
- `pid < -1`: Send to group |pid|

### signal (27)

```c
sighandler_t signal(int sig, sighandler_t handler);
```

Handler values:
- `SIG_DFL`: Default action
- `SIG_IGN`: Ignore signal
- Function pointer: Custom handler

## Time

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 40 | time | time_t* | time | Get time |
| 41 | gettimeofday | tv*, tz* | 0/-1 | Get time |
| 42 | nanosleep | req*, rem* | 0/-1 | Sleep |
| 43 | clock_gettime | clk_id, tp* | 0/-1 | Get clock |
| 52 | setitimer | which, new, old | 0/-1 | Set timer |
| 53 | getitimer | which, curr | 0/-1 | Get timer |

### nanosleep (42)

```c
int nanosleep(const struct timespec *req, struct timespec *rem);
```

Sleep for specified duration.

### clock_gettime (43)

```c
int clock_gettime(clockid_t clk_id, struct timespec *tp);
```

Clock IDs:
- `CLOCK_REALTIME`: Wall clock
- `CLOCK_MONOTONIC`: Monotonic since boot

## Terminal I/O

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 32 | tcgetattr | fd, termios* | 0/-1 | Get termios |
| 33 | tcsetattr | fd, act, termios* | 0/-1 | Set termios |
| 34 | ioctl | fd, req, arg | varies | Device control |
| 36 | isatty | fd | 1/0 | Check if TTY |

### ioctl (34)

Common requests:
- `TIOCGWINSZ`: Get window size
- `TIOCSWINSZ`: Set window size
- `TIOCGPGRP`: Get foreground group
- `TIOCSPGRP`: Set foreground group

## VOS-Specific

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 70 | uptime_ms | - | ms | System uptime |
| 71 | sleep | ms | 0 | Sleep milliseconds |
| 72 | yield | - | 0 | Yield CPU |
| 73 | proc_list | buf, size | count | List processes |
| 74 | screen_is_fb | - | 1/0 | Check framebuffer |
| 75 | gfx_blit_rgba | x, y, w, h, pixels | 0/-1 | Blit graphics |
| 76 | font_count | - | count | Get font count |
| 77 | font_info | idx, info* | 0/-1 | Get font info |
| 78 | font_set | idx | 0/-1 | Set font |
| 79 | font_get_current | - | idx | Get current font |

### uptime_ms (70)

```c
uint32_t sys_uptime_ms(void);
```

Returns milliseconds since system boot.

### gfx_blit_rgba (75)

```c
int sys_gfx_blit_rgba(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       const uint32_t *pixels);
```

Blits RGBA pixel data to framebuffer.

### proc_list (73)

```c
typedef struct {
    uint32_t pid;
    uint32_t ppid;
    uint32_t state;
    char name[32];
} vos_proc_info_t;

int sys_proc_list(vos_proc_info_t *buf, size_t max_count);
```

Returns list of running processes.

## Miscellaneous

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 22 | uname | utsname* | 0/-1 | System info |
| 37 | umask | mask | oldmask | Set file mask |
| 39 | fcntl | fd, cmd, arg | varies | File control |
| 48 | getenv_vos | name, buf, size | len | Get env var |
| 49 | setenv_vos | name, value | 0/-1 | Set env var |
| 50 | unsetenv_vos | name | 0/-1 | Unset env var |

### uname (22)

```c
struct utsname {
    char sysname[65];   // "VOS"
    char nodename[65];  // hostname
    char release[65];   // version
    char version[65];   // build info
    char machine[65];   // "i686"
};

int uname(struct utsname *buf);
```

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 1 | EPERM | Operation not permitted |
| 2 | ENOENT | No such file or directory |
| 3 | ESRCH | No such process |
| 4 | EINTR | Interrupted system call |
| 5 | EIO | I/O error |
| 7 | E2BIG | Argument list too long |
| 9 | EBADF | Bad file descriptor |
| 10 | ECHILD | No child processes |
| 11 | EAGAIN | Try again |
| 12 | ENOMEM | Out of memory |
| 13 | EACCES | Permission denied |
| 14 | EFAULT | Bad address |
| 17 | EEXIST | File exists |
| 19 | ENODEV | No such device |
| 20 | ENOTDIR | Not a directory |
| 21 | EISDIR | Is a directory |
| 22 | EINVAL | Invalid argument |
| 23 | ENFILE | Too many open files (system) |
| 24 | EMFILE | Too many open files (process) |
| 25 | ENOTTY | Not a typewriter |
| 27 | EFBIG | File too large |
| 28 | ENOSPC | No space left |
| 29 | ESPIPE | Illegal seek |
| 30 | EROFS | Read-only filesystem |
| 32 | EPIPE | Broken pipe |

## Userland Wrappers

Located in `user/syscall.h`:

```c
// Simple wrappers
static inline void sys_exit(int status) {
    syscall1(SYS_EXIT, status);
}

static inline int32_t sys_fork(void) {
    return syscall0(SYS_FORK);
}

static inline int32_t sys_read(int fd, void *buf, size_t count) {
    return syscall3(SYS_READ, fd, (int)buf, count);
}

// ... etc
```

## Newlib Integration

Newlib stubs in `user/newlib_syscalls.c` translate POSIX calls to VOS syscalls:

```c
int _open(const char *name, int flags, int mode) {
    int ret = syscall3(SYS_OPEN, (int)name, flags, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}
```

## Summary

VOS provides 71+ system calls covering:

1. **Process management** (fork, exec, wait, exit)
2. **File operations** (open, read, write, close)
3. **Memory management** (sbrk, mmap)
4. **Signals** (kill, signal, sigaction)
5. **Time** (clock_gettime, nanosleep)
6. **Terminal I/O** (tcgetattr, ioctl)
7. **VOS-specific** (graphics, fonts, process list)

Use this reference when developing VOS applications or extending the kernel.

---

*Previous: [Chapter 33: References](33_references.md)*
*Index: [Table of Contents](00_index.md)*
