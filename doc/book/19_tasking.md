# Chapter 19: Tasking and Scheduling

This is where VOS moves from "a kernel" to "a multitasking operating system". Tasking enables running multiple programs concurrently.

## Task Structure

```c
typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_ZOMBIE,
    TASK_STOPPED
} task_state_t;

typedef struct task {
    uint32_t pid;
    uint32_t ppid;              // Parent PID
    task_state_t state;

    // CPU state
    uint32_t esp;               // Saved stack pointer
    uint32_t kernel_stack_top;  // Top of kernel stack
    uint32_t *page_directory;   // Address space

    // Memory management
    uint32_t brk;               // Program break (heap)
    vm_area_t *vm_areas;        // Memory regions

    // File descriptors
    file_desc_t *fds[MAX_FDS];

    // Current directory
    char cwd[256];

    // Signal handling
    uint32_t pending_signals;
    uint32_t blocked_signals;
    sighandler_t signal_handlers[32];

    // Terminal
    struct termios termios;
    pid_t pgrp;                 // Process group
    pid_t session;

    // Exit status
    int exit_code;

    // Scheduling
    struct task *next;
    struct task *parent;
    struct task *children;
    struct task *sibling;
} task_t;
```

## Task Creation

### Kernel Stack

Each task needs its own kernel stack for handling interrupts and syscalls:

```c
task_t* task_create(void) {
    task_t *task = kcalloc(1, sizeof(task_t));

    task->pid = next_pid++;
    task->state = TASK_READY;

    // Allocate kernel stack (8 KB)
    uint32_t stack = (uint32_t)kmalloc(8192);
    task->kernel_stack_top = stack + 8192;

    // Initialize file descriptors (inherit stdio)
    task->fds[0] = current_task->fds[0];  // stdin
    task->fds[1] = current_task->fds[1];  // stdout
    task->fds[2] = current_task->fds[2];  // stderr

    // Copy cwd
    strcpy(task->cwd, current_task->cwd);

    // Default signal handlers
    for (int i = 0; i < 32; i++) {
        task->signal_handlers[i] = SIG_DFL;
    }

    // Add to task list
    add_to_ready_queue(task);

    return task;
}
```

### Setting Up Initial Stack Frame

```c
void task_setup_stack(task_t *task, uint32_t entry, uint32_t user_stack) {
    uint32_t *stack = (uint32_t *)task->kernel_stack_top;

    // Build iret frame for user mode entry
    *(--stack) = 0x23;          // SS (user data selector)
    *(--stack) = user_stack;    // ESP
    *(--stack) = 0x202;         // EFLAGS (IF=1)
    *(--stack) = 0x1B;          // CS (user code selector)
    *(--stack) = entry;         // EIP

    // Build interrupt frame (as if we interrupted at entry)
    *(--stack) = 0;             // Error code
    *(--stack) = 0;             // Interrupt number

    // General registers
    *(--stack) = 0;             // EAX
    *(--stack) = 0;             // ECX
    *(--stack) = 0;             // EDX
    *(--stack) = 0;             // EBX
    *(--stack) = 0;             // ESP (ignored)
    *(--stack) = 0;             // EBP
    *(--stack) = 0;             // ESI
    *(--stack) = 0;             // EDI

    // Segment registers
    *(--stack) = 0x23;          // DS
    *(--stack) = 0x23;          // ES
    *(--stack) = 0x23;          // FS
    *(--stack) = 0x23;          // GS

    task->esp = (uint32_t)stack;
}
```

## Context Switching

Context switching happens through the interrupt mechanism:

```c
static task_t *current_task = NULL;
static task_t *ready_queue = NULL;

interrupt_frame_t* schedule(interrupt_frame_t *frame) {
    if (!current_task) return frame;

    // Save current task's state
    current_task->esp = (uint32_t)frame;

    // Find next ready task
    task_t *next = get_next_ready_task();
    if (!next || next == current_task) {
        return frame;  // No switch needed
    }

    // Switch to next task
    current_task = next;
    next->state = TASK_RUNNING;

    // Update TSS with new kernel stack
    tss_set_kernel_stack(next->kernel_stack_top);

    // Switch page directory if different
    if (next->page_directory != get_page_directory()) {
        switch_page_directory(next->page_directory);
    }

    // Return new task's saved frame
    return (interrupt_frame_t *)next->esp;
}
```

### How Context Switch Works

1. Timer interrupt fires (IRQ0)
2. CPU saves state on current task's kernel stack
3. Assembly stub saves remaining registers
4. C handler calls `schedule()`
5. `schedule()` selects next task
6. Returns pointer to next task's saved frame
7. Assembly stub loads registers from that frame
8. `iret` returns to next task's code

## Round-Robin Scheduler

```c
static task_t* get_next_ready_task(void) {
    task_t *start = current_task ? current_task->next : ready_queue;
    task_t *task = start;

    do {
        if (task->state == TASK_READY) {
            return task;
        }
        task = task->next ? task->next : ready_queue;
    } while (task != start);

    // No ready tasks, return idle task
    return idle_task;
}
```

### Time Slicing

```c
#define SCHEDULER_QUANTUM 10  // 10 ms at 1000 Hz timer

static uint32_t slice_remaining = SCHEDULER_QUANTUM;

interrupt_frame_t* tasking_on_timer_tick(interrupt_frame_t *frame) {
    if (!current_task) return frame;

    slice_remaining--;

    if (slice_remaining == 0) {
        slice_remaining = SCHEDULER_QUANTUM;

        if (current_task->state == TASK_RUNNING) {
            current_task->state = TASK_READY;
        }

        return schedule(frame);
    }

    return frame;
}
```

## fork() Implementation

VOS supports UNIX-style `fork()`:

```c
int32_t sys_fork(interrupt_frame_t *frame) {
    task_t *parent = current_task;

    // Create child task
    task_t *child = task_create();
    child->ppid = parent->pid;
    child->parent = parent;

    // Copy address space
    child->page_directory = clone_page_directory(parent->page_directory);

    // Copy memory regions
    child->vm_areas = clone_vm_areas(parent->vm_areas);
    child->brk = parent->brk;

    // Copy file descriptors
    for (int i = 0; i < MAX_FDS; i++) {
        if (parent->fds[i]) {
            child->fds[i] = dup_fd(parent->fds[i]);
        }
    }

    // Copy signal handlers
    memcpy(child->signal_handlers, parent->signal_handlers,
           sizeof(parent->signal_handlers));

    // Copy the parent's stack frame for the child
    memcpy((void *)child->esp, frame, sizeof(interrupt_frame_t));

    // Child returns 0
    interrupt_frame_t *child_frame = (interrupt_frame_t *)child->esp;
    child_frame->eax = 0;

    // Parent returns child's PID
    return child->pid;
}
```

## Task States

```
           fork()
              |
              v
+--------> READY <--------+
|            |            |
|            | scheduled  |
|            v            |
|        RUNNING -------->+ (preempted)
|            |
|            | wait/block
|            v
|        BLOCKED -------->+ (event)
|            |
|            | exit()
|            v
|         ZOMBIE
|            |
|            | parent wait()
|            v
|       (destroyed)
```

### State Transitions

```c
void task_block(task_t *task) {
    task->state = TASK_BLOCKED;
    schedule(NULL);  // Yield CPU
}

void task_unblock(task_t *task) {
    task->state = TASK_READY;
}

void task_exit(int status) {
    current_task->exit_code = status;
    current_task->state = TASK_ZOMBIE;

    // Notify parent
    if (current_task->parent) {
        task_signal(current_task->parent, SIGCHLD);
    }

    // Orphan children to init
    reparent_children(current_task);

    // Never returns
    schedule(NULL);
}
```

## Wait and Waitpid

```c
int32_t sys_waitpid(pid_t pid, int *status, int options) {
    task_t *parent = current_task;

    while (1) {
        // Find matching zombie child
        task_t *child = find_zombie_child(parent, pid);

        if (child) {
            int exit_status = child->exit_code;

            if (status) {
                *status = (exit_status & 0xFF) << 8;
            }

            pid_t child_pid = child->pid;
            destroy_task(child);

            return child_pid;
        }

        // No zombie child
        if (options & WNOHANG) {
            return 0;  // Don't block
        }

        // Block until SIGCHLD
        task_block(parent);
    }
}
```

## Yield

```c
void sys_yield(void) {
    current_task->state = TASK_READY;
    schedule(NULL);
}
```

## Task List Management

```c
static void add_to_ready_queue(task_t *task) {
    task->next = ready_queue;
    ready_queue = task;
}

static void remove_from_ready_queue(task_t *task) {
    task_t **pp = &ready_queue;
    while (*pp) {
        if (*pp == task) {
            *pp = task->next;
            return;
        }
        pp = &(*pp)->next;
    }
}

task_t* find_task_by_pid(pid_t pid) {
    task_t *task = ready_queue;
    while (task) {
        if (task->pid == pid) {
            return task;
        }
        task = task->next;
    }
    return NULL;
}
```

## Idle Task

```c
static void idle_task_func(void) {
    while (1) {
        hlt();  // Low power wait
    }
}

void tasking_init(void) {
    // Create boot task (current execution context)
    boot_task = kcalloc(1, sizeof(task_t));
    boot_task->pid = 0;
    boot_task->state = TASK_RUNNING;
    strcpy(boot_task->cwd, "/");

    current_task = boot_task;

    // Create idle task
    idle_task = task_create();
    idle_task->pid = 1;
    task_setup_kernel_task(idle_task, idle_task_func);
}
```

## Summary

VOS tasking provides:

1. **Task structures** with complete process state
2. **Context switching** via interrupt frames
3. **Round-robin scheduling** with time slicing
4. **fork()** for process creation
5. **wait/waitpid** for process synchronization
6. **Task states** for proper lifecycle management

This forms the foundation for running multiple programs concurrently.

---

*Previous: [Chapter 18: FAT16 Filesystem](18_fat16.md)*
*Next: [Chapter 20: User Mode and ELF](20_usermode.md)*
