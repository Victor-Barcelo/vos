# Threading Implementation Roadmap for VOS

This document outlines options for adding threading support to VOS.

---

## Current State

VOS currently supports:
- Multi-process with fork/exec
- Single-threaded processes
- Cooperative context switching between processes
- Timer-based preemption

Missing:
- User-space threads (pthreads)
- Kernel threads
- Thread synchronization primitives

---

## Phase 1: Cooperative User Threads (Fibers)

### Overview
Lightweight cooperative threads running in user space. No kernel changes required.

### Implementation

```c
// fiber.h - User-space cooperative threads
#include <setjmp.h>

#define FIBER_STACK_SIZE 8192
#define MAX_FIBERS 16

typedef struct fiber {
    jmp_buf context;
    char *stack;
    int id;
    int state;  // 0=free, 1=ready, 2=running, 3=blocked
    void (*entry)(void *);
    void *arg;
} fiber_t;

static fiber_t fibers[MAX_FIBERS];
static int current_fiber = 0;

// Create a new fiber
int fiber_create(void (*func)(void *), void *arg) {
    for (int i = 0; i < MAX_FIBERS; i++) {
        if (fibers[i].state == 0) {
            fibers[i].stack = malloc(FIBER_STACK_SIZE);
            fibers[i].entry = func;
            fibers[i].arg = arg;
            fibers[i].state = 1;  // Ready
            fibers[i].id = i;
            // Setup stack pointer in context
            return i;
        }
    }
    return -1;
}

// Yield to next fiber
void fiber_yield(void) {
    int prev = current_fiber;
    // Find next ready fiber
    do {
        current_fiber = (current_fiber + 1) % MAX_FIBERS;
    } while (fibers[current_fiber].state != 1 &&
             current_fiber != prev);

    if (current_fiber != prev) {
        if (setjmp(fibers[prev].context) == 0) {
            longjmp(fibers[current_fiber].context, 1);
        }
    }
}
```

### Advantages
- No kernel modifications
- Low overhead
- Simple to implement
- Works with existing VOS

### Disadvantages
- Requires explicit yield() calls
- Blocking syscalls block all fibers
- No true parallelism

### Libraries
| Library | URL | Notes |
|---------|-----|-------|
| **Protothreads** | https://github.com/zserge/pt | Stackless, ~20 lines |
| **minicoro** | https://github.com/edubart/minicoro | Single-header coroutines |
| **libaco** | https://github.com/hnes/libaco | Fast asymmetric coroutines |
| **libco** | https://byuu.org/library/libco | Lightweight, portable |

---

## Phase 2: Kernel Thread Support

### Required Kernel Changes

1. **Thread Control Block (TCB)**
```c
typedef struct thread {
    uint32_t tid;           // Thread ID
    task_t *task;           // Parent process
    uint32_t *stack;        // Kernel stack
    uint32_t *user_stack;   // User stack
    registers_t regs;       // Saved registers
    int state;              // RUNNING, READY, BLOCKED
    int exit_code;
    struct thread *next;    // Scheduler list
} thread_t;
```

2. **Per-Task Thread List**
```c
typedef struct task {
    // ... existing fields ...
    thread_t *threads;      // List of threads
    thread_t *main_thread;  // Primary thread
    int thread_count;
} task_t;
```

3. **New Syscalls**
```c
#define SYS_THREAD_CREATE    80
#define SYS_THREAD_EXIT      81
#define SYS_THREAD_JOIN      82
#define SYS_THREAD_YIELD     83
#define SYS_THREAD_SELF      84

// Create new thread
// Returns TID or negative error
int sys_thread_create(void (*func)(void*), void *arg, void *stack);

// Exit current thread
void sys_thread_exit(int code);

// Wait for thread to finish
int sys_thread_join(int tid, int *retval);
```

4. **Scheduler Updates**
```c
void scheduler_tick(void) {
    // Save current thread state
    current_thread->regs = saved_regs;

    // Find next runnable thread (across all tasks)
    thread_t *next = find_next_ready_thread();

    if (next != current_thread) {
        current_thread = next;
        current_task = next->task;
        switch_context(&next->regs);
    }
}
```

5. **Address Space Sharing**
- Threads share: code, data, heap, open files
- Threads have separate: stack, registers

---

## Phase 3: Synchronization Primitives

### Spinlocks (Kernel)
```c
typedef struct {
    volatile int locked;
} spinlock_t;

void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ volatile("pause");
    }
}

void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}
```

### Mutexes (User)
```c
typedef struct {
    volatile int locked;
    int owner_tid;
    thread_t *wait_queue;
} mutex_t;

void mutex_lock(mutex_t *m) {
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        // Add to wait queue and block
        sys_thread_block(&m->wait_queue);
    }
    m->owner_tid = sys_thread_self();
}

void mutex_unlock(mutex_t *m) {
    m->owner_tid = 0;
    __sync_lock_release(&m->locked);
    // Wake one waiter
    sys_thread_wake(&m->wait_queue);
}
```

### Condition Variables
```c
typedef struct {
    thread_t *wait_queue;
} cond_t;

void cond_wait(cond_t *c, mutex_t *m) {
    mutex_unlock(m);
    sys_thread_block(&c->wait_queue);
    mutex_lock(m);
}

void cond_signal(cond_t *c) {
    sys_thread_wake_one(&c->wait_queue);
}

void cond_broadcast(cond_t *c) {
    sys_thread_wake_all(&c->wait_queue);
}
```

### Semaphores
```c
typedef struct {
    volatile int count;
    thread_t *wait_queue;
} semaphore_t;

void sem_wait(semaphore_t *s) {
    while (__sync_fetch_and_sub(&s->count, 1) <= 0) {
        __sync_fetch_and_add(&s->count, 1);
        sys_thread_block(&s->wait_queue);
    }
}

void sem_post(semaphore_t *s) {
    __sync_fetch_and_add(&s->count, 1);
    sys_thread_wake_one(&s->wait_queue);
}
```

---

## Phase 4: POSIX pthreads API

### Core Functions
```c
// Thread creation/management
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
void pthread_exit(void *retval);
int pthread_join(pthread_t thread, void **retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_detach(pthread_t thread);

// Mutexes
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);

// Condition variables
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
```

### Implementation in Newlib
Add to `user/newlib_syscalls.c`:
```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start)(void *), void *arg) {
    void *stack = malloc(PTHREAD_STACK_SIZE);
    if (!stack) return ENOMEM;

    int tid = syscall3(SYS_THREAD_CREATE, (int)start, (int)arg, (int)stack);
    if (tid < 0) {
        free(stack);
        return -tid;
    }

    *thread = tid;
    return 0;
}
```

---

## Reference Implementations

### xv6 Threading
- Simple kernel threads
- Spinlock-based synchronization
- Good learning resource
- https://github.com/mit-pdos/xv6-public

### Pintos (Stanford)
- User-space threads project
- Full synchronization primitives
- Well-documented
- https://pintos-os.org/

### ToaruOS
- Full pthread implementation
- Modern hobbyist OS
- https://github.com/klange/toaruos

### SerenityOS
- Kernel and user threads
- Lock debugging
- https://github.com/SerenityOS/serenity

---

## Testing Threads

### Basic Test
```c
#include <pthread.h>
#include <stdio.h>

void *thread_func(void *arg) {
    int id = (int)arg;
    printf("Thread %d running\n", id);
    return NULL;
}

int main() {
    pthread_t threads[4];

    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void*)i);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads complete\n");
    return 0;
}
```

### Producer-Consumer
```c
#include <pthread.h>

#define BUFFER_SIZE 10

int buffer[BUFFER_SIZE];
int count = 0;
pthread_mutex_t mutex;
pthread_cond_t not_empty, not_full;

void *producer(void *arg) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&mutex);
        while (count == BUFFER_SIZE)
            pthread_cond_wait(&not_full, &mutex);
        buffer[count++] = i;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&mutex);
        while (count == 0)
            pthread_cond_wait(&not_empty, &mutex);
        int val = buffer[--count];
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
```

---

## Complexity Estimates

| Phase | Kernel Changes | User Changes | Difficulty |
|-------|---------------|--------------|------------|
| Phase 1: Fibers | None | ~200 lines | Easy |
| Phase 2: Kernel Threads | ~500 lines | ~100 lines | Medium |
| Phase 3: Sync Primitives | ~300 lines | ~200 lines | Medium |
| Phase 4: pthreads | ~100 lines | ~400 lines | Medium |

Total: ~1800 lines of code for full pthreads support.

---

## Recommended Order

1. **Start with fibers** - Immediate benefit, no risk
2. **Add kernel thread syscalls** - Foundation for pthreads
3. **Implement spinlocks** - Needed for SMP anyway
4. **Add mutexes and condvars** - Most common sync
5. **Build pthread wrappers** - POSIX compatibility
6. **Add advanced features** - RW locks, barriers, TLS

---

## See Also

- [Chapter 32: Future Enhancements](../book/32_future.md)
- [networking.md](networking.md) - Threading useful for async I/O
- [system_libraries.md](system_libraries.md) - Coroutine libraries
