#include "task.h"
#include "kheap.h"
#include "string.h"
#include "gdt.h"
#include "timer.h"
#include "io.h"
#include "paging.h"
#include "pmm.h"
#include "elf.h"
#include "usercopy.h"
#include "kerrno.h"
#include "screen.h"
#include "vfs.h"
#include "keyboard.h"

#define KSTACK_SIZE (16u * 1024u)
#define TASK_NAME_LEN 15
#define TASK_MAX_SCAN 256
#define KSTACK_REGION_BASE 0xF0000000u
#define TASK_MAX_FDS 64

// Minimal termios/ioctl support for userland TTY programs (linenoise, etc.).
#define VOS_NCCS 32
#define VOS_TTY_LINE_MAX 256u

// ioctl request numbers (Linux-compatible values where practical).
enum {
    VOS_TCGETS     = 0x5401u,
    VOS_TCSETS     = 0x5402u,
    VOS_TCSETSW    = 0x5403u,
    VOS_TCSETSF    = 0x5404u,
    VOS_TIOCGPGRP  = 0x540Fu,
    VOS_TIOCSPGRP  = 0x5410u,
    VOS_TIOCGWINSZ = 0x5413u,
};

// termios c_lflag bits (subset).
enum {
    VOS_ISIG   = 0x00000001u,
    VOS_ICANON = 0x00000002u,
    VOS_ECHO   = 0x00000008u,
    VOS_IEXTEN = 0x00008000u,
};

// termios c_iflag bits (subset).
enum {
    VOS_ICRNL  = 0x00000100u,
};

// termios c_cflag bits (subset).
enum {
    VOS_CS8    = 0x00000030u,
};

// termios c_cc indices (subset; Linux-compatible).
enum {
    VOS_VINTR  = 0,
    VOS_VEOF   = 4,
    VOS_VERASE = 2,
    VOS_VTIME  = 5,
    VOS_VMIN   = 6,
};

typedef struct vos_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_cc[VOS_NCCS];
    uint32_t c_ispeed;
    uint32_t c_ospeed;
} vos_termios_t;

typedef struct vos_winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} vos_winsize_t;

typedef enum {
    FD_KIND_FREE = 0,
    FD_KIND_STDIN = 1,
    FD_KIND_STDOUT = 2,
    FD_KIND_STDERR = 3,
    FD_KIND_VFS = 4,
    FD_KIND_PIPE = 5,
} fd_kind_t;

typedef struct pipe_obj pipe_obj_t;

typedef struct fd_entry {
    fd_kind_t kind;
    vfs_handle_t* handle;
    pipe_obj_t* pipe;
    bool pipe_write_end;
    uint8_t pending[8];
    uint8_t pending_len;
    uint8_t pending_off;
} fd_entry_t;

typedef struct task {
    uint32_t id;
    uint32_t esp;            // saved stack pointer (points to interrupt frame)
    uint32_t kstack_top;     // top of kernel stack (for TSS.esp0)
    uint32_t* page_directory;
    bool user;
    uint32_t user_brk;
    uint32_t user_brk_min;
    fd_entry_t fds[TASK_MAX_FDS];
    char cwd[VFS_PATH_MAX];
    vos_termios_t tty;
    uint8_t tty_line[VOS_TTY_LINE_MAX];
    uint16_t tty_line_len;
    uint16_t tty_line_off;
    bool tty_line_ready;
    task_state_t state;
    uint32_t wake_tick;
    uint32_t wait_pid;
    int32_t exit_code;
    bool kill_pending;
    int32_t kill_exit_code;
    uint32_t cpu_ticks;
    char name[TASK_NAME_LEN + 1];
    struct task* next;
} task_t;

extern uint8_t stack_top;

static task_t* current_task = NULL;
static bool enabled = false;
static uint32_t next_id = 1;
static uint32_t tick_div = 0;
static uint32_t next_kstack_region = KSTACK_REGION_BASE;
static uint32_t tty_foreground_pid = 0;

static task_t* task_find_by_pid(uint32_t pid) {
    if (!current_task || pid == 0) {
        return NULL;
    }

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->id == pid) {
            return t;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }
    return NULL;
}

static uint32_t* push32(uint32_t* sp, uint32_t v) {
    sp--;
    *sp = v;
    return sp;
}

// -----------------------------
// Pipes (anonymous, in-kernel)
// -----------------------------

#define PIPE_BUF_SIZE 4096u

struct pipe_obj {
    uint8_t buf[PIPE_BUF_SIZE];
    uint32_t rpos;
    uint32_t wpos;
    uint32_t used;
    uint32_t readers;
    uint32_t writers;
};

static void wait_for_event(void) {
    bool were_enabled = irq_are_enabled();
    if (!were_enabled) {
        sti();
    }
    hlt();
    if (!were_enabled) {
        cli();
    }
}

static pipe_obj_t* pipe_create(void) {
    pipe_obj_t* p = (pipe_obj_t*)kmalloc(sizeof(*p));
    if (!p) {
        return NULL;
    }
    memset(p, 0, sizeof(*p));
    p->readers = 1;
    p->writers = 1;
    return p;
}

static void pipe_ref(pipe_obj_t* p, bool write_end) {
    if (!p) {
        return;
    }
    uint32_t f = irq_save();
    if (write_end) {
        p->writers++;
    } else {
        p->readers++;
    }
    irq_restore(f);
}

static void pipe_unref(pipe_obj_t* p, bool write_end) {
    if (!p) {
        return;
    }
    bool free_now = false;
    uint32_t f = irq_save();
    if (write_end) {
        if (p->writers) {
            p->writers--;
        }
    } else {
        if (p->readers) {
            p->readers--;
        }
    }
    free_now = (p->readers == 0 && p->writers == 0);
    irq_restore(f);
    if (free_now) {
        kfree(p);
    }
}

static uint32_t pipe_read_some(pipe_obj_t* p, uint8_t* out, uint32_t max) {
    if (!p || !out || max == 0) {
        return 0;
    }

    uint32_t f = irq_save();
    uint32_t avail = p->used;
    if (avail == 0) {
        irq_restore(f);
        return 0;
    }
    uint32_t n = max;
    if (n > avail) {
        n = avail;
    }

    for (uint32_t i = 0; i < n; i++) {
        out[i] = p->buf[p->rpos];
        p->rpos = (p->rpos + 1u) % PIPE_BUF_SIZE;
    }
    p->used -= n;
    irq_restore(f);
    return n;
}

static int32_t pipe_write_some(pipe_obj_t* p, const uint8_t* src, uint32_t len, uint32_t* out_written) {
    if (out_written) {
        *out_written = 0;
    }
    if (!p || (!src && len != 0)) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }

    uint32_t f = irq_save();
    if (p->readers == 0) {
        irq_restore(f);
        return -EPIPE;
    }

    uint32_t space = PIPE_BUF_SIZE - p->used;
    if (space == 0) {
        irq_restore(f);
        return 0;
    }
    uint32_t n = len;
    if (n > space) {
        n = space;
    }

    for (uint32_t i = 0; i < n; i++) {
        p->buf[p->wpos] = src[i];
        p->wpos = (p->wpos + 1u) % PIPE_BUF_SIZE;
    }
    p->used += n;
    irq_restore(f);

    if (out_written) {
        *out_written = n;
    }
    return 0;
}

static void fd_init(task_t* t) {
    if (!t) {
        return;
    }
    memset(t->fds, 0, sizeof(t->fds));
    if (TASK_MAX_FDS > 0) t->fds[0].kind = FD_KIND_STDIN;
    if (TASK_MAX_FDS > 1) t->fds[1].kind = FD_KIND_STDOUT;
    if (TASK_MAX_FDS > 2) t->fds[2].kind = FD_KIND_STDERR;
}

static void cwd_init(task_t* t) {
    if (!t) {
        return;
    }
    t->cwd[0] = '/';
    t->cwd[1] = '\0';
}

static void tty_init(task_t* t) {
    if (!t) {
        return;
    }

    memset(&t->tty, 0, sizeof(t->tty));
    t->tty.c_iflag = VOS_ICRNL;
    t->tty.c_oflag = 0;
    t->tty.c_cflag = VOS_CS8;
    t->tty.c_lflag = VOS_ISIG | VOS_ICANON | VOS_ECHO | VOS_IEXTEN;
    t->tty.c_cc[VOS_VINTR] = 0x03u;   // ^C
    t->tty.c_cc[VOS_VEOF] = 0x04u;    // ^D
    t->tty.c_cc[VOS_VERASE] = 0x08u;  // backspace
    t->tty.c_cc[VOS_VTIME] = 0u;
    t->tty.c_cc[VOS_VMIN] = 1u;

    t->tty_line_len = 0;
    t->tty_line_off = 0;
    t->tty_line_ready = false;
}

static void task_close_fds(task_t* t) {
    if (!t) {
        return;
    }

    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        fd_entry_t* ent = &t->fds[fd];

        if (ent->kind == FD_KIND_VFS && ent->handle) {
            vfs_handle_t* h = ent->handle;
            ent->kind = FD_KIND_FREE;
            ent->handle = NULL;
            ent->pipe = NULL;
            ent->pipe_write_end = false;
            ent->pending_len = 0;
            ent->pending_off = 0;
            (void)vfs_close(h);
            continue;
        }

        if (ent->kind == FD_KIND_PIPE && ent->pipe) {
            pipe_obj_t* p = ent->pipe;
            bool we = ent->pipe_write_end;
            ent->kind = FD_KIND_FREE;
            ent->handle = NULL;
            ent->pipe = NULL;
            ent->pipe_write_end = false;
            ent->pending_len = 0;
            ent->pending_off = 0;
            pipe_unref(p, we);
            continue;
        }

        if (ent->kind != FD_KIND_FREE) {
            ent->kind = FD_KIND_FREE;
            ent->handle = NULL;
            ent->pipe = NULL;
            ent->pipe_write_end = false;
            ent->pending_len = 0;
            ent->pending_off = 0;
        }
    }
}

static void task_set_name(task_t* t, const char* name) {
    if (!t) {
        return;
    }
    if (!name) {
        name = "";
    }
    strncpy(t->name, name, TASK_NAME_LEN);
    t->name[TASK_NAME_LEN] = '\0';
}

static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static bool kstack_alloc(uint32_t* out_stack_top) {
    if (!out_stack_top) {
        return false;
    }

    uint32_t region_base = next_kstack_region;
    uint32_t stack_bottom = region_base + PAGE_SIZE;          // guard page below
    uint32_t stack_top = stack_bottom + KSTACK_SIZE;

    if (stack_top < stack_bottom) {
        return false;
    }

    next_kstack_region = stack_top;

    paging_prepare_range(stack_bottom, KSTACK_SIZE, PAGE_PRESENT | PAGE_RW);

    for (uint32_t va = stack_bottom; va < stack_top; va += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return false;
        }
        paging_map_page(va, frame, PAGE_PRESENT | PAGE_RW);
        memset((void*)va, 0, PAGE_SIZE);
    }

    *out_stack_top = stack_top;
    return true;
}

static task_t* task_create_kernel(void (*entry)(void), const char* name) {
    uint32_t stack_top_addr = 0;
    if (!kstack_alloc(&stack_top_addr)) {
        return NULL;
    }

    uint32_t* sp = (uint32_t*)stack_top_addr;

    // iret frame (ring 0): eip, cs, eflags
    sp = push32(sp, 0x202u);            // EFLAGS (IF=1)
    sp = push32(sp, 0x08u);             // CS (kernel code)
    sp = push32(sp, (uint32_t)entry);   // EIP

    // err_code + int_no
    sp = push32(sp, 0u);
    sp = push32(sp, 0u);

    // pusha regs (eax..edi)
    sp = push32(sp, 0u); // eax
    sp = push32(sp, 0u); // ecx
    sp = push32(sp, 0u); // edx
    sp = push32(sp, 0u); // ebx
    sp = push32(sp, 0u); // esp (as pushed by pusha)
    sp = push32(sp, 0u); // ebp
    sp = push32(sp, 0u); // esi
    sp = push32(sp, 0u); // edi

    // seg regs (ds, es, fs, gs)
    sp = push32(sp, 0x10u); // ds
    sp = push32(sp, 0x10u); // es
    sp = push32(sp, 0x10u); // fs
    sp = push32(sp, 0x10u); // gs

    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) {
        return NULL;
    }
    memset(t, 0, sizeof(*t));
    t->id = ++next_id;
    t->esp = (uint32_t)sp;
    t->kstack_top = stack_top_addr;
    t->page_directory = paging_kernel_directory();
    t->user = false;
    t->user_brk = 0;
    t->user_brk_min = 0;
    fd_init(t);
    cwd_init(t);
    tty_init(t);
    t->state = TASK_STATE_RUNNABLE;
    t->wake_tick = 0;
    t->wait_pid = 0;
    t->exit_code = 0;
    t->cpu_ticks = 0;
    task_set_name(t, name);
    t->next = NULL;
    return t;
}

static task_t* task_create_user(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk, const char* name) {
    uint32_t stack_top_addr = 0;
    if (!kstack_alloc(&stack_top_addr)) {
        return NULL;
    }

    uint32_t* sp = (uint32_t*)stack_top_addr;

    // iret frame (ring 3): eip, cs, eflags, useresp, ss
    sp = push32(sp, 0x23u);         // SS (user data | RPL3)
    sp = push32(sp, user_esp);      // User ESP
    sp = push32(sp, 0x202u);        // EFLAGS (IF=1)
    sp = push32(sp, 0x1Bu);         // CS (user code | RPL3)
    sp = push32(sp, entry);         // EIP

    // err_code + int_no
    sp = push32(sp, 0u);
    sp = push32(sp, 0u);

    // pusha regs (eax..edi)
    sp = push32(sp, 0u); // eax
    sp = push32(sp, 0u); // ecx
    sp = push32(sp, 0u); // edx
    sp = push32(sp, 0u); // ebx
    sp = push32(sp, 0u); // esp (as pushed by pusha)
    sp = push32(sp, 0u); // ebp
    sp = push32(sp, 0u); // esi
    sp = push32(sp, 0u); // edi

    // seg regs (ds, es, fs, gs) - user data selector
    sp = push32(sp, 0x23u); // ds
    sp = push32(sp, 0x23u); // es
    sp = push32(sp, 0x23u); // fs
    sp = push32(sp, 0x23u); // gs

    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) {
        return NULL;
    }
    memset(t, 0, sizeof(*t));
    t->id = ++next_id;
    t->esp = (uint32_t)sp;
    t->kstack_top = stack_top_addr;
    t->page_directory = page_directory;
    t->user = true;
    t->user_brk = user_brk;
    t->user_brk_min = user_brk;
    fd_init(t);
    cwd_init(t);
    tty_init(t);
    t->state = TASK_STATE_RUNNABLE;
    t->wake_tick = 0;
    t->wait_pid = 0;
    t->exit_code = 0;
    t->cpu_ticks = 0;
    task_set_name(t, name);
    t->next = NULL;
    return t;
}

static void task_append(task_t* t) {
    if (!current_task) {
        current_task = t;
        t->next = t;
        return;
    }

    // Insert after current task (simple round-robin).
    t->next = current_task->next;
    current_task->next = t;
}

uint32_t tasking_current_pid(void) {
    return current_task ? current_task->id : 0;
}

bool tasking_current_should_exit(int32_t* out_exit_code) {
    if (!enabled || !current_task) {
        return false;
    }
    if (!current_task->kill_pending) {
        return false;
    }
    if (out_exit_code) {
        *out_exit_code = current_task->kill_exit_code;
    }
    return true;
}

uint32_t tasking_task_count(void) {
    uint32_t flags = irq_save();

    if (!current_task) {
        irq_restore(flags);
        return 0;
    }

    uint32_t count = 0;
    task_t* t = current_task;
    do {
        count++;
        t = t->next;
        if (!t) {
            break;
        }
    } while (t != current_task && count < TASK_MAX_SCAN);

    irq_restore(flags);
    return count;
}

static void fill_task_info(task_info_t* out, const task_t* t) {
    memset(out, 0, sizeof(*out));
    out->pid = t->id;
    out->user = t->user;
    out->state = t->state;
    out->cpu_ticks = t->cpu_ticks;
    out->exit_code = t->exit_code;
    out->wake_tick = t->wake_tick;
    out->wait_pid = t->wait_pid;
    strncpy(out->name, t->name, sizeof(out->name) - 1u);
    out->name[sizeof(out->name) - 1u] = '\0';

    out->eip = 0;
    out->esp = 0;
    if (t->esp != 0) {
        const interrupt_frame_t* f = (const interrupt_frame_t*)t->esp;
        out->eip = f->eip;
        out->esp = (uint32_t)t->esp;
    }
}

bool tasking_get_task_info(uint32_t index, task_info_t* out) {
    if (!out) {
        return false;
    }

    uint32_t flags = irq_save();

    if (!current_task) {
        irq_restore(flags);
        return false;
    }

    task_t* t = current_task;
    uint32_t i = 0;
    for (;;) {
        if (i == index) {
            fill_task_info(out, t);
            irq_restore(flags);
            return true;
        }
        t = t->next;
        i++;
        if (!t || t == current_task || i >= TASK_MAX_SCAN) {
            break;
        }
    }

    irq_restore(flags);
    return false;
}

static void wake_sleepers(uint32_t now_ticks) {
    if (!current_task) {
        return;
    }

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->state == TASK_STATE_SLEEPING) {
            if ((int32_t)(now_ticks - t->wake_tick) >= 0) {
                t->state = TASK_STATE_RUNNABLE;
                t->wake_tick = 0;
            }
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }
}

static void wake_waiters(uint32_t pid, int32_t exit_code) {
    if (!current_task) {
        return;
    }

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->state == TASK_STATE_WAITING && t->wait_pid == pid) {
            t->state = TASK_STATE_RUNNABLE;
            t->wait_pid = 0;
            if (t->esp) {
                interrupt_frame_t* f = (interrupt_frame_t*)t->esp;
                f->eax = (uint32_t)exit_code;
            }
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }
}

static task_t* pick_next_runnable(task_t* start, const task_t* stop) {
    task_t* t = start;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t || t == stop) {
            return NULL;
        }
        if (t->state == TASK_STATE_RUNNABLE && t->esp != 0) {
            return t;
        }
        t = t->next;
    }
    return NULL;
}

void tasking_init(void) {
    if (current_task) {
        return;
    }

    task_t* boot = (task_t*)kmalloc(sizeof(task_t));
    if (!boot) {
        return;
    }
    memset(boot, 0, sizeof(*boot));
    boot->id = next_id;
    boot->esp = 0;
    boot->kstack_top = (uint32_t)&stack_top;
    boot->page_directory = paging_kernel_directory();
    boot->user = false;
    boot->user_brk = 0;
    boot->user_brk_min = 0;
    fd_init(boot);
    cwd_init(boot);
    tty_init(boot);
    boot->state = TASK_STATE_RUNNABLE;
    boot->wake_tick = 0;
    boot->wait_pid = 0;
    boot->exit_code = 0;
    boot->cpu_ticks = 0;
    task_set_name(boot, "boot");
    boot->next = boot;
    current_task = boot;

    task_t* idle = task_create_kernel(idle_thread, "idle");
    if (idle) {
        task_append(idle);
    }

    // Switch every ~10ms at 1kHz PIT.
    tick_div = 0;
    enabled = true;
}

bool tasking_is_enabled(void) {
    return enabled;
}

interrupt_frame_t* tasking_on_timer_tick(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }

    // Only terminate a task at safe points (when the interrupt happened in ring3).
    if ((frame->cs & 3u) == 3u && current_task->kill_pending) {
        return tasking_exit(frame, current_task->kill_exit_code);
    }

    current_task->cpu_ticks++;

    uint32_t now = timer_get_ticks();
    wake_sleepers(now);

    tick_div++;
    if (tick_div < 10u) {
        return frame;
    }
    tick_div = 0;

    // Save current context.
    current_task->esp = (uint32_t)frame;

    // Find next runnable task.
    task_t* next = pick_next_runnable(current_task->next, current_task);
    if (!next) {
        return frame;
    }

    current_task = next;
    tss_set_kernel_stack(current_task->kstack_top);
    paging_switch_directory(current_task->page_directory);
    return (interrupt_frame_t*)current_task->esp;
}

interrupt_frame_t* tasking_yield(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }

    current_task->esp = (uint32_t)frame;
    task_t* next = pick_next_runnable(current_task->next, current_task);
    if (!next) {
        return frame;
    }

    current_task = next;
    tss_set_kernel_stack(current_task->kstack_top);
    paging_switch_directory(current_task->page_directory);
    return (interrupt_frame_t*)current_task->esp;
}

interrupt_frame_t* tasking_exit(interrupt_frame_t* frame, int32_t exit_code) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }

    task_close_fds(current_task);
    current_task->state = TASK_STATE_ZOMBIE;
    current_task->exit_code = exit_code;
    current_task->esp = (uint32_t)frame;
    wake_waiters(current_task->id, exit_code);
    return tasking_yield(frame);
}

interrupt_frame_t* tasking_sleep_until(interrupt_frame_t* frame, uint32_t wake_tick) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }
    current_task->state = TASK_STATE_SLEEPING;
    current_task->wake_tick = wake_tick;
    current_task->esp = (uint32_t)frame;
    return tasking_yield(frame);
}

interrupt_frame_t* tasking_wait(interrupt_frame_t* frame, uint32_t pid) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }
    if (pid == 0 || pid == current_task->id) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    task_t* target = NULL;
    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->id == pid) {
            target = t;
            break;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }

    if (!target) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    if (target->state == TASK_STATE_ZOMBIE) {
        frame->eax = (uint32_t)target->exit_code;
        return frame;
    }

    current_task->state = TASK_STATE_WAITING;
    current_task->wait_pid = pid;
    current_task->esp = (uint32_t)frame;
    return tasking_yield(frame);
}

int32_t tasking_kill(uint32_t pid, int32_t exit_code) {
    if (!enabled || !current_task) {
        return -EINVAL;
    }
    if (pid == 0) {
        return -EINVAL;
    }

    uint32_t flags = irq_save();

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->id == pid) {
            if (!t->user) {
                irq_restore(flags);
                return -EPERM;
            }
            if (t->state != TASK_STATE_ZOMBIE) {
                // Defer termination to a safe point (syscall entry or timer tick in ring3)
                // to avoid tearing down resources while the target is executing in-kernel.
                t->kill_pending = true;
                t->kill_exit_code = exit_code;
                if (t->state != TASK_STATE_RUNNABLE) {
                    t->state = TASK_STATE_RUNNABLE;
                    t->wake_tick = 0;
                    t->wait_pid = 0;
                }
            }
            irq_restore(flags);
            return 0;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }

    irq_restore(flags);
    return -ESRCH;
}

interrupt_frame_t* tasking_sbrk(interrupt_frame_t* frame, int32_t increment) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }
    if (!current_task->user) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    uint32_t old_brk = current_task->user_brk;
    if (increment == 0) {
        frame->eax = old_brk;
        return frame;
    }

    uint32_t new_brk = old_brk;
    if (increment > 0) {
        uint32_t inc = (uint32_t)increment;
        if (old_brk + inc < old_brk) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        new_brk = old_brk + inc;
    } else {
        uint32_t dec = (uint32_t)(-increment);
        if (dec > old_brk) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        new_brk = old_brk - dec;
    }

    const uint32_t USER_BASE = 0x01000000u;
    const uint32_t USER_STACK_TOP = 0x02000000u;
    const uint32_t USER_STACK_PAGES = 8u;
    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;

    if (new_brk < USER_BASE || new_brk < current_task->user_brk_min || new_brk > stack_guard_bottom) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    uint32_t irq_flags = irq_save();

    if (increment > 0) {
        uint32_t start = (old_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
        uint32_t end = (new_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

        uint32_t va = start;
        for (; va < end; va += PAGE_SIZE) {
            if (va >= stack_guard_bottom) {
                break;
            }

            uint32_t frame_paddr = pmm_alloc_frame();
            if (frame_paddr == 0) {
                break;
            }
            paging_map_page(va, frame_paddr, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            memset((void*)va, 0, PAGE_SIZE);
        }

        if (va != end) {
            // Roll back partial growth.
            for (uint32_t un = start; un < va; un += PAGE_SIZE) {
                uint32_t paddr = 0;
                if (paging_unmap_page(un, &paddr) && paddr) {
                    pmm_free_frame(paddr);
                }
            }
            irq_restore(irq_flags);
            frame->eax = (uint32_t)-1;
            return frame;
        }
    } else {
        uint32_t start = (new_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
        uint32_t end = (old_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

        for (uint32_t va = start; va < end; va += PAGE_SIZE) {
            uint32_t paddr = 0;
            if (paging_unmap_page(va, &paddr) && paddr) {
                pmm_free_frame(paddr);
            }
        }
    }

    current_task->user_brk = new_brk;
    irq_restore(irq_flags);
    frame->eax = old_brk;
    return frame;
}

uint32_t tasking_spawn_user_pid(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk) {
    if (!current_task) {
        return 0;
    }
    if (!page_directory) {
        return 0;
    }

    task_t* t = task_create_user(entry, user_esp, page_directory, user_brk, "user");
    if (!t) {
        return 0;
    }

    // Inherit the caller's current working directory and terminal settings
    // so userland behaves like a normal process tree.
    strncpy(t->cwd, current_task->cwd, sizeof(t->cwd) - 1u);
    t->cwd[sizeof(t->cwd) - 1u] = '\0';
    t->tty = current_task->tty;

    task_append(t);
    return t->id;
}

bool tasking_spawn_user(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk) {
    return tasking_spawn_user_pid(entry, user_esp, page_directory, user_brk) != 0;
}

int32_t tasking_spawn_exec(const char* path, const char* const* argv, uint32_t argc) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    if (argc > 32u) {
        return -EINVAL;
    }

    // Open the file via the mount-aware VFS so /disk works too.
    vfs_handle_t* h = NULL;
    int32_t rc = vfs_open_path(current_task->cwd, path, 0, &h);
    if (rc < 0) {
        return rc;
    }

    vfs_stat_t st;
    rc = vfs_fstat(h, &st);
    if (rc < 0) {
        (void)vfs_close(h);
        return rc;
    }
    if (st.is_dir) {
        (void)vfs_close(h);
        return -EISDIR;
    }
    if (st.size == 0) {
        (void)vfs_close(h);
        return -ENOEXEC;
    }

    uint8_t* image = (uint8_t*)kmalloc(st.size);
    if (!image) {
        (void)vfs_close(h);
        return -ENOMEM;
    }

    uint32_t total = 0;
    while (total < st.size) {
        uint32_t got = 0;
        rc = vfs_read(h, image + total, st.size - total, &got);
        if (rc < 0) {
            kfree(image);
            (void)vfs_close(h);
            return rc;
        }
        if (got == 0) {
            break;
        }
        total += got;
    }
    (void)vfs_close(h);

    if (total != st.size) {
        kfree(image);
        return -EIO;
    }

    char abs[VFS_PATH_MAX];
    rc = vfs_path_resolve(current_task->cwd, path, abs);
    if (rc < 0) {
        kfree(image);
        return rc;
    }

    uint32_t entry = 0;
    uint32_t user_esp = 0;
    uint32_t brk = 0;
    uint32_t* user_dir = paging_create_user_directory();
    if (!user_dir) {
        kfree(image);
        return -ENOMEM;
    }

    const char* kargv[32];
    uint32_t kargc = 0;
    if (argc == 0 || !argv) {
        kargv[kargc++] = abs;
    } else {
        for (uint32_t i = 0; i < argc && kargc < 32u; i++) {
            kargv[kargc++] = argv[i] ? argv[i] : "";
        }
        if (kargc == 0) {
            kargv[kargc++] = abs;
        }
    }

    uint32_t irq_flags = irq_save();
    uint32_t* prev_dir = current_task->page_directory ? current_task->page_directory : paging_kernel_directory();
    paging_switch_directory(user_dir);
    bool ok = elf_load_user_image(image, st.size, &entry, &user_esp, &brk);
    if (ok) {
        ok = elf_setup_user_stack(&user_esp, kargv, kargc);
    }
    paging_switch_directory(prev_dir);
    irq_restore(irq_flags);

    kfree(image);

    if (!ok) {
        return -ENOEXEC;
    }

    uint32_t pid = tasking_spawn_user_pid(entry, user_esp, user_dir, brk);
    if (pid == 0) {
        return -ENOMEM;
    }
    return (int32_t)pid;
}

int32_t tasking_fd_open(const char* path, uint32_t flags) {
    if (!current_task || !path) {
        return -EINVAL;
    }

    vfs_handle_t* h = NULL;
    int32_t rc = vfs_open_path(current_task->cwd, path, flags, &h);
    if (rc < 0) {
        return rc;
    }

    uint32_t irq_flags = irq_save();
    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        if (current_task->fds[fd].kind == FD_KIND_FREE) {
            current_task->fds[fd].kind = FD_KIND_VFS;
            current_task->fds[fd].handle = h;
            current_task->fds[fd].pipe = NULL;
            current_task->fds[fd].pipe_write_end = false;
            irq_restore(irq_flags);
            return fd;
        }
    }
    irq_restore(irq_flags);

    (void)vfs_close(h);
    return -EMFILE;
}

int32_t tasking_fd_close(int32_t fd) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    if (ent->kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -EBADF;
    }

    vfs_handle_t* h = NULL;
    pipe_obj_t* p = NULL;
    bool pipe_we = false;

    if (ent->kind == FD_KIND_VFS) {
        h = ent->handle;
    } else if (ent->kind == FD_KIND_PIPE) {
        p = ent->pipe;
        pipe_we = ent->pipe_write_end;
    }

    ent->kind = FD_KIND_FREE;
    ent->handle = NULL;
    ent->pipe = NULL;
    ent->pipe_write_end = false;
    ent->pending_len = 0;
    ent->pending_off = 0;
    irq_restore(irq_flags);

    if (h) {
        return vfs_close(h);
    }
    if (p) {
        pipe_unref(p, pipe_we);
        return 0;
    }
    return 0;
}

bool tasking_tty_handle_input_char(uint8_t c) {
    if (!enabled || !current_task) {
        return false;
    }
    if (tty_foreground_pid == 0) {
        return false;
    }

    task_t* fg = task_find_by_pid(tty_foreground_pid);
    if (!fg || !fg->user || fg->state == TASK_STATE_ZOMBIE) {
        return false;
    }

    // Respect the foreground task's terminal settings:
    // only generate an interrupt when ISIG is enabled and the input matches VINTR.
    if ((fg->tty.c_lflag & VOS_ISIG) == 0) {
        return false;
    }
    uint8_t vintr = fg->tty.c_cc[VOS_VINTR];
    if (vintr == 0) {
        vintr = 0x03u; // default ^C
    }
    if (c != vintr) {
        return false;
    }

    // Approximate SIGINT using exit code 130 (128 + SIGINT).
    (void)tasking_kill(tty_foreground_pid, 130);
    screen_putchar('^');
    screen_putchar('C');
    screen_putchar('\n');
    return true;
}

static uint32_t tty_encode_key(int8_t key, uint8_t seq[8]) {
    if (!seq) {
        return 0;
    }

    switch (key) {
        case KEY_UP:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'A';
            return 3;
        case KEY_DOWN:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'B';
            return 3;
        case KEY_RIGHT:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'C';
            return 3;
        case KEY_LEFT:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'D';
            return 3;
        case KEY_HOME:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'H';
            return 3;
        case KEY_END:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = 'F';
            return 3;
        case KEY_PGUP:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '5'; seq[3] = '~';
            return 4;
        case KEY_PGDN:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '6'; seq[3] = '~';
            return 4;
        case KEY_DELETE:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '3'; seq[3] = '~';
            return 4;
        default:
            seq[0] = (uint8_t)key;
            return 1;
    }
}

static void tty_echo_key(int8_t key) {
    if (key == '\n' || key == '\r') {
        screen_putchar('\n');
        return;
    }
    if (key == '\b') {
        screen_backspace();
        return;
    }

    unsigned char uc = (unsigned char)key;
    if (uc >= (unsigned char)' ' && uc <= (unsigned char)'~') {
        screen_putchar((char)uc);
        return;
    }
    if (key == '\t') {
        screen_putchar('\t');
        return;
    }
}

static int32_t tty_deliver_canon_line(void* dst_user, uint32_t len) {
    if (!current_task || !dst_user) {
        return -EFAULT;
    }
    if (len == 0) {
        return 0;
    }

    if (!current_task->tty_line_ready || current_task->tty_line_off >= current_task->tty_line_len) {
        current_task->tty_line_ready = false;
        current_task->tty_line_len = 0;
        current_task->tty_line_off = 0;
        return 0;
    }

    uint32_t avail = (uint32_t)(current_task->tty_line_len - current_task->tty_line_off);
    uint32_t to_copy = len;
    if (to_copy > avail) {
        to_copy = avail;
    }
    if (to_copy == 0) {
        return 0;
    }

    if (!copy_to_user(dst_user, current_task->tty_line + current_task->tty_line_off, to_copy)) {
        return -EFAULT;
    }

    current_task->tty_line_off = (uint16_t)(current_task->tty_line_off + to_copy);
    if (current_task->tty_line_off >= current_task->tty_line_len) {
        current_task->tty_line_ready = false;
        current_task->tty_line_len = 0;
        current_task->tty_line_off = 0;
    }

    return (int32_t)to_copy;
}

static int32_t tty_read_canonical(void* dst_user, uint32_t len) {
    if (!current_task || !dst_user) {
        return -EFAULT;
    }
    if (len == 0) {
        return 0;
    }

    bool echo = (current_task->tty.c_lflag & VOS_ECHO) != 0;

    // If a line is already buffered, deliver it.
    if (current_task->tty_line_ready) {
        return tty_deliver_canon_line(dst_user, len);
    }

    // Start a fresh line.
    current_task->tty_line_len = 0;
    current_task->tty_line_off = 0;
    current_task->tty_line_ready = false;

    for (;;) {
        int8_t key = (int8_t)keyboard_getchar(); // blocks

        if (key == '\r' && (current_task->tty.c_iflag & VOS_ICRNL) != 0) {
            key = '\n';
        }

        uint8_t cc_eof = current_task->tty.c_cc[VOS_VEOF];
        uint8_t cc_erase = current_task->tty.c_cc[VOS_VERASE];

        if ((uint8_t)key == cc_eof) {
            // EOF: return 0 if no buffered bytes, otherwise return the line so far.
            if (current_task->tty_line_len == 0) {
                return 0;
            }
            current_task->tty_line_ready = true;
            break;
        }

        if (key == '\n') {
            if (echo) {
                tty_echo_key('\n');
            }
            if (current_task->tty_line_len < (uint16_t)VOS_TTY_LINE_MAX) {
                current_task->tty_line[current_task->tty_line_len++] = (uint8_t)'\n';
            }
            current_task->tty_line_ready = true;
            break;
        }

        if (key == '\b' || (uint8_t)key == cc_erase) {
            if (current_task->tty_line_len != 0) {
                current_task->tty_line_len--;
                if (echo) {
                    tty_echo_key('\b');
                }
            }
            continue;
        }

        uint8_t seq[8];
        uint32_t seq_len = tty_encode_key(key, seq);
        if (seq_len == 0) {
            continue;
        }

        uint32_t space = (uint32_t)VOS_TTY_LINE_MAX - (uint32_t)current_task->tty_line_len;
        if (space == 0) {
            continue;
        }
        if (seq_len > space) {
            seq_len = space;
        }

        for (uint32_t i = 0; i < seq_len; i++) {
            current_task->tty_line[current_task->tty_line_len++] = seq[i];
        }

        if (echo) {
            tty_echo_key(key);
        }
    }

    return tty_deliver_canon_line(dst_user, len);
}

int32_t tasking_fd_read(int32_t fd, void* dst_user, uint32_t len) {
    if (!current_task || !dst_user) {
        return -EFAULT;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }
    if (len == 0) {
        return 0;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];

    if (ent->kind == FD_KIND_STDIN) {
        irq_restore(irq_flags);

        bool canon = (current_task->tty.c_lflag & VOS_ICANON) != 0;
        if (canon) {
            return tty_read_canonical(dst_user, len);
        }

        bool echo = (current_task->tty.c_lflag & VOS_ECHO) != 0;
        uint8_t vmin = current_task->tty.c_cc[VOS_VMIN];
        uint8_t vtime = current_task->tty.c_cc[VOS_VTIME];

        uint8_t* dst = (uint8_t*)dst_user;
        uint32_t read = 0;

        // Drain any buffered escape sequence bytes first.
        while (read < len) {
            uint8_t b = 0;
            bool have = false;

            uint32_t f = irq_save();
            if (ent->pending_off < ent->pending_len) {
                b = ent->pending[ent->pending_off++];
                have = true;
                if (ent->pending_off >= ent->pending_len) {
                    ent->pending_len = 0;
                    ent->pending_off = 0;
                }
            }
            irq_restore(f);

            if (!have) {
                break;
            }

            if (!copy_to_user(dst + read, &b, 1u)) {
                return (read != 0) ? (int32_t)read : -EFAULT;
            }
            read++;
        }

        bool block = (read == 0);
        // Support the most common non-canonical "polling" mode:
        // VMIN=0, VTIME=0 => return immediately if no input is available.
        if (vmin == 0 && vtime == 0) {
            block = false;
        }
        while (read < len) {
            char c = 0;
            if (block) {
                c = keyboard_getchar(); // guarantees progress
            } else {
                if (!keyboard_try_getchar(&c)) {
                    break;
                }
            }
            block = false;

            if (echo) {
                tty_echo_key((int8_t)c);
            }

            uint8_t seq[8];
            uint32_t seq_len = tty_encode_key((int8_t)c, seq);
            if (seq_len == 0) {
                continue;
            }

            uint32_t avail = len - read;
            uint32_t to_copy = seq_len;
            if (to_copy > avail) {
                to_copy = avail;
            }

            if (!copy_to_user(dst + read, seq, to_copy)) {
                return (read != 0) ? (int32_t)read : -EFAULT;
            }
            read += to_copy;

            if (to_copy < seq_len) {
                uint32_t left = seq_len - to_copy;
                if (left > sizeof(ent->pending)) {
                    left = sizeof(ent->pending);
                }
                uint32_t f = irq_save();
                ent->pending_len = (uint8_t)left;
                ent->pending_off = 0;
                for (uint32_t i = 0; i < left; i++) {
                    ent->pending[i] = seq[to_copy + i];
                }
                irq_restore(f);
                break;
            }
        }

        return (int32_t)read;
    }

    if (ent->kind == FD_KIND_PIPE && ent->pipe) {
        pipe_obj_t* p = ent->pipe;
        irq_restore(irq_flags);

        uint32_t total = 0;
        uint8_t tmp[128];
        while (total < len) {
            uint32_t chunk = len - total;
            if (chunk > (uint32_t)sizeof(tmp)) {
                chunk = (uint32_t)sizeof(tmp);
            }

            uint32_t got = pipe_read_some(p, tmp, chunk);
            if (got != 0) {
                if (!copy_to_user((uint8_t*)dst_user + total, tmp, got)) {
                    return (total != 0) ? (int32_t)total : -EFAULT;
                }
                total += got;
                continue;
            }

            // Empty: EOF if no writers, otherwise block unless we already read something.
            uint32_t f = irq_save();
            uint32_t writers = p->writers;
            irq_restore(f);
            if (writers == 0) {
                break;
            }
            if (total != 0) {
                break;
            }
            wait_for_event();
        }

        return (int32_t)total;
    }

    if (ent->kind != FD_KIND_VFS || !ent->handle) {
        irq_restore(irq_flags);
        return -EBADF;
    }
    irq_restore(irq_flags);

    uint32_t total = 0;
    uint8_t tmp[256];
    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > (uint32_t)sizeof(tmp)) {
            chunk = (uint32_t)sizeof(tmp);
        }

        uint32_t got = 0;
        int32_t rc = vfs_read(ent->handle, tmp, chunk, &got);
        if (rc < 0) {
            return (total != 0) ? (int32_t)total : rc;
        }
        if (got == 0) {
            break;
        }
        if (!copy_to_user((uint8_t*)dst_user + total, tmp, got)) {
            return (total != 0) ? (int32_t)total : -EFAULT;
        }
        total += got;
    }
    return (int32_t)total;
}

int32_t tasking_fd_write(int32_t fd, const void* src_user, uint32_t len) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }
    if (len == 0) {
        return 0;
    }
    if (!src_user) {
        return -EFAULT;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    fd_kind_t kind = ent->kind;
    vfs_handle_t* h = ent->handle;
    irq_restore(irq_flags);

    uint32_t total = 0;
    uint8_t tmp[128];

    if (kind == FD_KIND_STDOUT || kind == FD_KIND_STDERR) {
        while (total < len) {
            uint32_t chunk = len - total;
            if (chunk > (uint32_t)sizeof(tmp)) {
                chunk = (uint32_t)sizeof(tmp);
            }
            if (!copy_from_user(tmp, (const uint8_t*)src_user + total, chunk)) {
                return (total != 0) ? (int32_t)total : -EFAULT;
            }
            for (uint32_t i = 0; i < chunk; i++) {
                screen_putchar((char)tmp[i]);
            }
            total += chunk;
        }
        return (int32_t)total;
    }

    if (kind == FD_KIND_PIPE && ent->pipe) {
        pipe_obj_t* p = ent->pipe;
        uint8_t tmp[128];

        while (total < len) {
            uint32_t chunk = len - total;
            if (chunk > (uint32_t)sizeof(tmp)) {
                chunk = (uint32_t)sizeof(tmp);
            }
            if (!copy_from_user(tmp, (const uint8_t*)src_user + total, chunk)) {
                return (total != 0) ? (int32_t)total : -EFAULT;
            }

            uint32_t wrote = 0;
            int32_t rc = pipe_write_some(p, tmp, chunk, &wrote);
            if (rc < 0) {
                return (total != 0) ? (int32_t)total : rc;
            }
            if (wrote != 0) {
                total += wrote;
                continue;
            }

            // Full: block unless we already wrote something.
            if (total != 0) {
                break;
            }
            wait_for_event();
        }

        return (int32_t)total;
    }

    if (kind != FD_KIND_VFS || !h) {
        return -EBADF;
    }

    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > (uint32_t)sizeof(tmp)) {
            chunk = (uint32_t)sizeof(tmp);
        }
        if (!copy_from_user(tmp, (const uint8_t*)src_user + total, chunk)) {
            return (total != 0) ? (int32_t)total : -EFAULT;
        }
        uint32_t wrote = 0;
        int32_t rc = vfs_write(h, tmp, chunk, &wrote);
        if (rc < 0) {
            return (total != 0) ? (int32_t)total : rc;
        }
        total += wrote;
        if (wrote != chunk) {
            break;
        }
    }
    return (int32_t)total;
}

int32_t tasking_fd_lseek(int32_t fd, int32_t offset, int32_t whence) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    vfs_handle_t* h = ent->handle;
    fd_kind_t kind = ent->kind;
    irq_restore(irq_flags);

    if (kind != FD_KIND_VFS || !h) {
        return -ESPIPE;
    }

    uint32_t new_off = 0;
    int32_t rc = vfs_lseek(h, offset, whence, &new_off);
    if (rc < 0) {
        return rc;
    }
    return (int32_t)new_off;
}

int32_t tasking_fd_fstat(int32_t fd, void* st_user) {
    if (!current_task || !st_user) {
        return -EFAULT;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    fd_kind_t kind = ent->kind;
    vfs_handle_t* h = ent->handle;
    irq_restore(irq_flags);

    vfs_stat_t st;
    memset(&st, 0, sizeof(st));

    if (kind == FD_KIND_VFS && h) {
        int32_t rc = vfs_fstat(h, &st);
        if (rc < 0) {
            return rc;
        }
    } else if (kind == FD_KIND_STDIN || kind == FD_KIND_STDOUT || kind == FD_KIND_STDERR) {
        // Leave as a "tty-like" entry (size 0, not a directory).
    } else if (kind == FD_KIND_PIPE) {
        // Anonymous pipe: treat as a non-directory stream.
    } else {
        return -EBADF;
    }

    if (!copy_to_user(st_user, &st, (uint32_t)sizeof(st))) {
        return -EFAULT;
    }
    return 0;
}

int32_t tasking_stat(const char* path, void* st_user) {
    if (!current_task || !path || !st_user) {
        return -EINVAL;
    }

    vfs_stat_t st;
    int32_t rc = vfs_stat_path(current_task->cwd, path, &st);
    if (rc < 0) {
        return rc;
    }
    if (!copy_to_user(st_user, &st, (uint32_t)sizeof(st))) {
        return -EFAULT;
    }
    return 0;
}

int32_t tasking_mkdir(const char* path) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    return vfs_mkdir_path(current_task->cwd, path);
}

int32_t tasking_readdir(int32_t fd, void* dirent_user) {
    if (!current_task || !dirent_user) {
        return -EFAULT;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    vfs_handle_t* h = ent->handle;
    fd_kind_t kind = ent->kind;
    irq_restore(irq_flags);

    if (kind != FD_KIND_VFS || !h) {
        return -EBADF;
    }

    vfs_dirent_t de;
    int32_t rc = vfs_readdir(h, &de);
    if (rc <= 0) {
        return rc;
    }

    if (!copy_to_user(dirent_user, &de, (uint32_t)sizeof(de))) {
        return -EFAULT;
    }
    return 1;
}

int32_t tasking_chdir(const char* path) {
    if (!current_task || !path) {
        return -EINVAL;
    }

    vfs_stat_t st;
    int32_t rc = vfs_stat_path(current_task->cwd, path, &st);
    if (rc < 0) {
        return rc;
    }
    if (!st.is_dir) {
        return -ENOTDIR;
    }

    char abs[VFS_PATH_MAX];
    rc = vfs_path_resolve(current_task->cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    strncpy(current_task->cwd, abs, sizeof(current_task->cwd) - 1u);
    current_task->cwd[sizeof(current_task->cwd) - 1u] = '\0';
    return 0;
}

int32_t tasking_getcwd(void* dst_user, uint32_t len) {
    if (!current_task || !dst_user) {
        return -EFAULT;
    }

    uint32_t need = (uint32_t)strlen(current_task->cwd) + 1u;
    if (len < need) {
        return -ERANGE;
    }
    if (!copy_to_user(dst_user, current_task->cwd, need)) {
        return -EFAULT;
    }
    return (int32_t)(need - 1u);
}

int32_t tasking_unlink(const char* path) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    return vfs_unlink_path(current_task->cwd, path);
}

int32_t tasking_rename(const char* old_path, const char* new_path) {
    if (!current_task || !old_path || !new_path) {
        return -EINVAL;
    }
    return vfs_rename_path(current_task->cwd, old_path, new_path);
}

int32_t tasking_rmdir(const char* path) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    return vfs_rmdir_path(current_task->cwd, path);
}

int32_t tasking_truncate(const char* path, uint32_t new_size) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    return vfs_truncate_path(current_task->cwd, path, new_size);
}

int32_t tasking_fd_ftruncate(int32_t fd, uint32_t new_size) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    vfs_handle_t* h = ent->handle;
    fd_kind_t kind = ent->kind;
    irq_restore(irq_flags);

    if (kind != FD_KIND_VFS || !h) {
        return -EBADF;
    }
    return vfs_ftruncate(h, new_size);
}

int32_t tasking_fd_fsync(int32_t fd) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    vfs_handle_t* h = ent->handle;
    fd_kind_t kind = ent->kind;
    irq_restore(irq_flags);

    if (kind != FD_KIND_VFS || !h) {
        return -EBADF;
    }
    return vfs_fsync(h);
}

int32_t tasking_fd_dup(int32_t oldfd) {
    if (!current_task) {
        return -EINVAL;
    }
    if (oldfd < 0 || oldfd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    vfs_handle_t* h = NULL;
    pipe_obj_t* p = NULL;
    bool pipe_we = false;

    uint32_t irq_flags = irq_save();
    fd_entry_t* src = &current_task->fds[oldfd];
    if (src->kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -EBADF;
    }

    int32_t newfd = -1;
    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        if (current_task->fds[fd].kind == FD_KIND_FREE) {
            newfd = fd;
            break;
        }
    }
    if (newfd < 0) {
        irq_restore(irq_flags);
        return -EMFILE;
    }

    fd_entry_t* dst = &current_task->fds[newfd];
    dst->kind = src->kind;
    dst->handle = src->handle;
    dst->pipe = src->pipe;
    dst->pipe_write_end = src->pipe_write_end;
    dst->pending_len = 0;
    dst->pending_off = 0;

    if (src->kind == FD_KIND_VFS && src->handle) {
        h = src->handle;
    } else if (src->kind == FD_KIND_PIPE && src->pipe) {
        p = src->pipe;
        pipe_we = src->pipe_write_end;
    }

    irq_restore(irq_flags);

    if (h) {
        vfs_ref(h);
    }
    if (p) {
        pipe_ref(p, pipe_we);
    }

    return newfd;
}

int32_t tasking_fd_dup2(int32_t oldfd, int32_t newfd) {
    if (!current_task) {
        return -EINVAL;
    }
    if (oldfd < 0 || oldfd >= (int32_t)TASK_MAX_FDS || newfd < 0 || newfd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }
    if (oldfd == newfd) {
        return newfd;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* src = &current_task->fds[oldfd];
    if (src->kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -EBADF;
    }

    bool need_close = current_task->fds[newfd].kind != FD_KIND_FREE;
    irq_restore(irq_flags);

    if (need_close) {
        int32_t rc_close = tasking_fd_close(newfd);
        if (rc_close < 0) {
            return rc_close;
        }
    }

    vfs_handle_t* h = NULL;
    pipe_obj_t* p = NULL;
    bool pipe_we = false;

    irq_flags = irq_save();
    src = &current_task->fds[oldfd];
    if (src->kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -EBADF;
    }

    fd_entry_t* dst = &current_task->fds[newfd];
    dst->kind = src->kind;
    dst->handle = src->handle;
    dst->pipe = src->pipe;
    dst->pipe_write_end = src->pipe_write_end;
    dst->pending_len = 0;
    dst->pending_off = 0;

    if (src->kind == FD_KIND_VFS && src->handle) {
        h = src->handle;
    } else if (src->kind == FD_KIND_PIPE && src->pipe) {
        p = src->pipe;
        pipe_we = src->pipe_write_end;
    }

    irq_restore(irq_flags);

    if (h) {
        vfs_ref(h);
    }
    if (p) {
        pipe_ref(p, pipe_we);
    }

    return newfd;
}

int32_t tasking_pipe(void* pipefds_user) {
    if (!current_task || !pipefds_user) {
        return -EFAULT;
    }

    pipe_obj_t* p = pipe_create();
    if (!p) {
        return -ENOMEM;
    }

    int32_t rfd = -1;
    int32_t wfd = -1;

    uint32_t irq_flags = irq_save();
    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        if (current_task->fds[fd].kind != FD_KIND_FREE) {
            continue;
        }
        if (rfd < 0) {
            rfd = fd;
        } else {
            wfd = fd;
            break;
        }
    }

    if (rfd < 0 || wfd < 0) {
        irq_restore(irq_flags);
        kfree(p);
        return -EMFILE;
    }

    fd_entry_t* r = &current_task->fds[rfd];
    r->kind = FD_KIND_PIPE;
    r->handle = NULL;
    r->pipe = p;
    r->pipe_write_end = false;
    r->pending_len = 0;
    r->pending_off = 0;

    fd_entry_t* w = &current_task->fds[wfd];
    w->kind = FD_KIND_PIPE;
    w->handle = NULL;
    w->pipe = p;
    w->pipe_write_end = true;
    w->pending_len = 0;
    w->pending_off = 0;

    irq_restore(irq_flags);

    int32_t pair[2] = {rfd, wfd};
    if (!copy_to_user(pipefds_user, pair, (uint32_t)sizeof(pair))) {
        (void)tasking_fd_close(rfd);
        (void)tasking_fd_close(wfd);
        return -EFAULT;
    }

    return 0;
}

int32_t tasking_fd_ioctl(int32_t fd, uint32_t req, void* argp_user) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    fd_kind_t kind = ent->kind;
    irq_restore(irq_flags);

    if (kind != FD_KIND_STDIN && kind != FD_KIND_STDOUT && kind != FD_KIND_STDERR) {
        return -ENOTTY;
    }

    if (!argp_user && (req == VOS_TCGETS || req == VOS_TCSETS || req == VOS_TCSETSW || req == VOS_TCSETSF ||
                       req == VOS_TIOCGPGRP || req == VOS_TIOCSPGRP || req == VOS_TIOCGWINSZ)) {
        return -EFAULT;
    }

    switch (req) {
        case VOS_TIOCGPGRP: {
            uint32_t pid = tty_foreground_pid;
            if (!copy_to_user(argp_user, &pid, (uint32_t)sizeof(pid))) {
                return -EFAULT;
            }
            return 0;
        }
        case VOS_TIOCSPGRP: {
            uint32_t pid = 0;
            if (!copy_from_user(&pid, argp_user, (uint32_t)sizeof(pid))) {
                return -EFAULT;
            }
            if (pid == 0) {
                tty_foreground_pid = 0;
                return 0;
            }
            task_t* fg = task_find_by_pid(pid);
            if (!fg) {
                return -ESRCH;
            }
            if (!fg->user) {
                return -EPERM;
            }
            tty_foreground_pid = pid;
            return 0;
        }
        case VOS_TIOCGWINSZ: {
            vos_winsize_t ws;
            ws.ws_col = (uint16_t)screen_cols();
            ws.ws_row = (uint16_t)screen_usable_rows();
            ws.ws_xpixel = (uint16_t)screen_framebuffer_width();
            ws.ws_ypixel = (uint16_t)screen_framebuffer_height();
            if (!copy_to_user(argp_user, &ws, (uint32_t)sizeof(ws))) {
                return -EFAULT;
            }
            return 0;
        }
        case VOS_TCGETS: {
            if (!copy_to_user(argp_user, &current_task->tty, (uint32_t)sizeof(current_task->tty))) {
                return -EFAULT;
            }
            return 0;
        }
        case VOS_TCSETS:
        case VOS_TCSETSW:
        case VOS_TCSETSF: {
            vos_termios_t t;
            if (!copy_from_user(&t, argp_user, (uint32_t)sizeof(t))) {
                return -EFAULT;
            }

            // Keep only the bits we support; preserve other bits so userland can round-trip.
            current_task->tty = t;

            // Drop any partially buffered input when terminal settings change.
            irq_flags = irq_save();
            ent = &current_task->fds[fd];
            ent->pending_len = 0;
            ent->pending_off = 0;
            irq_restore(irq_flags);
            current_task->tty_line_len = 0;
            current_task->tty_line_off = 0;
            current_task->tty_line_ready = false;
            return 0;
        }
        default:
            return -ENOTTY;
    }
}
