# Chapter 22: Signals and IPC

Signals provide asynchronous notification to processes. VOS implements POSIX-style signal handling with 32 signals.

## Signal Basics

Signals are software interrupts delivered to processes:

- **Asynchronous**: Can arrive at any time
- **Default actions**: Terminate, ignore, stop, continue
- **Custom handlers**: User-defined functions

## Signal Numbers

```c
#define SIGHUP      1   // Hangup
#define SIGINT      2   // Interrupt (Ctrl+C)
#define SIGQUIT     3   // Quit (Ctrl+\)
#define SIGILL      4   // Illegal instruction
#define SIGTRAP     5   // Trace trap
#define SIGABRT     6   // Abort
#define SIGBUS      7   // Bus error
#define SIGFPE      8   // Floating point exception
#define SIGKILL     9   // Kill (cannot be caught)
#define SIGUSR1    10   // User defined 1
#define SIGSEGV    11   // Segmentation violation
#define SIGUSR2    12   // User defined 2
#define SIGPIPE    13   // Broken pipe
#define SIGALRM    14   // Alarm clock
#define SIGTERM    15   // Termination
#define SIGSTKFLT  16   // Stack fault
#define SIGCHLD    17   // Child status changed
#define SIGCONT    18   // Continue
#define SIGSTOP    19   // Stop (cannot be caught)
#define SIGTSTP    20   // Terminal stop (Ctrl+Z)
#define SIGTTIN    21   // Background read
#define SIGTTOU    22   // Background write
#define SIGURG     23   // Urgent condition
#define SIGXCPU    24   // CPU time limit
#define SIGXFSZ    25   // File size limit
#define SIGVTALRM  26   // Virtual timer
#define SIGPROF    27   // Profiling timer
#define SIGWINCH   28   // Window size change
#define SIGIO      29   // I/O possible
#define SIGPWR     30   // Power failure
#define SIGSYS     31   // Bad syscall
```

## Signal Handler

```c
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)   // Default action
#define SIG_IGN ((sighandler_t)1)   // Ignore signal
#define SIG_ERR ((sighandler_t)-1)  // Error return
```

## Task Signal State

```c
typedef struct task {
    // ...
    uint32_t pending_signals;       // Bitmask of pending signals
    uint32_t blocked_signals;       // Bitmask of blocked signals
    sighandler_t signal_handlers[32];
    // ...
} task_t;
```

## Sending Signals

### sys_kill

```c
int32_t sys_kill(pid_t pid, int sig) {
    if (sig < 0 || sig > 31) {
        return -EINVAL;
    }

    if (pid > 0) {
        // Send to specific process
        task_t *target = find_task_by_pid(pid);
        if (!target) return -ESRCH;
        return task_signal(target, sig);
    }
    else if (pid == 0) {
        // Send to all processes in current group
        return kill_pgrp(current_task->pgrp, sig);
    }
    else if (pid == -1) {
        // Send to all processes (except init)
        return kill_all(sig);
    }
    else {
        // Send to process group |pid|
        return kill_pgrp(-pid, sig);
    }
}

int task_signal(task_t *task, int sig) {
    if (sig == 0) {
        return 0;  // Just check if process exists
    }

    // Set pending bit
    task->pending_signals |= (1 << sig);

    // Wake up if blocked
    if (task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
    }

    return 0;
}
```

### Keyboard Signals

```c
void keyboard_handle_ctrl_c(void) {
    // Send SIGINT to foreground process group
    if (foreground_pgrp > 0) {
        kill_pgrp(foreground_pgrp, SIGINT);
    }
}

void keyboard_handle_ctrl_z(void) {
    // Send SIGTSTP to foreground process group
    if (foreground_pgrp > 0) {
        kill_pgrp(foreground_pgrp, SIGTSTP);
    }
}
```

## Setting Handlers

### sys_signal

```c
sighandler_t sys_signal(int sig, sighandler_t handler) {
    if (sig < 1 || sig > 31) {
        return SIG_ERR;
    }

    // SIGKILL and SIGSTOP cannot be caught
    if (sig == SIGKILL || sig == SIGSTOP) {
        return SIG_ERR;
    }

    task_t *task = current_task;
    sighandler_t old = task->signal_handlers[sig];
    task->signal_handlers[sig] = handler;

    return old;
}
```

### sys_sigaction

```c
struct sigaction {
    sighandler_t sa_handler;
    uint32_t sa_mask;
    int sa_flags;
};

int32_t sys_sigaction(int sig, const struct sigaction *act,
                      struct sigaction *oldact) {
    if (sig < 1 || sig > 31) return -EINVAL;
    if (sig == SIGKILL || sig == SIGSTOP) return -EINVAL;

    task_t *task = current_task;

    if (oldact) {
        oldact->sa_handler = task->signal_handlers[sig];
        // ... copy mask and flags
    }

    if (act) {
        task->signal_handlers[sig] = act->sa_handler;
        // ... set mask and flags
    }

    return 0;
}
```

## Signal Delivery

Signals are delivered when returning to user mode:

```c
interrupt_frame_t* check_signals(interrupt_frame_t *frame) {
    task_t *task = current_task;

    // Only deliver in user mode
    if ((frame->cs & 3) != 3) {
        return frame;
    }

    while (task->pending_signals & ~task->blocked_signals) {
        int sig = find_first_signal(task);
        task->pending_signals &= ~(1 << sig);

        sighandler_t handler = task->signal_handlers[sig];

        if (handler == SIG_IGN) {
            continue;
        }
        else if (handler == SIG_DFL) {
            default_signal_action(task, sig);
        }
        else {
            // Deliver to user handler
            deliver_signal(frame, sig, handler);
            return frame;
        }
    }

    return frame;
}
```

### Default Actions

```c
static void default_signal_action(task_t *task, int sig) {
    switch (sig) {
    // Terminate
    case SIGALRM:
    case SIGBUS:
    case SIGFPE:
    case SIGHUP:
    case SIGILL:
    case SIGINT:
    case SIGKILL:
    case SIGPIPE:
    case SIGPOLL:
    case SIGPROF:
    case SIGSEGV:
    case SIGSYS:
    case SIGTERM:
    case SIGTRAP:
    case SIGUSR1:
    case SIGUSR2:
    case SIGVTALRM:
    case SIGXCPU:
    case SIGXFSZ:
        task_exit(128 + sig);
        break;

    // Ignore
    case SIGCHLD:
    case SIGURG:
    case SIGWINCH:
        break;

    // Stop
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
        task->state = TASK_STOPPED;
        schedule(NULL);
        break;

    // Continue
    case SIGCONT:
        if (task->state == TASK_STOPPED) {
            task->state = TASK_READY;
        }
        break;
    }
}
```

### User Handler Delivery

```c
static void deliver_signal(interrupt_frame_t *frame, int sig,
                          sighandler_t handler) {
    // Save current frame on user stack
    uint32_t *user_sp = (uint32_t *)frame->user_esp;

    // Push signal return frame
    user_sp -= sizeof(signal_frame_t) / 4;
    signal_frame_t *sf = (signal_frame_t *)user_sp;

    sf->pretcode = (uint32_t)signal_trampoline;
    sf->sig = sig;
    memcpy(&sf->saved_frame, frame, sizeof(interrupt_frame_t));

    // Set up to call handler
    frame->eip = (uint32_t)handler;
    frame->user_esp = (uint32_t)user_sp;

    // Pass signal number as argument
    user_sp--;
    *user_sp = sig;
    frame->user_esp = (uint32_t)user_sp;
}
```

### Signal Trampoline

```c
// In user space - returns from signal handler
void signal_trampoline(void) {
    __asm__ volatile(
        "mov $60, %%eax\n"  // SYS_SIGRETURN
        "int $0x80\n"
        ::: "eax"
    );
}

// Kernel syscall to restore context
int32_t sys_sigreturn(interrupt_frame_t *frame) {
    task_t *task = current_task;

    // Get signal frame from user stack
    signal_frame_t *sf = (signal_frame_t *)(frame->user_esp - 4);

    // Restore saved frame
    memcpy(frame, &sf->saved_frame, sizeof(interrupt_frame_t));

    return frame->eax;  // Restore original return value
}
```

## Signal Blocking

```c
int32_t sys_sigprocmask(int how, const uint32_t *set, uint32_t *oldset) {
    task_t *task = current_task;

    if (oldset) {
        *oldset = task->blocked_signals;
    }

    if (set) {
        switch (how) {
        case SIG_BLOCK:
            task->blocked_signals |= *set;
            break;
        case SIG_UNBLOCK:
            task->blocked_signals &= ~*set;
            break;
        case SIG_SETMASK:
            task->blocked_signals = *set;
            break;
        default:
            return -EINVAL;
        }

        // Cannot block SIGKILL or SIGSTOP
        task->blocked_signals &= ~((1 << SIGKILL) | (1 << SIGSTOP));
    }

    return 0;
}
```

## Pipes

Pipes provide unidirectional byte streams between processes:

```c
typedef struct pipe {
    char buffer[PIPE_BUFFER_SIZE];
    int read_pos;
    int write_pos;
    int count;
    int readers;
    int writers;
    task_t *wait_read;
    task_t *wait_write;
} pipe_t;

int32_t sys_pipe(int fds[2]) {
    pipe_t *pipe = kcalloc(1, sizeof(pipe_t));
    pipe->readers = 1;
    pipe->writers = 1;

    // Create read end (fds[0])
    vfs_node_t *read_node = vfs_create_pipe_node(pipe, PIPE_READ);
    fds[0] = vfs_alloc_fd(read_node, O_RDONLY);

    // Create write end (fds[1])
    vfs_node_t *write_node = vfs_create_pipe_node(pipe, PIPE_WRITE);
    fds[1] = vfs_alloc_fd(write_node, O_WRONLY);

    return 0;
}
```

### Pipe Read

```c
int32_t pipe_read(pipe_t *pipe, void *buffer, size_t count) {
    char *buf = buffer;
    size_t read = 0;

    while (read < count) {
        if (pipe->count == 0) {
            if (pipe->writers == 0) {
                // EOF - no more writers
                break;
            }
            // Block until data available
            pipe->wait_read = current_task;
            task_block(current_task);
            pipe->wait_read = NULL;
            continue;
        }

        buf[read++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
        pipe->count--;

        // Wake writer if waiting
        if (pipe->wait_write) {
            task_unblock(pipe->wait_write);
        }
    }

    return read;
}
```

### Pipe Write

```c
int32_t pipe_write(pipe_t *pipe, const void *buffer, size_t count) {
    const char *buf = buffer;
    size_t written = 0;

    while (written < count) {
        if (pipe->readers == 0) {
            // Broken pipe - no readers
            task_signal(current_task, SIGPIPE);
            return -EPIPE;
        }

        if (pipe->count == PIPE_BUFFER_SIZE) {
            // Buffer full - block
            pipe->wait_write = current_task;
            task_block(current_task);
            pipe->wait_write = NULL;
            continue;
        }

        pipe->buffer[pipe->write_pos] = buf[written++];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_SIZE;
        pipe->count++;

        // Wake reader if waiting
        if (pipe->wait_read) {
            task_unblock(pipe->wait_read);
        }
    }

    return written;
}
```

## Process Groups

```c
int32_t sys_setpgid(pid_t pid, pid_t pgid) {
    task_t *task;

    if (pid == 0) {
        task = current_task;
    } else {
        task = find_task_by_pid(pid);
        if (!task) return -ESRCH;
    }

    if (pgid == 0) {
        pgid = task->pid;
    }

    task->pgrp = pgid;
    return 0;
}

pid_t sys_getpgrp(void) {
    return current_task->pgrp;
}
```

## Summary

VOS signal and IPC support provides:

1. **32 signals** with POSIX semantics
2. **Custom handlers** via signal/sigaction
3. **Signal blocking** with sigprocmask
4. **Default actions** (terminate, ignore, stop, continue)
5. **Keyboard signals** (Ctrl+C, Ctrl+Z)
6. **Pipes** for inter-process communication
7. **Process groups** for job control

---

*Previous: [Chapter 21: System Calls](21_syscalls.md)*
*Next: [Chapter 23: Terminal I/O](23_termios.md)*
