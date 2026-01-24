# I/O Multiplexing Implementation for VOS

This document outlines the implementation of select(), poll(), and related I/O multiplexing for VOS.

---

## Current State

VOS currently lacks I/O multiplexing:
- `read()` blocks until data available
- `write()` blocks until complete
- No way to wait on multiple FDs
- No non-blocking I/O support

This limits:
- Event-driven programming
- Network servers (future)
- Interactive applications
- Efficient I/O handling

---

## Required Components

### 1. Non-blocking I/O

First, file descriptors need non-blocking mode support.

```c
// kernel/vfs.c

// Add to open_file_t structure
typedef struct open_file {
    vfs_node_t *node;
    uint32_t position;
    uint32_t flags;      // O_RDONLY, O_WRONLY, O_RDWR
    uint32_t status;     // ADD: O_NONBLOCK support
} open_file_t;

// Modify read to check non-blocking
int vfs_read(int fd, void *buf, size_t count) {
    open_file_t *f = get_open_file(fd);
    if (!f) return -EBADF;

    // Check if data available
    if (!f->node->ops->has_data(f->node)) {
        if (f->status & O_NONBLOCK) {
            return -EAGAIN;  // Would block
        }
        // Block until data available
        wait_for_data(f->node);
    }

    return f->node->ops->read(f->node, buf, count, f->position);
}
```

### 2. fcntl() for O_NONBLOCK

```c
// kernel/syscall.c

int sys_fcntl(int fd, int cmd, int arg) {
    open_file_t *f = get_open_file(fd);
    if (!f) return -EBADF;

    switch (cmd) {
        case F_GETFL:
            return f->flags | f->status;

        case F_SETFL:
            // Only allow changing O_NONBLOCK, O_APPEND
            f->status = arg & (O_NONBLOCK | O_APPEND);
            return 0;

        case F_DUPFD:
            return sys_dup2(fd, arg);

        default:
            return -EINVAL;
    }
}
```

---

## select() Implementation

### Data Structures

```c
// include/select.h

#define FD_SETSIZE 64

typedef struct {
    uint32_t bits[FD_SETSIZE / 32];
} fd_set;

#define FD_ZERO(set)    memset((set), 0, sizeof(fd_set))
#define FD_SET(fd, set) ((set)->bits[(fd)/32] |= (1 << ((fd)%32)))
#define FD_CLR(fd, set) ((set)->bits[(fd)/32] &= ~(1 << ((fd)%32)))
#define FD_ISSET(fd, set) ((set)->bits[(fd)/32] & (1 << ((fd)%32)))

struct timeval {
    long tv_sec;
    long tv_usec;
};
```

### Kernel Implementation

```c
// kernel/select.c

#include "task.h"
#include "timer.h"

// Check if any FD in set is ready
static int check_fds(int nfds, fd_set *readfds, fd_set *writefds,
                     fd_set *exceptfds, fd_set *result_read,
                     fd_set *result_write, fd_set *result_except) {
    int ready = 0;

    FD_ZERO(result_read);
    FD_ZERO(result_write);
    FD_ZERO(result_except);

    for (int fd = 0; fd < nfds; fd++) {
        open_file_t *f = get_open_file(fd);
        if (!f) continue;

        // Check readable
        if (readfds && FD_ISSET(fd, readfds)) {
            if (f->node->ops->has_data && f->node->ops->has_data(f->node)) {
                FD_SET(fd, result_read);
                ready++;
            }
        }

        // Check writable
        if (writefds && FD_ISSET(fd, writefds)) {
            if (f->node->ops->can_write && f->node->ops->can_write(f->node)) {
                FD_SET(fd, result_write);
                ready++;
            }
        }

        // Check exceptional
        if (exceptfds && FD_ISSET(fd, exceptfds)) {
            if (f->node->ops->has_error && f->node->ops->has_error(f->node)) {
                FD_SET(fd, result_except);
                ready++;
            }
        }
    }

    return ready;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds,
               fd_set *exceptfds, struct timeval *timeout) {
    if (nfds < 0 || nfds > FD_SETSIZE)
        return -EINVAL;

    fd_set result_read, result_write, result_except;
    uint32_t deadline = 0;

    // Calculate deadline if timeout specified
    if (timeout) {
        uint32_t ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
        deadline = timer_get_ticks() + ms;
    }

    while (1) {
        // Check all FDs
        int ready = check_fds(nfds, readfds, writefds, exceptfds,
                              &result_read, &result_write, &result_except);

        if (ready > 0) {
            // Copy results back
            if (readfds) *readfds = result_read;
            if (writefds) *writefds = result_write;
            if (exceptfds) *exceptfds = result_except;
            return ready;
        }

        // Check timeout
        if (timeout) {
            if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
                // Poll mode - return immediately
                if (readfds) FD_ZERO(readfds);
                if (writefds) FD_ZERO(writefds);
                if (exceptfds) FD_ZERO(exceptfds);
                return 0;
            }
            if (timer_get_ticks() >= deadline) {
                // Timeout expired
                if (readfds) FD_ZERO(readfds);
                if (writefds) FD_ZERO(writefds);
                if (exceptfds) FD_ZERO(exceptfds);
                return 0;
            }
        }

        // Sleep briefly then retry
        task_yield();
    }
}
```

### User-Space Wrapper

```c
// user/newlib_syscalls.c

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {
    int ret = syscall5(SYS_SELECT, nfds, (int)readfds, (int)writefds,
                       (int)exceptfds, (int)timeout);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}
```

---

## poll() Implementation

### Data Structures

```c
// include/poll.h

struct pollfd {
    int fd;           // File descriptor
    short events;     // Requested events
    short revents;    // Returned events
};

#define POLLIN   0x001  // Data to read
#define POLLOUT  0x004  // Can write
#define POLLERR  0x008  // Error
#define POLLHUP  0x010  // Hangup
#define POLLNVAL 0x020  // Invalid FD
```

### Kernel Implementation

```c
// kernel/poll.c

int sys_poll(struct pollfd *fds, int nfds, int timeout) {
    if (!fds || nfds < 0)
        return -EINVAL;

    uint32_t deadline = 0;
    if (timeout > 0) {
        deadline = timer_get_ticks() + timeout;
    }

    while (1) {
        int ready = 0;

        for (int i = 0; i < nfds; i++) {
            fds[i].revents = 0;

            if (fds[i].fd < 0) {
                continue;  // Skip negative FDs
            }

            open_file_t *f = get_open_file(fds[i].fd);
            if (!f) {
                fds[i].revents = POLLNVAL;
                ready++;
                continue;
            }

            // Check requested events
            if (fds[i].events & POLLIN) {
                if (f->node->ops->has_data &&
                    f->node->ops->has_data(f->node)) {
                    fds[i].revents |= POLLIN;
                }
            }

            if (fds[i].events & POLLOUT) {
                if (f->node->ops->can_write &&
                    f->node->ops->can_write(f->node)) {
                    fds[i].revents |= POLLOUT;
                }
            }

            // Always check for errors
            if (f->node->ops->has_error &&
                f->node->ops->has_error(f->node)) {
                fds[i].revents |= POLLERR;
            }

            if (fds[i].revents) {
                ready++;
            }
        }

        if (ready > 0) {
            return ready;
        }

        // Check timeout
        if (timeout == 0) {
            return 0;  // Non-blocking
        }
        if (timeout > 0 && timer_get_ticks() >= deadline) {
            return 0;  // Timeout
        }

        // Sleep and retry
        task_yield();
    }
}
```

---

## VFS Node Operations

Add polling support to VFS nodes:

```c
// include/vfs.h

typedef struct vfs_ops {
    // Existing operations
    int (*read)(vfs_node_t *node, void *buf, size_t len, uint32_t offset);
    int (*write)(vfs_node_t *node, const void *buf, size_t len, uint32_t offset);
    int (*open)(vfs_node_t *node, uint32_t flags);
    int (*close)(vfs_node_t *node);

    // New polling operations
    int (*has_data)(vfs_node_t *node);      // Is data available to read?
    int (*can_write)(vfs_node_t *node);     // Can we write without blocking?
    int (*has_error)(vfs_node_t *node);     // Is there an error condition?
} vfs_ops_t;
```

### TTY Implementation

```c
// kernel/tty.c

static int tty_has_data(vfs_node_t *node) {
    tty_t *tty = (tty_t *)node->private;
    return tty->input_count > 0;
}

static int tty_can_write(vfs_node_t *node) {
    // TTY can always accept writes
    return 1;
}

vfs_ops_t tty_ops = {
    .read = tty_read,
    .write = tty_write,
    .has_data = tty_has_data,
    .can_write = tty_can_write,
    .has_error = NULL,
};
```

### Pipe Implementation

```c
// kernel/pipe.c

static int pipe_has_data(vfs_node_t *node) {
    pipe_t *pipe = (pipe_t *)node->private;
    return pipe->count > 0;
}

static int pipe_can_write(vfs_node_t *node) {
    pipe_t *pipe = (pipe_t *)node->private;
    return pipe->count < PIPE_BUFFER_SIZE;
}

static int pipe_has_error(vfs_node_t *node) {
    pipe_t *pipe = (pipe_t *)node->private;
    // Error if write end closed and buffer empty
    return (pipe->writers == 0 && pipe->count == 0);
}
```

---

## Example Usage

### select() Example

```c
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    fd_set readfds;
    struct timeval tv;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

        if (ret == -1) {
            perror("select");
            break;
        } else if (ret == 0) {
            printf("Timeout, no input\n");
        } else {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                char buf[256];
                int n = read(STDIN_FILENO, buf, sizeof(buf));
                printf("Read %d bytes\n", n);
            }
        }
    }

    return 0;
}
```

### poll() Example

```c
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = some_pipe_fd;
    fds[1].events = POLLIN;

    while (1) {
        int ret = poll(fds, 2, 5000);  // 5 second timeout

        if (ret == -1) {
            perror("poll");
            break;
        } else if (ret == 0) {
            printf("Timeout\n");
            continue;
        }

        if (fds[0].revents & POLLIN) {
            printf("Data on stdin\n");
        }

        if (fds[1].revents & POLLIN) {
            printf("Data on pipe\n");
        }

        if (fds[1].revents & POLLHUP) {
            printf("Pipe closed\n");
            break;
        }
    }

    return 0;
}
```

---

## Efficient Implementation (Future)

### Wait Queues

Instead of busy-polling, use wait queues:

```c
typedef struct wait_queue {
    task_t *head;
    task_t *tail;
} wait_queue_t;

void wait_queue_add(wait_queue_t *wq, task_t *task) {
    task->next_waiting = NULL;
    if (wq->tail) {
        wq->tail->next_waiting = task;
    } else {
        wq->head = task;
    }
    wq->tail = task;
    task->state = TASK_WAITING;
}

void wait_queue_wake(wait_queue_t *wq) {
    if (wq->head) {
        task_t *task = wq->head;
        wq->head = task->next_waiting;
        if (!wq->head) wq->tail = NULL;
        task->state = TASK_READY;
        scheduler_add(task);
    }
}
```

### Per-FD Wait Queues

```c
typedef struct vfs_node {
    // ... existing fields ...
    wait_queue_t read_wait;    // Tasks waiting to read
    wait_queue_t write_wait;   // Tasks waiting to write
} vfs_node_t;

// In read():
if (!has_data(node) && !(flags & O_NONBLOCK)) {
    wait_queue_add(&node->read_wait, current_task);
    schedule();  // Block until woken
}

// In write() when data arrives:
wait_queue_wake(&node->read_wait);
```

---

## Syscall Numbers

```c
#define SYS_SELECT  100
#define SYS_POLL    101
#define SYS_PSELECT 102  // Future: with signal mask
#define SYS_PPOLL   103  // Future: with signal mask
```

---

## Testing

### Test Non-blocking Read

```c
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main() {
    // Set stdin non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char buf[10];
    int ret = read(STDIN_FILENO, buf, 10);

    if (ret == -1 && errno == EAGAIN) {
        printf("No data available (correct)\n");
    }

    // Restore blocking
    fcntl(STDIN_FILENO, F_SETFL, flags);
    return 0;
}
```

### Test select() with Pipe

```c
#include <sys/select.h>
#include <unistd.h>

int main() {
    int pipefd[2];
    pipe(pipefd);

    if (fork() == 0) {
        // Child: write after delay
        close(pipefd[0]);
        sleep(2);
        write(pipefd[1], "hello", 5);
        close(pipefd[1]);
        _exit(0);
    }

    // Parent: select on pipe
    close(pipefd[1]);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(pipefd[0], &readfds);

    printf("Waiting for data...\n");
    int ret = select(pipefd[0] + 1, &readfds, NULL, NULL, NULL);
    printf("select returned %d\n", ret);

    if (FD_ISSET(pipefd[0], &readfds)) {
        char buf[10];
        read(pipefd[0], buf, 10);
        printf("Got: %s\n", buf);
    }

    close(pipefd[0]);
    return 0;
}
```

---

## Complexity Estimate

| Component | Lines | Difficulty |
|-----------|-------|------------|
| O_NONBLOCK support | ~50 | Easy |
| fcntl() F_SETFL | ~30 | Easy |
| VFS has_data/can_write | ~100 | Easy |
| select() syscall | ~100 | Medium |
| poll() syscall | ~80 | Medium |
| Wait queues (efficient) | ~150 | Medium |
| **Total** | **~510** | Medium |

---

## See Also

- [Chapter 32: Future Enhancements](../book/32_future.md)
- [networking.md](networking.md) - select/poll essential for network I/O
- [threading_roadmap.md](threading_roadmap.md) - Threading also benefits from I/O multiplexing
