# Chapter 21: System Calls

System calls are the interface between user programs and the kernel. VOS provides 71 syscalls covering files, processes, memory, time, signals, and more.

## Syscall Interface

### Convention

- **Trigger**: `int 0x80`
- **Number**: EAX
- **Arguments**: EBX, ECX, EDX, ESI, EDI
- **Return**: EAX (negative = error)

### User-Side Wrapper

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

### Kernel-Side Handler

```c
interrupt_frame_t* syscall_handle(interrupt_frame_t *frame) {
    uint32_t num = frame->eax;
    uint32_t a = frame->ebx;
    uint32_t b = frame->ecx;
    uint32_t c = frame->edx;
    uint32_t d = frame->esi;
    uint32_t e = frame->edi;

    int32_t result;

    switch (num) {
        case SYS_EXIT:      result = sys_exit(a); break;
        case SYS_READ:      result = sys_read(a, (void*)b, c); break;
        case SYS_WRITE:     result = sys_write(a, (void*)b, c); break;
        // ... 68 more cases ...
        default:            result = -ENOSYS;
    }

    frame->eax = (uint32_t)result;
    return frame;
}
```

## Complete Syscall Reference

### File Operations

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 2 | SYS_OPEN | `int open(path, flags, mode)` | Open file |
| 3 | SYS_CLOSE | `int close(fd)` | Close file descriptor |
| 4 | SYS_READ | `ssize_t read(fd, buf, count)` | Read from fd |
| 5 | SYS_WRITE | `ssize_t write(fd, buf, count)` | Write to fd |
| 6 | SYS_LSEEK | `off_t lseek(fd, offset, whence)` | Seek in file |
| 7 | SYS_STAT | `int stat(path, buf)` | Get file status |
| 8 | SYS_FSTAT | `int fstat(fd, buf)` | Get fd status |
| 9 | SYS_UNLINK | `int unlink(path)` | Delete file |
| 10 | SYS_RENAME | `int rename(old, new)` | Rename file |
| 11 | SYS_MKDIR | `int mkdir(path, mode)` | Create directory |
| 12 | SYS_RMDIR | `int rmdir(path)` | Remove directory |
| 13 | SYS_GETCWD | `char* getcwd(buf, size)` | Get working directory |
| 14 | SYS_CHDIR | `int chdir(path)` | Change directory |
| 15 | SYS_OPENDIR | `DIR* opendir(path)` | Open directory |
| 16 | SYS_READDIR | `int readdir(fd, entry)` | Read directory entry |
| 17 | SYS_CLOSEDIR | `int closedir(fd)` | Close directory |
| 18 | SYS_DUP | `int dup(oldfd)` | Duplicate fd |
| 19 | SYS_DUP2 | `int dup2(oldfd, newfd)` | Duplicate to specific fd |
| 20 | SYS_PIPE | `int pipe(fds[2])` | Create pipe |
| 21 | SYS_TRUNCATE | `int truncate(path, len)` | Truncate file |
| 22 | SYS_FTRUNCATE | `int ftruncate(fd, len)` | Truncate by fd |
| 23 | SYS_ACCESS | `int access(path, mode)` | Check access |
| 24 | SYS_CHMOD | `int chmod(path, mode)` | Change permissions |
| 25 | SYS_CHOWN | `int chown(path, uid, gid)` | Change owner |
| 26 | SYS_LSTAT | `int lstat(path, buf)` | Stat symlink |
| 27 | SYS_READLINK | `ssize_t readlink(path, buf, size)` | Read symlink |
| 28 | SYS_SYMLINK | `int symlink(target, linkpath)` | Create symlink |
| 29 | SYS_LINK | `int link(old, new)` | Create hard link |
| 30 | SYS_FCNTL | `int fcntl(fd, cmd, arg)` | File control |
| 31 | SYS_IOCTL | `int ioctl(fd, request, arg)` | Device control |
| 32 | SYS_UMASK | `mode_t umask(mask)` | Set file creation mask |

### Process Management

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 0 | SYS_EXIT | `void exit(status)` | Exit process |
| 33 | SYS_GETPID | `pid_t getpid()` | Get process ID |
| 34 | SYS_GETPPID | `pid_t getppid()` | Get parent PID |
| 35 | SYS_GETUID | `uid_t getuid()` | Get user ID |
| 36 | SYS_GETGID | `gid_t getgid()` | Get group ID |
| 37 | SYS_SETUID | `int setuid(uid)` | Set user ID |
| 38 | SYS_SETGID | `int setgid(gid)` | Set group ID |
| 39 | SYS_GETEUID | `uid_t geteuid()` | Get effective UID |
| 40 | SYS_GETEGID | `gid_t getegid()` | Get effective GID |
| 41 | SYS_GETPGRP | `pid_t getpgrp()` | Get process group |
| 42 | SYS_SETPGID | `int setpgid(pid, pgid)` | Set process group |
| 43 | SYS_GETSID | `pid_t getsid(pid)` | Get session ID |
| 44 | SYS_SETSID | `pid_t setsid()` | Create session |
| 68 | SYS_FORK | `pid_t fork()` | Create child process |
| 69 | SYS_EXECVE | `int execve(path, argv, envp)` | Execute program |
| 70 | SYS_WAITPID | `pid_t waitpid(pid, status, opts)` | Wait for child |
| 1 | SYS_YIELD | `void yield()` | Yield CPU |

### Memory Management

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 45 | SYS_SBRK | `void* sbrk(increment)` | Extend heap |
| 46 | SYS_MMAP | `void* mmap(addr, len, prot, flags, fd, off)` | Map memory |
| 47 | SYS_MUNMAP | `int munmap(addr, len)` | Unmap memory |
| 48 | SYS_MPROTECT | `int mprotect(addr, len, prot)` | Set protection |

### Time

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 49 | SYS_TIME | `time_t time(tloc)` | Get Unix time |
| 50 | SYS_GETTIMEOFDAY | `int gettimeofday(tv, tz)` | Get time |
| 51 | SYS_NANOSLEEP | `int nanosleep(req, rem)` | Sleep |
| 52 | SYS_CLOCK_GETTIME | `int clock_gettime(id, tp)` | Get clock |
| 53 | SYS_CLOCK_SETTIME | `int clock_settime(id, tp)` | Set clock |

### Signals

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 54 | SYS_KILL | `int kill(pid, sig)` | Send signal |
| 55 | SYS_SIGNAL | `sighandler_t signal(sig, handler)` | Set handler |
| 56 | SYS_SIGACTION | `int sigaction(sig, act, oldact)` | Signal action |
| 57 | SYS_SIGPROCMASK | `int sigprocmask(how, set, oldset)` | Block signals |
| 58 | SYS_SIGPENDING | `int sigpending(set)` | Get pending |
| 59 | SYS_SIGSUSPEND | `int sigsuspend(mask)` | Wait for signal |
| 60 | SYS_SIGRETURN | `void sigreturn()` | Return from handler |
| 61 | SYS_ALARM | `unsigned alarm(seconds)` | Set alarm |
| 62 | SYS_PAUSE | `int pause()` | Wait for signal |

### Terminal I/O

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 63 | SYS_TCGETATTR | `int tcgetattr(fd, termios)` | Get terminal attr |
| 64 | SYS_TCSETATTR | `int tcsetattr(fd, act, termios)` | Set terminal attr |
| 65 | SYS_ISATTY | `int isatty(fd)` | Is terminal |
| 66 | SYS_TTYNAME | `char* ttyname(fd)` | Get terminal name |

### System Information

| Number | Name | Signature | Description |
|--------|------|-----------|-------------|
| 67 | SYS_UNAME | `int uname(buf)` | Get system info |

## Key Syscall Implementations

### sys_write

```c
int32_t sys_write(int fd, const void *buf, size_t count) {
    task_t *task = current_task;

    if (fd < 0 || fd >= MAX_FDS || !task->fds[fd]) {
        return -EBADF;
    }

    file_desc_t *desc = task->fds[fd];

    // Validate user buffer
    if (!validate_user_buffer(buf, count, false)) {
        return -EFAULT;
    }

    // Handle stdout/stderr specially
    if (desc->node == &console_node) {
        for (size_t i = 0; i < count; i++) {
            screen_putchar(((char *)buf)[i]);
        }
        return count;
    }

    return vfs_write(fd, buf, count);
}
```

### sys_fork

```c
int32_t sys_fork(interrupt_frame_t *frame) {
    task_t *parent = current_task;
    task_t *child = task_create();

    child->ppid = parent->pid;
    child->parent = parent;

    // Clone address space
    child->page_directory = clone_page_directory(parent->page_directory);

    // Clone memory regions
    child->brk = parent->brk;
    child->vm_areas = clone_vm_areas(parent->vm_areas);

    // Clone file descriptors
    for (int i = 0; i < MAX_FDS; i++) {
        if (parent->fds[i]) {
            child->fds[i] = dup_fd(parent->fds[i]);
        }
    }

    // Clone signal handlers
    memcpy(child->signal_handlers, parent->signal_handlers,
           sizeof(parent->signal_handlers));

    strcpy(child->cwd, parent->cwd);

    // Set up child's stack to return 0
    interrupt_frame_t *child_frame = (interrupt_frame_t *)child->esp;
    memcpy(child_frame, frame, sizeof(interrupt_frame_t));
    child_frame->eax = 0;

    // Parent returns child PID
    return child->pid;
}
```

### sys_execve

```c
int32_t sys_execve(const char *path, char *const argv[], char *const envp[]) {
    // Validate strings
    char kpath[256];
    if (copy_from_user(kpath, path, 256) < 0) {
        return -EFAULT;
    }

    // Load executable
    void *elf_data;
    size_t elf_size;
    int ret = vfs_read_file(kpath, &elf_data, &elf_size);
    if (ret < 0) return ret;

    elf_load_result_t result;
    ret = elf_load(elf_data, elf_size, &result);
    kfree(elf_data);
    if (ret < 0) return ret;

    // Copy args to user stack
    uint32_t sp = copy_args_to_stack(result.stack, argv, envp);

    // Replace current process
    task_t *task = current_task;
    free_user_pages(task);

    // Modify frame for new program
    interrupt_frame_t *frame = (interrupt_frame_t *)task->esp;
    frame->eip = result.entry;
    frame->user_esp = sp;
    frame->eax = 0;

    // Reset signals
    for (int i = 0; i < 32; i++) {
        task->signal_handlers[i] = SIG_DFL;
    }

    return 0;
}
```

### sys_mmap

```c
int32_t sys_mmap(void *addr, size_t length, int prot, int flags,
                 int fd, off_t offset) {
    task_t *task = current_task;

    // Find free address space
    uint32_t vaddr = find_free_region(task, (uint32_t)addr, length);
    if (!vaddr) return -ENOMEM;

    // Calculate page flags
    uint32_t pflags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) pflags |= PAGE_RW;

    // Map pages
    for (uint32_t off = 0; off < length; off += 0x1000) {
        uint32_t phys = pmm_alloc_frame();
        paging_map_page(vaddr + off, phys, pflags);
        memset((void *)(vaddr + off), 0, 0x1000);
    }

    // If mapping a file, copy contents
    if (fd >= 0 && (flags & MAP_PRIVATE)) {
        vfs_lseek(fd, offset, SEEK_SET);
        vfs_read(fd, (void *)vaddr, length);
    }

    // Track region
    add_vm_area(task, vaddr, vaddr + length, prot);

    return vaddr;
}
```

## Error Codes

```c
#define EPERM           1   // Operation not permitted
#define ENOENT          2   // No such file or directory
#define ESRCH           3   // No such process
#define EINTR           4   // Interrupted syscall
#define EIO             5   // I/O error
#define ENXIO           6   // No such device
#define E2BIG           7   // Argument list too long
#define ENOEXEC         8   // Exec format error
#define EBADF           9   // Bad file descriptor
#define ECHILD         10   // No child processes
#define EAGAIN         11   // Try again
#define ENOMEM         12   // Out of memory
#define EACCES         13   // Permission denied
#define EFAULT         14   // Bad address
#define EBUSY          16   // Device busy
#define EEXIST         17   // File exists
#define ENODEV         19   // No such device
#define ENOTDIR        20   // Not a directory
#define EISDIR         21   // Is a directory
#define EINVAL         22   // Invalid argument
#define EMFILE         24   // Too many open files
#define ENOSPC         28   // No space left
#define ESPIPE         29   // Illegal seek
#define EPIPE          32   // Broken pipe
#define ENOSYS         38   // Function not implemented
```

## Summary

VOS provides 71 system calls enabling:

1. **File I/O** - open, read, write, close, seek, stat
2. **Directories** - mkdir, rmdir, readdir, chdir
3. **Processes** - fork, execve, wait, exit
4. **Memory** - sbrk, mmap, munmap
5. **Signals** - kill, signal, sigaction
6. **Time** - time, nanosleep, clock_gettime
7. **Terminal** - tcgetattr, tcsetattr, ioctl

This comprehensive syscall set enables running real UNIX-like programs.

---

*Previous: [Chapter 20: User Mode and ELF](20_usermode.md)*
*Next: [Chapter 22: Signals and IPC](22_signals.md)*
