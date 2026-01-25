# Chapter 24: POSIX Overview and VOS Compliance

This chapter provides an extensive overview of POSIX standards and documents VOS's current level of compliance.

## What is POSIX?

POSIX (Portable Operating System Interface) is a family of standards specified by IEEE for maintaining compatibility between operating systems. The name was suggested by Richard Stallman and stands for "Portable Operating System Interface" with the X representing UNIX heritage.

### Purpose

POSIX defines:
- System call interfaces
- Shell and utility behavior
- C library functions
- Threading APIs
- Real-time extensions

### Benefits

- **Portability**: Programs written to POSIX run on any compliant system
- **Familiarity**: Consistent interface across UNIX-like systems
- **Documentation**: Well-specified behavior

## POSIX Standards History

### POSIX.1 (IEEE 1003.1)

The original standard (1988) defined:
- Process creation and control (fork, exec, wait)
- File operations (open, read, write, close)
- Signals
- Pipes
- Terminal I/O (termios)
- Basic C library

### POSIX.1b (IEEE 1003.1b-1993)

Real-time extensions:
- Semaphores
- Shared memory
- Message queues
- Asynchronous I/O
- Clocks and timers

### POSIX.1c (IEEE 1003.1c-1995)

Thread extensions (pthreads):
- Thread creation and management
- Mutexes and condition variables
- Thread-specific data

### POSIX.2 (IEEE 1003.2)

Shell and utilities:
- Shell command language (sh)
- Utility programs (ls, grep, sed, etc.)
- Regular expressions

### Single UNIX Specification (SUS)

POSIX was incorporated into The Open Group's Single UNIX Specification, merging with X/Open standards. Modern POSIX (POSIX.1-2017) is also SUSv4.

## VOS POSIX Implementation Status

### Overall Assessment

**VOS POSIX Compliance: ~45%**

VOS implements a subset of POSIX focused on:
- Basic file operations
- Process control with fork/exec
- Signals
- Terminal I/O

### Supported Features

#### File Operations (80% complete)

| Function | Status | Notes |
|----------|--------|-------|
| open() | Yes | O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND |
| close() | Yes | |
| read() | Yes | |
| write() | Yes | |
| lseek() | Yes | SEEK_SET, SEEK_CUR, SEEK_END |
| stat() | Yes | Basic fields |
| fstat() | Yes | |
| lstat() | Partial | Returns stat for symlink target |
| unlink() | Yes | |
| link() | No | Hard links not supported |
| symlink() | Partial | Read support only |
| readlink() | Yes | |
| rename() | Yes | |
| mkdir() | Yes | |
| rmdir() | Yes | |
| opendir() | Yes | |
| readdir() | Yes | |
| closedir() | Yes | |
| getcwd() | Yes | |
| chdir() | Yes | |
| fchdir() | No | |
| dup() | Yes | |
| dup2() | Yes | |
| pipe() | Yes | |
| fcntl() | Partial | F_DUPFD, F_GETFL, F_SETFL |
| access() | Yes | |
| chmod() | Stub | VOS doesn't track permissions |
| chown() | Stub | VOS doesn't track ownership |
| umask() | Stub | |
| truncate() | Yes | |
| ftruncate() | Yes | |

#### Process Control (75% complete)

| Function | Status | Notes |
|----------|--------|-------|
| fork() | Yes | Full copy-on-write |
| execve() | Yes | |
| execl/execv/execvp | Yes | Via newlib |
| _exit() | Yes | |
| wait() | Yes | |
| waitpid() | Yes | WNOHANG supported |
| getpid() | Yes | |
| getppid() | Yes | |
| getuid() | Stub | Returns 0 |
| getgid() | Stub | Returns 0 |
| geteuid() | Stub | Returns 0 |
| getegid() | Stub | Returns 0 |
| setuid() | Stub | |
| setgid() | Stub | |
| setsid() | Yes | |
| getsid() | Yes | |
| setpgid() | Yes | |
| getpgrp() | Yes | |

#### Signals (70% complete)

| Function | Status | Notes |
|----------|--------|-------|
| kill() | Yes | |
| raise() | Yes | Via newlib |
| signal() | Yes | |
| sigaction() | Partial | Basic functionality |
| sigprocmask() | Yes | |
| sigpending() | Yes | |
| sigsuspend() | Yes | |
| pause() | Yes | |
| alarm() | Yes | |

**Supported Signals:**
- SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP
- SIGABRT, SIGBUS, SIGFPE, SIGKILL, SIGUSR1
- SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM
- SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP
- SIGTTIN, SIGTTOU, SIGURG, SIGWINCH

#### Terminal I/O (80% complete)

| Function | Status | Notes |
|----------|--------|-------|
| tcgetattr() | Yes | |
| tcsetattr() | Yes | TCSANOW, TCSADRAIN, TCSAFLUSH |
| cfgetispeed() | Yes | |
| cfgetospeed() | Yes | |
| cfsetispeed() | Yes | |
| cfsetospeed() | Yes | |
| tcdrain() | Yes | |
| tcflush() | Partial | |
| tcflow() | No | |
| tcsendbreak() | No | |
| isatty() | Yes | |
| ttyname() | Partial | |

**Termios Features:**
- Canonical and raw modes
- Echo control
- Signal characters (VINTR, VQUIT, VSUSP)
- Line editing (VERASE, VKILL)
- VMIN/VTIME for raw reads

#### Time Functions (60% complete)

| Function | Status | Notes |
|----------|--------|-------|
| time() | Yes | |
| gettimeofday() | Yes | |
| clock_gettime() | Partial | CLOCK_REALTIME, CLOCK_MONOTONIC |
| clock_settime() | No | |
| nanosleep() | Yes | |
| sleep() | Yes | Via nanosleep |
| usleep() | Yes | Via nanosleep |

#### Memory Management (50% complete)

| Function | Status | Notes |
|----------|--------|-------|
| sbrk() | Yes | |
| mmap() | Partial | MAP_ANONYMOUS, MAP_PRIVATE |
| munmap() | Yes | |
| mprotect() | Stub | |
| brk() | Partial | Via sbrk |

#### System Information (40% complete)

| Function | Status | Notes |
|----------|--------|-------|
| uname() | Yes | |
| gethostname() | No | |
| sysconf() | Partial | _SC_ARG_MAX, _SC_PAGE_SIZE |
| pathconf() | No | |
| fpathconf() | No | |

### Not Implemented

#### Threading (pthreads) - 0%

VOS does not currently support threads:
- pthread_create()
- pthread_join()
- pthread_mutex_*()
- pthread_cond_*()
- pthread_key_*()

#### Networking (sockets) - 0%

VOS has no networking:
- socket()
- bind()
- listen()
- accept()
- connect()
- send()/recv()
- select()/poll()

#### Advanced I/O - 0%

- select()
- poll()
- epoll (Linux-specific)
- kqueue (BSD-specific)
- Asynchronous I/O (aio_*)

#### IPC (System V) - 0%

- shmget()/shmat()
- msgget()/msgsnd()
- semget()/semop()

#### POSIX IPC - 0%

- sem_open()
- shm_open()
- mq_open()

## Implementation Details

### fork() Implementation

VOS implements a true fork() with copy-on-write potential:

```c
pid_t fork(void) {
    // Creates child process
    // Clones address space
    // Clones file descriptors
    // Child returns 0
    // Parent returns child PID
}
```

### execve() Implementation

```c
int execve(const char *path, char *const argv[], char *const envp[]) {
    // Loads ELF executable
    // Replaces address space
    // Sets up argc, argv, envp on stack
    // Jumps to entry point
}
```

### Signal Delivery

Signals are delivered on return to user mode:
1. Check pending & ~blocked signals
2. Find highest priority pending signal
3. If custom handler: set up signal frame and call handler
4. If SIG_DFL: perform default action
5. If SIG_IGN: discard signal

### Terminal Processing

Input goes through termios processing:
1. Input flags (c_iflag): CR/NL mapping, case
2. Signal check: VINTR, VQUIT, VSUSP
3. Line editing: VERASE, VKILL (if ICANON)
4. Echo (if ECHO)
5. Buffering: line (ICANON) or character (raw)

## Compliance Roadmap

### Phase 1: Core Completion (Current)

- [x] Basic file operations
- [x] fork/exec/wait
- [x] Basic signals
- [x] termios
- [ ] Complete fcntl
- [ ] File locking (flock)

### Phase 2: Enhanced Functionality

- [ ] select()/poll() for I/O multiplexing
- [ ] More complete mmap()
- [ ] Real permissions (chmod/chown)
- [ ] Symbolic link creation
- [ ] Directory file descriptors

### Phase 3: Advanced Features

- [ ] pthreads (cooperative initially)
- [ ] POSIX semaphores
- [ ] POSIX shared memory
- [ ] Better real-time support

### Phase 4: Networking

- [ ] Socket API
- [ ] TCP/IP stack
- [ ] DNS resolver

## Testing POSIX Compliance

### Test Programs

```c
// Test fork/exec
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/echo", "echo", "Hello", NULL);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}
```

```c
// Test signals
#include <signal.h>
#include <stdio.h>

volatile int got_signal = 0;

void handler(int sig) {
    got_signal = 1;
}

int main() {
    signal(SIGUSR1, handler);
    raise(SIGUSR1);
    return got_signal ? 0 : 1;
}
```

```c
// Test termios
#include <termios.h>
#include <unistd.h>

int main() {
    struct termios t;
    if (tcgetattr(0, &t) < 0) return 1;
    t.c_lflag &= ~ECHO;
    if (tcsetattr(0, TCSANOW, &t) < 0) return 1;
    return 0;
}
```

## Comparison with Other Systems

| Feature | VOS | Linux | FreeBSD | macOS |
|---------|-----|-------|---------|-------|
| fork/exec | Yes | Yes | Yes | Yes |
| Threads | No | Yes | Yes | Yes |
| Signals | Partial | Full | Full | Full |
| Sockets | No | Yes | Yes | Yes |
| select/poll | No | Yes | Yes | Yes |
| mmap | Partial | Full | Full | Full |
| termios | Yes | Yes | Yes | Yes |
| POSIX IPC | No | Yes | Yes | Yes |

## Porting Software to VOS

### Compatible Software

Programs using only:
- Basic file I/O
- fork/exec process model
- Simple signals
- termios

### Likely Compatible

With minor modifications:
- Command-line utilities
- Simple shells
- Text editors (vi, nano)
- Compilers (TCC)
- Interpreters (Lua, BASIC)

### Not Compatible

Programs requiring:
- Threads (most modern software)
- Networking (web, databases)
- select/poll (async I/O)
- Shared memory
- GUI toolkits

## Summary

VOS achieves approximately 45% POSIX compliance, focusing on:

1. **File operations** - Most POSIX file functions work
2. **Process control** - Full fork/exec/wait support
3. **Signals** - Basic signal handling and delivery
4. **Terminal I/O** - Complete termios implementation
5. **Time functions** - Basic time and sleep support

Key missing features:
- Threading (pthreads)
- Networking (sockets)
- I/O multiplexing (select/poll)
- Advanced IPC (shared memory, semaphores)

This level of compliance allows running many command-line utilities and simple programs while keeping the kernel implementation manageable.

---

*Previous: [Chapter 23: Terminal I/O](23_termios.md)*
*Next: [Chapter 25: Shell and Commands](25_shell.md)*
