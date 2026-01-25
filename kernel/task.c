#include "task.h"
#include "kheap.h"
#include "string.h"
#include "gdt.h"
#include "timer.h"
#include "io.h"
#include "paging.h"
#include "early_alloc.h"
#include "pmm.h"
#include "elf.h"
#include "usercopy.h"
#include "kerrno.h"
#include "screen.h"
#include "serial.h"
#include "vfs.h"
#include "keyboard.h"

#define KSTACK_SIZE (16u * 1024u)
#define TASK_NAME_LEN 15
#define TASK_MAX_SCAN 256
#define KSTACK_REGION_BASE 0xF0000000u
#define TASK_MAX_FDS 64

// Minimal signal support for userland ports (ne, etc.).
// We follow newlib's default numbering (see <sys/signal.h> for i386):
//   SIGKILL=9, SIGSTOP=17, SIGTSTP=18, SIGCONT=19, SIGWINCH=28, ...
#define VOS_SIG_MAX 32
#define VOS_SIG_DFL 0u
#define VOS_SIG_IGN 1u
#define VOS_SIGKILL 9
#define VOS_SIGALRM 14
#define VOS_SIGSTOP 17
#define VOS_SIGWINCH 28
#define VOS_SIGCHLD 20

// Keep in sync with newlib's <sys/_default_fcntl.h>.
enum {
    VOS_F_DUPFD = 0,
    VOS_F_GETFD = 1,
    VOS_F_SETFD = 2,
    VOS_F_GETFL = 3,
    VOS_F_SETFL = 4,
    VOS_F_DUPFD_CLOEXEC = 14,
};

enum {
    VOS_FD_CLOEXEC = 1,
};

enum {
    VOS_O_ACCMODE = 3,
    VOS_O_APPEND = 0x0008u,
    VOS_O_NONBLOCK = 0x4000u,
};

// User virtual address layout (must match kernel/elf.c and kernel/paging.c).
#define USER_BASE  0x02000000u
#define USER_LIMIT 0xC0000000u

// Keep the user stack high to leave plenty of room for heap + mmaps.
#define USER_STACK_TOP   0xBFF00000u
#define USER_STACK_PAGES 64u

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
    uint32_t fd_flags;   // FD_CLOEXEC etc (F_GETFD/F_SETFD)
    uint32_t fl_flags;   // O_* status flags (F_GETFL/F_SETFL; subset)
    vfs_handle_t* handle;
    pipe_obj_t* pipe;
    bool pipe_write_end;
    uint8_t pending[8];
    uint8_t pending_len;
    uint8_t pending_off;
} fd_entry_t;

typedef struct vm_area {
    uint32_t start;
    uint32_t size;
    uint32_t prot;
    struct vm_area* next;
} vm_area_t;

typedef struct vos_sigframe {
    uint32_t magic;
    uint32_t sig;
    uint32_t saved_mask;
    interrupt_frame_t frame;
    uint32_t user_esp;
    uint32_t user_ss;
} __attribute__((packed)) vos_sigframe_t;

#define VOS_SIGFRAME_MAGIC 0x53494746u /* 'SIGF' */

typedef struct task {
    uint32_t id;
    uint32_t ppid;
    uint32_t pgid;
    uint32_t esp;            // saved stack pointer (points to interrupt frame)
    uint32_t kstack_top;     // top of kernel stack (for TSS.esp0)
    uint32_t* page_directory;
    bool user;
    uint32_t uid;
    uint32_t gid;
    uint32_t user_brk;
    uint32_t user_brk_min;
    vm_area_t* vm_areas;
    uint32_t mmap_top;
    fd_entry_t fds[TASK_MAX_FDS];
    char cwd[VFS_PATH_MAX];
    vos_termios_t tty;
    uint8_t tty_line[VOS_TTY_LINE_MAX];
    uint16_t tty_line_len;
    uint16_t tty_line_off;
    bool tty_line_ready;
    uint32_t sig_pending;
    uint32_t sig_mask;
    uint32_t sig_handlers[VOS_SIG_MAX];
    task_state_t state;
    uint32_t wake_tick;
    uint32_t wait_pid;
    void* wait_status_user; // waitpid-style status output (optional)
    bool wait_return_pid;   // true for waitpid-style return (pid), false for legacy wait (exit code)
    int32_t exit_code;
    bool waited;            // set once exit status has been delivered to a waiter; safe to reap
    bool kill_pending;
    int32_t kill_exit_code;
    uint32_t alarm_tick; // 0 = disabled; timer_get_ticks() deadline for SIGALRM
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
static uint32_t tty_foreground_pgid = 0;
static bool reap_pending = false;

// Sentinel value used to wait for "any child" in waitpid-style syscalls.
#define WAIT_ANY_PID 0xFFFFFFFFu
#define FORK_COPY_VA 0xE0000000u

static void task_close_fds(task_t* t);

static void task_free_vm_areas(vm_area_t* head) {
    vm_area_t* cur = head;
    while (cur) {
        vm_area_t* next = cur->next;
        kfree(cur);
        cur = next;
    }
}

static vm_area_t* vm_clone_areas(const vm_area_t* head) {
    vm_area_t* out_head = NULL;
    vm_area_t** tail = &out_head;

    const vm_area_t* cur = head;
    while (cur) {
        vm_area_t* node = (vm_area_t*)kmalloc(sizeof(*node));
        if (!node) {
            task_free_vm_areas(out_head);
            return NULL;
        }
        memset(node, 0, sizeof(*node));
        node->start = cur->start;
        node->size = cur->size;
        node->prot = cur->prot;
        node->next = NULL;
        *tail = node;
        tail = &node->next;
        cur = cur->next;
    }
    return out_head;
}

static void free_user_pages_in_directory(uint32_t* dir) {
    if (!dir) {
        return;
    }

    uint32_t start_pde = USER_BASE >> 22;
    uint32_t end_pde = USER_LIMIT >> 22;

    for (uint32_t dir_index = start_pde; dir_index < end_pde; dir_index++) {
        uint32_t pde = dir[dir_index];
        if ((pde & PAGE_PRESENT) == 0 || (pde & PAGE_USER) == 0) {
            continue;
        }

        uint32_t* table = (uint32_t*)(pde & 0xFFFFF000u);
        for (uint32_t tbl_index = 0; tbl_index < 1024u; tbl_index++) {
            uint32_t pte = table[tbl_index];
            if ((pte & PAGE_PRESENT) == 0 || (pte & PAGE_USER) == 0) {
                continue;
            }

            uint32_t paddr = pte & 0xFFFFF000u;
            table[tbl_index] = 0;
            if (paddr) {
                pmm_free_frame(paddr);
            }
        }
    }
}

static void task_free_user_pages(task_t* t) {
    if (!t || !t->user || !t->page_directory) {
        return;
    }
    free_user_pages_in_directory(t->page_directory);
}

static void task_free_kstack(task_t* t) {
    if (!t) {
        return;
    }

    // Only stacks allocated via kstack_alloc() live in the dedicated region.
    if (t->kstack_top < (KSTACK_REGION_BASE + PAGE_SIZE + KSTACK_SIZE)) {
        return;
    }

    uint32_t bottom = t->kstack_top - KSTACK_SIZE;
    for (uint32_t va = bottom; va < t->kstack_top; va += PAGE_SIZE) {
        uint32_t paddr = 0;
        if (paging_unmap_page(va, &paddr) && paddr) {
            pmm_free_frame(paddr);
        }
    }
}

static task_t* task_find_prev(task_t* target) {
    if (!current_task || !target) {
        return NULL;
    }

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t || !t->next) {
            return NULL;
        }
        if (t->next == target) {
            return t;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }
    return NULL;
}

static bool task_detach(task_t* target) {
    if (!current_task || !target) {
        return false;
    }
    if (target == current_task) {
        return false;
    }

    task_t* prev = task_find_prev(target);
    if (!prev) {
        return false;
    }

    prev->next = target->next;
    target->next = NULL;
    return true;
}

static void task_reap_detached(task_t* t) {
    if (!t) {
        return;
    }

    task_close_fds(t);
    task_free_vm_areas(t->vm_areas);
    t->vm_areas = NULL;
    task_free_user_pages(t);
    task_free_kstack(t);
    kfree(t);
}

static void task_reap_waited_zombies(void) {
    if (!current_task || !reap_pending) {
        return;
    }

    uint32_t irq_flags = irq_save();

    task_t* prev = current_task;
    task_t* t = current_task->next;

    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t || t == current_task) {
            break;
        }

        if (t->state == TASK_STATE_ZOMBIE && t->waited) {
            prev->next = t->next;
            task_t* victim = t;
            t = prev->next;
            victim->next = NULL;
            task_reap_detached(victim);
            continue;
        }

        prev = t;
        t = t->next;
    }

    // Best-effort: waited zombies should be fully drained in one scan on small
    // systems. Clear the hint flag; it will be raised again if needed.
    reap_pending = false;
    irq_restore(irq_flags);
}

static bool frame_from_user(const interrupt_frame_t* frame) {
    return frame && ((frame->cs & 0x3u) == 0x3u);
}

static uint32_t frame_get_user_esp(const interrupt_frame_t* frame) {
    const uint32_t* extra = (const uint32_t*)((const uint8_t*)frame + sizeof(*frame));
    return extra[0];
}

static uint32_t frame_get_user_ss(const interrupt_frame_t* frame) {
    const uint32_t* extra = (const uint32_t*)((const uint8_t*)frame + sizeof(*frame));
    return extra[1];
}

static void frame_set_user_esp(interrupt_frame_t* frame, uint32_t user_esp) {
    uint32_t* extra = (uint32_t*)((uint8_t*)frame + sizeof(*frame));
    extra[0] = user_esp;
}

static void frame_set_user_ss(interrupt_frame_t* frame, uint32_t user_ss) {
    uint32_t* extra = (uint32_t*)((uint8_t*)frame + sizeof(*frame));
    extra[1] = user_ss;
}

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

static task_t* task_find_any_by_pgid(uint32_t pgid) {
    if (!current_task || pgid == 0) {
        return NULL;
    }
    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->pgid == pgid) {
            return t;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }
    return NULL;
}

static void task_queue_signal(task_t* t, int32_t sig) {
    if (!t) {
        return;
    }
    if (t->state == TASK_STATE_ZOMBIE) {
        return;
    }

    // Uncatchable signals become deferred kills to avoid tearing down resources
    // while the target is executing in-kernel.
    if (sig == VOS_SIGKILL) {
        t->kill_pending = true;
        t->kill_exit_code = 128 + sig;
    } else {
        t->sig_pending |= (1u << (uint32_t)sig);
    }

    bool wake = (sig == VOS_SIGKILL);
    if (!wake) {
        wake = (t->sig_mask & (1u << (uint32_t)sig)) == 0;
    }

    if (wake && t->state != TASK_STATE_RUNNABLE) {
        t->state = TASK_STATE_RUNNABLE;
        t->wake_tick = 0;
        t->wait_pid = 0;
    }
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
    if (TASK_MAX_FDS > 0) {
        t->fds[0].kind = FD_KIND_STDIN;
        t->fds[0].fl_flags = 0; // O_RDONLY
    }
    if (TASK_MAX_FDS > 1) {
        t->fds[1].kind = FD_KIND_STDOUT;
        t->fds[1].fl_flags = 1; // O_WRONLY
    }
    if (TASK_MAX_FDS > 2) {
        t->fds[2].kind = FD_KIND_STDERR;
        t->fds[2].fl_flags = 1; // O_WRONLY
    }
}

static void fd_inherit(task_t* child, const task_t* parent) {
    if (!child || !parent) {
        return;
    }

    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        const fd_entry_t* src = &parent->fds[fd];
        fd_entry_t* dst = &child->fds[fd];

        dst->pending_len = 0;
        dst->pending_off = 0;

        if (src->kind == FD_KIND_FREE || (src->fd_flags & VOS_FD_CLOEXEC) != 0) {
            dst->kind = FD_KIND_FREE;
            dst->fd_flags = 0;
            dst->fl_flags = 0;
            dst->handle = NULL;
            dst->pipe = NULL;
            dst->pipe_write_end = false;
            continue;
        }

        dst->kind = src->kind;
        dst->fd_flags = 0; // CLOEXEC semantics: descriptors that survive exec have FD flags cleared
        dst->fl_flags = src->fl_flags;

        if (src->kind == FD_KIND_VFS && src->handle) {
            dst->handle = src->handle;
            dst->pipe = NULL;
            dst->pipe_write_end = false;
            vfs_ref(dst->handle);
            continue;
        }

        if (src->kind == FD_KIND_PIPE && src->pipe) {
            dst->handle = NULL;
            dst->pipe = src->pipe;
            dst->pipe_write_end = src->pipe_write_end;
            pipe_ref(dst->pipe, dst->pipe_write_end);
            continue;
        }

        dst->handle = NULL;
        dst->pipe = NULL;
        dst->pipe_write_end = false;
    }
}

static void fd_clone(task_t* child, const task_t* parent) {
    if (!child || !parent) {
        return;
    }

    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        const fd_entry_t* src = &parent->fds[fd];
        fd_entry_t* dst = &child->fds[fd];

        dst->pending_len = 0;
        dst->pending_off = 0;

        if (src->kind == FD_KIND_FREE) {
            dst->kind = FD_KIND_FREE;
            dst->fd_flags = 0;
            dst->fl_flags = 0;
            dst->handle = NULL;
            dst->pipe = NULL;
            dst->pipe_write_end = false;
            continue;
        }

        dst->kind = src->kind;
        dst->fd_flags = src->fd_flags;
        dst->fl_flags = src->fl_flags;

        if (src->kind == FD_KIND_VFS && src->handle) {
            dst->handle = src->handle;
            dst->pipe = NULL;
            dst->pipe_write_end = false;
            vfs_ref(dst->handle);
            continue;
        }

        if (src->kind == FD_KIND_PIPE && src->pipe) {
            dst->handle = NULL;
            dst->pipe = src->pipe;
            dst->pipe_write_end = src->pipe_write_end;
            pipe_ref(dst->pipe, dst->pipe_write_end);
            continue;
        }

        dst->handle = NULL;
        dst->pipe = NULL;
        dst->pipe_write_end = false;
    }
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

    uint32_t va = stack_bottom;
    for (; va < stack_top; va += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            // Roll back partial allocation.
            for (uint32_t un = stack_bottom; un < va; un += PAGE_SIZE) {
                uint32_t paddr = 0;
                if (paging_unmap_page(un, &paddr) && paddr) {
                    pmm_free_frame(paddr);
                }
            }
            next_kstack_region = region_base;
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
    t->ppid = 0;
    t->pgid = 0;
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
    t->alarm_tick = 0;
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
    t->ppid = 0;
    t->pgid = t->id;
    t->esp = (uint32_t)sp;
    t->kstack_top = stack_top_addr;
    t->page_directory = page_directory;
    t->user = true;
    t->user_brk = user_brk;
    t->user_brk_min = user_brk;
    t->vm_areas = NULL;
    t->mmap_top = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    fd_init(t);
    cwd_init(t);
    tty_init(t);
    t->state = TASK_STATE_RUNNABLE;
    t->wake_tick = 0;
    t->wait_pid = 0;
    t->exit_code = 0;
    t->alarm_tick = 0;
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

static uint32_t* fork_ensure_child_table(uint32_t* dir, uint32_t dir_index) {
    if (!dir) {
        return NULL;
    }

    uint32_t entry = dir[dir_index];
    if (entry & PAGE_PRESENT) {
        return (uint32_t*)(entry & 0xFFFFF000u);
    }

    uint32_t* table = (uint32_t*)early_alloc(PAGE_SIZE, PAGE_SIZE);
    memset(table, 0, PAGE_SIZE);
    dir[dir_index] = ((uint32_t)table & 0xFFFFF000u) | (PAGE_PRESENT | PAGE_RW | PAGE_USER);
    return table;
}

static uint32_t* fork_clone_user_directory(const task_t* parent) {
    if (!parent || !parent->user || !parent->page_directory) {
        return NULL;
    }

    uint32_t* child_dir = paging_create_user_directory();
    if (!child_dir) {
        return NULL;
    }

    // Ensure the scratch mapping is backed by a page table in the kernel directory.
    paging_prepare_range(FORK_COPY_VA, PAGE_SIZE, PAGE_PRESENT | PAGE_RW);

    uint32_t start_pde = USER_BASE >> 22;
    uint32_t end_pde = USER_LIMIT >> 22;

    for (uint32_t dir_index = start_pde; dir_index < end_pde; dir_index++) {
        uint32_t pde = parent->page_directory[dir_index];
        if ((pde & PAGE_PRESENT) == 0 || (pde & PAGE_USER) == 0) {
            continue;
        }

        uint32_t* src_table = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t* dst_table = NULL;

        for (uint32_t tbl_index = 0; tbl_index < 1024u; tbl_index++) {
            uint32_t pte = src_table[tbl_index];
            if ((pte & PAGE_PRESENT) == 0 || (pte & PAGE_USER) == 0) {
                continue;
            }

            if (!dst_table) {
                dst_table = fork_ensure_child_table(child_dir, dir_index);
                if (!dst_table) {
                    free_user_pages_in_directory(child_dir);
                    return NULL;
                }
            }

            uint32_t va = (dir_index << 22) | (tbl_index << 12);

            uint32_t dst_paddr = pmm_alloc_frame();
            if (dst_paddr == 0) {
                free_user_pages_in_directory(child_dir);
                return NULL;
            }

            uint32_t map_flags = PAGE_PRESENT | PAGE_USER;
            if (pte & PAGE_RW) {
                map_flags |= PAGE_RW;
            }
            dst_table[tbl_index] = (dst_paddr & 0xFFFFF000u) | (map_flags & 0xFFFu);

            // Copy the source page into the new physical frame using a temporary
            // kernel mapping.
            paging_map_page(FORK_COPY_VA, dst_paddr, PAGE_PRESENT | PAGE_RW);
            memcpy((void*)FORK_COPY_VA, (const void*)va, PAGE_SIZE);
            (void)paging_unmap_page(FORK_COPY_VA, NULL);
        }
    }

    return child_dir;
}

uint32_t tasking_current_pid(void) {
    return current_task ? current_task->id : 0;
}

uint32_t tasking_current_ppid(void) {
    uint32_t irq_flags = irq_save();
    uint32_t ppid = current_task ? current_task->ppid : 0u;
    irq_restore(irq_flags);
    return ppid;
}

uint32_t tasking_getpgrp(void) {
    uint32_t irq_flags = irq_save();
    uint32_t pgid = current_task ? current_task->pgid : 0u;
    irq_restore(irq_flags);
    return pgid;
}

int32_t tasking_alarm(uint32_t seconds) {
    if (!enabled || !current_task) {
        return -EINVAL;
    }

    uint32_t hz = timer_get_hz();
    if (hz == 0) {
        return -EINVAL;
    }

    uint32_t now = timer_get_ticks();

    uint32_t irq_flags = irq_save();
    uint32_t prev_tick = current_task->alarm_tick;

    uint32_t prev_remaining = 0;
    if (prev_tick != 0 && (int32_t)(prev_tick - now) > 0) {
        uint32_t rem_ticks = prev_tick - now;
        prev_remaining = (rem_ticks + hz - 1u) / hz;
    }

    if (seconds == 0) {
        current_task->alarm_tick = 0;
        irq_restore(irq_flags);
        return (int32_t)prev_remaining;
    }

    uint64_t add = (uint64_t)seconds * (uint64_t)hz;
    if (add > 0xFFFFFFFFu) {
        add = 0xFFFFFFFFu;
    }

    uint32_t deadline = now + (uint32_t)add;
    if (deadline == 0) {
        deadline = 0xFFFFFFFFu;
    }
    current_task->alarm_tick = deadline;

    irq_restore(irq_flags);
    return (int32_t)prev_remaining;
}

uint32_t tasking_getuid(void) {
    uint32_t irq_flags = irq_save();
    uint32_t uid = current_task ? current_task->uid : 0u;
    irq_restore(irq_flags);
    return uid;
}

uint32_t tasking_getgid(void) {
    uint32_t irq_flags = irq_save();
    uint32_t gid = current_task ? current_task->gid : 0u;
    irq_restore(irq_flags);
    return gid;
}

int32_t tasking_setuid(uint32_t uid) {
    uint32_t irq_flags = irq_save();
    if (!current_task) {
        irq_restore(irq_flags);
        return -EINVAL;
    }
    if (current_task->uid != 0u && uid != current_task->uid) {
        irq_restore(irq_flags);
        return -EPERM;
    }
    current_task->uid = uid;
    irq_restore(irq_flags);
    return 0;
}

int32_t tasking_setgid(uint32_t gid) {
    uint32_t irq_flags = irq_save();
    if (!current_task) {
        irq_restore(irq_flags);
        return -EINVAL;
    }
    if (current_task->uid != 0u && gid != current_task->gid) {
        irq_restore(irq_flags);
        return -EPERM;
    }
    current_task->gid = gid;
    irq_restore(irq_flags);
    return 0;
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

bool tasking_current_should_interrupt(void) {
    if (!enabled || !current_task) {
        return false;
    }
    if (current_task->kill_pending) {
        return true;
    }
    uint32_t pending = current_task->sig_pending & ~current_task->sig_mask;
    return pending != 0;
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

static void check_alarms(uint32_t now_ticks) {
    if (!current_task) {
        return;
    }

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }

        if (t->user && t->alarm_tick != 0) {
            if ((int32_t)(now_ticks - t->alarm_tick) >= 0) {
                t->alarm_tick = 0;
                task_queue_signal(t, VOS_SIGALRM);
            }
        }

        t = t->next;
        if (t == current_task) {
            break;
        }
    }
}

static int32_t wait_encode_status(int32_t exit_code) {
    // Best-effort POSIX encoding: store exit status in the high byte.
    // This matches WEXITSTATUS(status) on typical systems.
    uint32_t code = (exit_code < 0) ? 255u : ((uint32_t)exit_code & 0xFFu);
    return (int32_t)(code << 8);
}

static void wake_waiters(task_t* dead) {
    if (!current_task || !dead) {
        return;
    }

    uint32_t pid = dead->id;
    int32_t exit_code = dead->exit_code;
    uint32_t dead_ppid = dead->ppid;
    bool any_woken = false;
    bool any_delivered = false;

    uint32_t irq_flags = irq_save();
    uint32_t* dead_dir = current_task->page_directory ? current_task->page_directory : paging_kernel_directory();

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }

        bool match = false;
        if (t->state == TASK_STATE_WAITING) {
            if (t->wait_pid == pid) {
                match = true;
            } else if (t->wait_pid == WAIT_ANY_PID && dead_ppid != 0 && t->id == dead_ppid) {
                match = true;
            }
        }

        if (match) {
            t->state = TASK_STATE_RUNNABLE;
            t->wait_pid = 0;

            if (t->esp) {
                interrupt_frame_t* f = (interrupt_frame_t*)t->esp;
                if (t->wait_return_pid) {
                    bool delivered = true;
                    if (t->wait_status_user) {
                        int32_t status = wait_encode_status(exit_code);

                        uint32_t* waiter_dir = t->page_directory ? t->page_directory : paging_kernel_directory();
                        if (waiter_dir != dead_dir) {
                            paging_switch_directory(waiter_dir);
                        }
                        delivered = copy_to_user(t->wait_status_user, &status, (uint32_t)sizeof(status));
                        if (waiter_dir != dead_dir) {
                            paging_switch_directory(dead_dir);
                        }
                    }

                    if (delivered) {
                        f->eax = pid;
                        any_delivered = true;
                    } else {
                        f->eax = (uint32_t)-EFAULT;
                    }
                } else {
                    f->eax = (uint32_t)exit_code;
                    any_delivered = true;
                }
            }

            t->wait_status_user = NULL;
            t->wait_return_pid = false;
            any_woken = true;
        }

        t = t->next;
        if (t == current_task) {
            break;
        }
    }

    if (any_woken && any_delivered) {
        dead->waited = true;
        reap_pending = true;
    }

    irq_restore(irq_flags);
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

    task_reap_waited_zombies();

    // Only terminate a task at safe points (when the interrupt happened in ring3).
    if ((frame->cs & 3u) == 3u && current_task->kill_pending) {
        return tasking_exit(frame, current_task->kill_exit_code);
    }

    current_task->cpu_ticks++;

    uint32_t now = timer_get_ticks();
    wake_sleepers(now);
    check_alarms(now);

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

    task_reap_waited_zombies();

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
    current_task->waited = false;
    current_task->esp = (uint32_t)frame;
    wake_waiters(current_task);
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

    // POSIX-ish: only allow waiting on direct children.
    if (target->ppid != current_task->id) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    if (target->state == TASK_STATE_ZOMBIE) {
        if (target->waited) {
            frame->eax = (uint32_t)-1;
            return frame;
        }

        int32_t code = target->exit_code;

        uint32_t irq_flags = irq_save();
        if (task_detach(target)) {
            task_reap_detached(target);
        }
        irq_restore(irq_flags);

        frame->eax = (uint32_t)code;
        return frame;
    }

    current_task->state = TASK_STATE_WAITING;
    current_task->wait_pid = pid;
    current_task->wait_status_user = NULL;
    current_task->wait_return_pid = false;
    current_task->esp = (uint32_t)frame;
    return tasking_yield(frame);
}

interrupt_frame_t* tasking_waitpid(interrupt_frame_t* frame, int32_t pid, void* status_user, int32_t options) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }
    if (!current_task->user) {
        frame->eax = (uint32_t)-EINVAL;
        return frame;
    }

    bool nohang = (options & 0x1) != 0; // WNOHANG (POSIX/LINUX value)

    if (pid == 0 || pid < -1) {
        frame->eax = (uint32_t)-EINVAL;
        return frame;
    }

    if (pid > 0) {
        task_t* target = task_find_by_pid((uint32_t)pid);
        if (!target || target->ppid != current_task->id || !target->user) {
            frame->eax = (uint32_t)-ECHILD;
            return frame;
        }

        if (target->state == TASK_STATE_ZOMBIE) {
            if (target->waited) {
                frame->eax = (uint32_t)-ECHILD;
                return frame;
            }

            if (status_user) {
                int32_t status = wait_encode_status(target->exit_code);
                if (!copy_to_user(status_user, &status, (uint32_t)sizeof(status))) {
                    frame->eax = (uint32_t)-EFAULT;
                    return frame;
                }
            }

            uint32_t irq_flags = irq_save();
            if (task_detach(target)) {
                task_reap_detached(target);
            }
            irq_restore(irq_flags);

            frame->eax = (uint32_t)pid;
            return frame;
        }

        if (nohang) {
            frame->eax = 0;
            return frame;
        }

        current_task->state = TASK_STATE_WAITING;
        current_task->wait_pid = (uint32_t)pid;
        current_task->wait_status_user = status_user;
        current_task->wait_return_pid = true;
        current_task->esp = (uint32_t)frame;
        return tasking_yield(frame);
    }

    // pid == -1: wait for any child.
    bool any_child = false;
    task_t* zombie = NULL;
    task_t* t = current_task->next;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t || t == current_task) {
            break;
        }
        if (t->user && t->ppid == current_task->id && !t->waited) {
            any_child = true;
            if (t->state == TASK_STATE_ZOMBIE) {
                zombie = t;
                break;
            }
        }
        t = t->next;
    }

    if (zombie) {
        int32_t status = wait_encode_status(zombie->exit_code);
        if (status_user) {
            if (!copy_to_user(status_user, &status, (uint32_t)sizeof(status))) {
                frame->eax = (uint32_t)-EFAULT;
                return frame;
            }
        }

        uint32_t child_pid = zombie->id;
        uint32_t irq_flags = irq_save();
        if (task_detach(zombie)) {
            task_reap_detached(zombie);
        }
        irq_restore(irq_flags);

        frame->eax = child_pid;
        return frame;
    }

    if (!any_child) {
        frame->eax = (uint32_t)-ECHILD;
        return frame;
    }

    if (nohang) {
        frame->eax = 0;
        return frame;
    }

    current_task->state = TASK_STATE_WAITING;
    current_task->wait_pid = WAIT_ANY_PID;
    current_task->wait_status_user = status_user;
    current_task->wait_return_pid = true;
    current_task->esp = (uint32_t)frame;
    return tasking_yield(frame);
}

int32_t tasking_kill(int32_t pid, int32_t sig) {
    if (!enabled || !current_task) {
        return -EINVAL;
    }
    // pid > 0: target process
    // pid == 0: target caller's process group
    // pid < 0: target process group (-pid)
    bool target_group = false;
    uint32_t target_pgid = 0;
    uint32_t target_pid = 0;
    if (pid == 0) {
        target_group = true;
        target_pgid = current_task->pgid;
    } else if (pid < 0) {
        if ((uint32_t)pid == 0x80000000u) { // INT32_MIN
            return -EINVAL;
        }
        target_group = true;
        target_pgid = (uint32_t)(-pid);
    } else {
        target_pid = (uint32_t)pid;
    }
    if (sig < 0 || sig >= (int32_t)VOS_SIG_MAX) {
        return -EINVAL;
    }

    uint32_t flags = irq_save();

    task_t* t = current_task;
    bool any_match = false;
    bool any_signaled = false;
    bool any_perm_denied = false;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        bool match = false;
        if (target_group) {
            match = (target_pgid != 0 && t->pgid == target_pgid);
        } else {
            match = (t->id == target_pid);
        }

        if (match) {
            any_match = true;
            if (!t->user) {
                any_perm_denied = true;
            } else if (current_task->uid != 0u && t->uid != current_task->uid) {
                any_perm_denied = true;
            } else if (sig != 0) {
                task_queue_signal(t, sig);
                any_signaled = true;
                if (!target_group) {
                    break;
                }
            } else {
                any_signaled = true;
                if (!target_group) {
                    break;
                }
            }
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }

    irq_restore(flags);
    if (!any_match) {
        return -ESRCH;
    }
    if (any_signaled) {
        return 0;
    }
    if (any_perm_denied) {
        return -EPERM;
    }
    return -ESRCH;
}

int32_t tasking_setpgid(int32_t pid, int32_t pgid) {
    if (!enabled || !current_task) {
        return -EINVAL;
    }

    if (pid == 0) {
        pid = (int32_t)current_task->id;
    }
    if (pgid == 0) {
        pgid = pid;
    }
    if (pid < 0 || pgid < 0) {
        return -EINVAL;
    }

    uint32_t upid = (uint32_t)pid;
    uint32_t upgid = (uint32_t)pgid;

    uint32_t flags = irq_save();

    task_t* target = task_find_by_pid(upid);
    if (!target) {
        irq_restore(flags);
        return -ESRCH;
    }
    if (!target->user) {
        irq_restore(flags);
        return -EPERM;
    }

    if (current_task->uid != 0u && target->uid != current_task->uid) {
        irq_restore(flags);
        return -EPERM;
    }

    // Only allow changing our own pgid or that of a direct child.
    if (target != current_task && target->ppid != current_task->id) {
        irq_restore(flags);
        return -EPERM;
    }

    // POSIX: pgid must refer to an existing process group, or create a new one
    // whose ID equals pid.
    if (upgid != upid && !task_find_any_by_pgid(upgid)) {
        irq_restore(flags);
        return -ESRCH;
    }

    target->pgid = upgid;
    irq_restore(flags);
    return 0;
}

static bool sig_default_ignore(int32_t sig) {
    return sig == VOS_SIGWINCH || sig == VOS_SIGCHLD;
}

int32_t tasking_signal_set_handler(int32_t sig, uint32_t handler, uint32_t* out_old) {
    if (!enabled || !current_task || !current_task->user) {
        return -EINVAL;
    }
    if (sig <= 0 || sig >= (int32_t)VOS_SIG_MAX) {
        return -EINVAL;
    }
    if (sig == VOS_SIGKILL || sig == VOS_SIGSTOP) {
        return -EINVAL;
    }
    if (out_old) {
        *out_old = current_task->sig_handlers[sig];
    }
    current_task->sig_handlers[sig] = handler;
    return 0;
}

int32_t tasking_sigprocmask(int32_t how, const void* set_user, void* old_user) {
    if (!enabled || !current_task || !current_task->user) {
        return -EINVAL;
    }

    uint32_t old = current_task->sig_mask;
    if (old_user != NULL) {
        if (!copy_to_user(old_user, &old, sizeof(old))) {
            return -EFAULT;
        }
    }

    if (set_user == NULL) {
        return 0;
    }

    uint32_t set = 0;
    if (!copy_from_user(&set, set_user, sizeof(set))) {
        return -EFAULT;
    }

    // Don't allow blocking uncatchable signals.
    set &= ~(1u << VOS_SIGKILL);
    set &= ~(1u << VOS_SIGSTOP);

    if (how == 0 /* SIG_SETMASK */) {
        current_task->sig_mask = set;
    } else if (how == 1 /* SIG_BLOCK */) {
        current_task->sig_mask |= set;
    } else if (how == 2 /* SIG_UNBLOCK */) {
        current_task->sig_mask &= ~set;
    } else {
        return -EINVAL;
    }

    // Ensure uncatchable signals are always unmasked.
    current_task->sig_mask &= ~(1u << VOS_SIGKILL);
    current_task->sig_mask &= ~(1u << VOS_SIGSTOP);
    return 0;
}

interrupt_frame_t* tasking_sigreturn(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame || !current_task->user || !frame_from_user(frame)) {
        return frame;
    }

    uint32_t user_esp = frame_get_user_esp(frame);
    vos_sigframe_t sf;
    if (!copy_from_user(&sf, (const void*)user_esp, (uint32_t)sizeof(sf))) {
        return tasking_exit(frame, -EFAULT);
    }
    if (sf.magic != VOS_SIGFRAME_MAGIC) {
        return tasking_exit(frame, -EINVAL);
    }

    current_task->sig_mask = sf.saved_mask;
    current_task->sig_mask &= ~(1u << VOS_SIGKILL);
    current_task->sig_mask &= ~(1u << VOS_SIGSTOP);

    memcpy(frame, &sf.frame, sizeof(*frame));
    frame_set_user_esp(frame, sf.user_esp);
    frame_set_user_ss(frame, sf.user_ss);
    return frame;
}

interrupt_frame_t* tasking_deliver_pending_signals(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame || !current_task->user || !frame_from_user(frame)) {
        return frame;
    }

    // Deferred kill takes precedence (e.g. SIGKILL).
    if (current_task->kill_pending) {
        return tasking_exit(frame, current_task->kill_exit_code);
    }

    uint32_t pending = current_task->sig_pending & ~current_task->sig_mask;
    if (pending == 0) {
        return frame;
    }

    int32_t sig = -1;
    for (int32_t i = 1; i < (int32_t)VOS_SIG_MAX; i++) {
        if ((pending & (1u << (uint32_t)i)) != 0) {
            sig = i;
            break;
        }
    }
    if (sig < 0) {
        return frame;
    }

    // Consume the pending bit now; if delivery fails we'll terminate.
    current_task->sig_pending &= ~(1u << (uint32_t)sig);

    uint32_t handler = current_task->sig_handlers[sig];
    if (handler == VOS_SIG_IGN) {
        return frame;
    }
    if (handler == VOS_SIG_DFL) {
        if (sig_default_ignore(sig)) {
            return frame;
        }
        return tasking_exit(frame, 128 + sig);
    }

    // Build a minimal signal trampoline on the user stack:
    //   handler(sig) -> (return) -> stub -> SYS_SIGRETURN
    uint32_t old_user_esp = frame_get_user_esp(frame);
    uint32_t old_user_ss = frame_get_user_ss(frame);

    uint8_t stub[] = {
        0x83, 0xC4, 0x04,             // add esp, 4   (pop sig arg)
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, imm32 (SYS_SIGRETURN)
        0xCD, 0x80,                   // int 0x80
        0x0F, 0x0B,                   // ud2
    };

    // The syscall number is defined in kernel/syscall.c; keep this in sync.
    const uint32_t SYS_SIGRETURN = 56u;
    stub[4] = (uint8_t)(SYS_SIGRETURN & 0xFFu);
    stub[5] = (uint8_t)((SYS_SIGRETURN >> 8) & 0xFFu);
    stub[6] = (uint8_t)((SYS_SIGRETURN >> 16) & 0xFFu);
    stub[7] = (uint8_t)((SYS_SIGRETURN >> 24) & 0xFFu);

    vos_sigframe_t sf;
    memset(&sf, 0, sizeof(sf));
    sf.magic = VOS_SIGFRAME_MAGIC;
    sf.sig = (uint32_t)sig;
    sf.saved_mask = current_task->sig_mask;
    memcpy(&sf.frame, frame, sizeof(*frame));
    sf.user_esp = old_user_esp;
    sf.user_ss = old_user_ss;

    // Block this signal while inside the handler to avoid trivial recursion.
    current_task->sig_mask |= (1u << (uint32_t)sig);

    uint32_t total = 8u + (uint32_t)sizeof(sf) + (uint32_t)sizeof(stub);
    uint32_t new_esp = old_user_esp - total;

    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    if (new_esp < stack_guard_bottom + PAGE_SIZE) {
        return tasking_exit(frame, -EFAULT);
    }

    // Layout:
    //  [new_esp+0]  ret -> stub
    //  [new_esp+4]  sig argument
    //  [new_esp+8]  sigframe
    //  [..]         stub bytes
    uint32_t stub_addr = new_esp + 8u + (uint32_t)sizeof(sf);
    uint32_t ret = stub_addr;

    uint8_t buf[256];
    if (total > (uint32_t)sizeof(buf)) {
        return tasking_exit(frame, -EFAULT);
    }
    memset(buf, 0, sizeof(buf));
    memcpy(&buf[0], &ret, sizeof(ret));
    uint32_t sig_u32 = (uint32_t)sig;
    memcpy(&buf[4], &sig_u32, sizeof(sig_u32));
    memcpy(&buf[8], &sf, sizeof(sf));
    memcpy(&buf[8u + sizeof(sf)], stub, sizeof(stub));

    if (!copy_to_user((void*)new_esp, buf, total)) {
        return tasking_exit(frame, -EFAULT);
    }

    frame_set_user_esp(frame, new_esp);
    frame_set_user_ss(frame, old_user_ss);
    frame->eip = handler;
    return frame;
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

    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;

    if (new_brk < USER_BASE || new_brk < current_task->user_brk_min || new_brk > stack_guard_bottom) {
        frame->eax = (uint32_t)-1;
        return frame;
    }

    uint32_t irq_flags = irq_save();

    if (increment > 0) {
        uint32_t start = (old_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
        uint32_t end = (new_brk + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);

        // Allocate any required page tables before allocating physical frames for the
        // heap pages, otherwise early_alloc() page tables could overlap frames.
        if (end > start) {
            paging_prepare_range(start, end - start, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        }

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

// -----------------------------
// User virtual memory mappings
// -----------------------------

// mmap prot bits (POSIX-ish).
enum {
    VOS_PROT_READ  = 0x1u,
    VOS_PROT_WRITE = 0x2u,
    VOS_PROT_EXEC  = 0x4u,
};

// mmap flags (Linux-compatible values where practical).
enum {
    VOS_MAP_SHARED    = 0x01u,
    VOS_MAP_PRIVATE   = 0x02u,
    VOS_MAP_FIXED     = 0x10u,
    VOS_MAP_ANONYMOUS = 0x20u,
};

// vfs_lseek whence values (match kernel/vfs_posix.c).
enum {
    VOS_SEEK_SET = 0,
    VOS_SEEK_CUR = 1,
    VOS_SEEK_END = 2,
};

static uint32_t u32_align_down(uint32_t v, uint32_t a) {
    return v & ~(a - 1u);
}

static uint32_t u32_align_up(uint32_t v, uint32_t a) {
    return (v + a - 1u) & ~(a - 1u);
}

static bool vm_overlap_any(const vm_area_t* head, uint32_t start, uint32_t end) {
    const vm_area_t* cur = head;
    while (cur) {
        uint32_t a = cur->start;
        uint32_t b = cur->start + cur->size;
        if (b < a) {
            // Corrupt entry; treat as overlap to avoid mapping over random memory.
            return true;
        }
        if (start < b && a < end) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}

static void vm_insert_sorted(task_t* t, vm_area_t* node) {
    if (!t || !node) {
        return;
    }
    if (!t->vm_areas || node->start < t->vm_areas->start) {
        node->next = t->vm_areas;
        t->vm_areas = node;
        return;
    }
    vm_area_t* cur = t->vm_areas;
    while (cur->next && cur->next->start <= node->start) {
        cur = cur->next;
    }
    node->next = cur->next;
    cur->next = node;
}

static void user_unmap_pages(uint32_t start, uint32_t end) {
    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        uint32_t paddr = 0;
        if (paging_unmap_page(va, &paddr) && paddr) {
            pmm_free_frame(paddr);
        }
    }
}

static int32_t user_map_zero_pages(uint32_t start, uint32_t end, uint32_t map_flags) {
    paging_prepare_range(start, end - start, map_flags);

    uint32_t va = start;
    for (; va < end; va += PAGE_SIZE) {
        uint32_t frame_paddr = pmm_alloc_frame();
        if (frame_paddr == 0) {
            break;
        }
        paging_map_page(va, frame_paddr, map_flags);
        memset((void*)va, 0, PAGE_SIZE);
    }

    if (va != end) {
        user_unmap_pages(start, va);
        return -ENOMEM;
    }
    return 0;
}

static int32_t tasking_mprotect_pages(uint32_t start, uint32_t end, uint32_t prot) {
    uint32_t* dir = current_task ? current_task->page_directory : NULL;
    if (!dir) {
        return -EINVAL;
    }

    bool writable = (prot & VOS_PROT_WRITE) != 0;

    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        uint32_t dir_index = (va >> 22) & 0x3FFu;
        uint32_t tbl_index = (va >> 12) & 0x3FFu;

        uint32_t pde = dir[dir_index];
        if ((pde & PAGE_PRESENT) == 0 || (pde & PAGE_USER) == 0) {
            return -EFAULT;
        }
        uint32_t* table = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t pte = table[tbl_index];
        if ((pte & PAGE_PRESENT) == 0 || (pte & PAGE_USER) == 0) {
            return -EFAULT;
        }

        if (writable) {
            pte |= PAGE_RW;
        } else {
            pte &= ~PAGE_RW;
        }
        table[tbl_index] = pte;
        __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
    }
    return 0;
}

int32_t tasking_mmap(uint32_t addr_hint, uint32_t length, uint32_t prot, uint32_t flags, int32_t fd, uint32_t offset, uint32_t* out_addr) {
    if (!enabled || !current_task || !current_task->user) {
        return -EINVAL;
    }
    if (!out_addr) {
        return -EINVAL;
    }
    *out_addr = 0;

    if (length == 0) {
        return -EINVAL;
    }
    if ((flags & (VOS_MAP_PRIVATE | VOS_MAP_SHARED)) == 0) {
        return -EINVAL;
    }

    bool anonymous = (flags & VOS_MAP_ANONYMOUS) != 0;
    vfs_handle_t* file = NULL;
    uint32_t file_size = 0;
    uint32_t file_off_saved = 0;

    if (anonymous) {
        if (fd != -1) {
            return -EINVAL;
        }
        if (offset != 0) {
            return -EINVAL;
        }
    } else {
        if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
            return -EBADF;
        }
        if (offset != 0) {
            // Offset support would require passing it through the syscall ABI.
            return -EINVAL;
        }

        uint32_t f = irq_save();
        fd_entry_t* ent = &current_task->fds[fd];
        if (ent->kind == FD_KIND_VFS) {
            file = ent->handle;
        }
        irq_restore(f);

        if (!file) {
            return -EBADF;
        }

        vfs_stat_t st;
        int32_t rc = vfs_fstat(file, &st);
        if (rc < 0) {
            return rc;
        }
        if (st.is_dir) {
            return -EISDIR;
        }
        file_size = st.size;

        // Save current file offset so mmap does not disturb it.
        rc = vfs_lseek(file, 0, VOS_SEEK_CUR, &file_off_saved);
        if (rc < 0) {
            return rc;
        }
    }

    uint32_t size = u32_align_up(length, PAGE_SIZE);
    if (size == 0) {
        return -EINVAL;
    }

    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    uint32_t user_max = stack_guard_bottom;
    if (user_max < USER_BASE || user_max > USER_LIMIT) {
        return -EINVAL;
    }

    uint32_t start = 0;
    if ((flags & VOS_MAP_FIXED) != 0) {
        if (addr_hint == 0) {
            return -EINVAL;
        }
        start = u32_align_down(addr_hint, PAGE_SIZE);
        uint32_t end = start + size;
        if (end < start) {
            return -EINVAL;
        }
        if (start < USER_BASE || end > user_max) {
            return -EINVAL;
        }
        if (start < current_task->user_brk) {
            return -EINVAL;
        }
        if (vm_overlap_any(current_task->vm_areas, start, end)) {
            return -EINVAL;
        }
    } else {
        uint32_t top = current_task->mmap_top;
        if (top == 0) {
            top = user_max;
        }
        // Ensure we don't collide with the current heap.
        if (top <= current_task->user_brk + size) {
            return -ENOMEM;
        }
        start = u32_align_down(top - size, PAGE_SIZE);
        if (start < USER_BASE || start + size > user_max) {
            return -ENOMEM;
        }
    }

    uint32_t map_flags = PAGE_PRESENT | PAGE_USER;
    if ((prot & VOS_PROT_WRITE) != 0) {
        map_flags |= PAGE_RW;
    }

    uint32_t irq_flags = irq_save();
    int32_t rc = user_map_zero_pages(start, start + size, map_flags);
    if (rc < 0) {
        irq_restore(irq_flags);
        return rc;
    }

    vm_area_t* node = (vm_area_t*)kmalloc(sizeof(*node));
    if (!node) {
        user_unmap_pages(start, start + size);
        irq_restore(irq_flags);
        return -ENOMEM;
    }
    memset(node, 0, sizeof(*node));
    node->start = start;
    node->size = size;
    node->prot = prot;
    node->next = NULL;
    vm_insert_sorted(current_task, node);

    if ((flags & VOS_MAP_FIXED) == 0) {
        current_task->mmap_top = start;
    }

    irq_restore(irq_flags);
    *out_addr = start;

    if (!anonymous) {
        // Eagerly copy the file contents into the mapping (MAP_SHARED behaves
        // like MAP_PRIVATE for now; no writeback).
        uint32_t to_copy = length;
        if (to_copy > file_size) {
            to_copy = file_size;
        }
        if (to_copy != 0) {
            int32_t rc = vfs_lseek(file, 0, VOS_SEEK_SET, NULL);
            if (rc < 0) {
                (void)tasking_munmap(start, size);
                return rc;
            }

            uint32_t copied = 0;
            uint8_t tmp[512];
            while (copied < to_copy) {
                uint32_t want = to_copy - copied;
                if (want > (uint32_t)sizeof(tmp)) {
                    want = (uint32_t)sizeof(tmp);
                }

                uint32_t got = 0;
                rc = vfs_read(file, tmp, want, &got);
                if (rc < 0) {
                    (void)vfs_lseek(file, (int32_t)file_off_saved, VOS_SEEK_SET, NULL);
                    (void)tasking_munmap(start, size);
                    return rc;
                }
                if (got == 0) {
                    break;
                }

                if (!copy_to_user((void*)(start + copied), tmp, got)) {
                    (void)vfs_lseek(file, (int32_t)file_off_saved, VOS_SEEK_SET, NULL);
                    (void)tasking_munmap(start, size);
                    return -EFAULT;
                }
                copied += got;
            }

            (void)vfs_lseek(file, (int32_t)file_off_saved, VOS_SEEK_SET, NULL);
        }
    }

    return 0;
}

int32_t tasking_munmap(uint32_t addr, uint32_t length) {
    if (!enabled || !current_task || !current_task->user) {
        return -EINVAL;
    }
    if (length == 0) {
        return -EINVAL;
    }
    if ((addr & (PAGE_SIZE - 1u)) != 0) {
        return -EINVAL;
    }

    uint32_t start = addr;
    uint32_t end = addr + length;
    if (end < start) {
        return -EINVAL;
    }
    start = u32_align_down(start, PAGE_SIZE);
    end = u32_align_up(end, PAGE_SIZE);

    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    if (start < USER_BASE || end > stack_guard_bottom) {
        return -EINVAL;
    }

    uint32_t irq_flags = irq_save();

    vm_area_t* prev = NULL;
    vm_area_t* cur = current_task->vm_areas;
    while (cur) {
        uint32_t a = cur->start;
        uint32_t b = cur->start + cur->size;
        if (b < a) {
            break;
        }
        if (b <= start) {
            prev = cur;
            cur = cur->next;
            continue;
        }
        if (a >= end) {
            break;
        }

        uint32_t u0 = (a > start) ? a : start;
        uint32_t u1 = (b < end) ? b : end;
        if (u1 > u0) {
            user_unmap_pages(u0, u1);
        }

        if (u0 == a && u1 == b) {
            vm_area_t* next = cur->next;
            if (prev) {
                prev->next = next;
            } else {
                current_task->vm_areas = next;
            }
            kfree(cur);
            cur = next;
            continue;
        }

        if (u0 == a) {
            cur->start = u1;
            cur->size = b - u1;
            prev = cur;
            cur = cur->next;
            continue;
        }

        if (u1 == b) {
            cur->size = u0 - a;
            prev = cur;
            cur = cur->next;
            continue;
        }

        // Split the region into two.
        vm_area_t* tail = (vm_area_t*)kmalloc(sizeof(*tail));
        if (!tail) {
            irq_restore(irq_flags);
            return -ENOMEM;
        }
        memset(tail, 0, sizeof(*tail));
        tail->start = u1;
        tail->size = b - u1;
        tail->prot = cur->prot;
        tail->next = cur->next;

        cur->size = u0 - a;
        cur->next = tail;
        prev = tail;
        cur = tail->next;
    }

    irq_restore(irq_flags);
    return 0;
}

int32_t tasking_mprotect(uint32_t addr, uint32_t length, uint32_t prot) {
    if (!enabled || !current_task || !current_task->user) {
        return -EINVAL;
    }
    if (length == 0) {
        return -EINVAL;
    }
    if ((addr & (PAGE_SIZE - 1u)) != 0) {
        return -EINVAL;
    }

    uint32_t start = addr;
    uint32_t end = addr + length;
    if (end < start) {
        return -EINVAL;
    }
    start = u32_align_down(start, PAGE_SIZE);
    end = u32_align_up(end, PAGE_SIZE);

    uint32_t stack_guard_bottom = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    if (start < USER_BASE || end > stack_guard_bottom) {
        return -EINVAL;
    }

    uint32_t irq_flags = irq_save();
    int32_t rc = tasking_mprotect_pages(start, end, prot);
    irq_restore(irq_flags);
    return rc;
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

    t->ppid = current_task->id;

    // Inherit the caller's current working directory and terminal settings
    // so userland behaves like a normal process tree.
    strncpy(t->cwd, current_task->cwd, sizeof(t->cwd) - 1u);
    t->cwd[sizeof(t->cwd) - 1u] = '\0';
    t->tty = current_task->tty;
    t->uid = current_task->uid;
    t->gid = current_task->gid;
    fd_inherit(t, current_task);

    task_append(t);
    return t->id;
}

bool tasking_spawn_user(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk) {
    return tasking_spawn_user_pid(entry, user_esp, page_directory, user_brk) != 0;
}

int32_t tasking_fork(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame) {
        return -EINVAL;
    }
    if (!current_task->user || !frame_from_user(frame)) {
        return -EPERM;
    }

    uint32_t irq_flags = irq_save();

    uint32_t* child_dir = fork_clone_user_directory(current_task);
    if (!child_dir) {
        irq_restore(irq_flags);
        return -ENOMEM;
    }

    vm_area_t* vm_clone = NULL;
    if (current_task->vm_areas) {
        vm_clone = vm_clone_areas(current_task->vm_areas);
        if (!vm_clone) {
            free_user_pages_in_directory(child_dir);
            irq_restore(irq_flags);
            return -ENOMEM;
        }
    }

    uint32_t stack_top_addr = 0;
    if (!kstack_alloc(&stack_top_addr)) {
        task_free_vm_areas(vm_clone);
        free_user_pages_in_directory(child_dir);
        irq_restore(irq_flags);
        return -ENOMEM;
    }

    // Copy the current user context into the child, adjusting EAX so the
    // fork() return value is 0 in the child.
    uint32_t frame_bytes = (uint32_t)sizeof(interrupt_frame_t) + 8u; // user esp + ss
    uint32_t child_sp_addr = stack_top_addr - frame_bytes;
    memcpy((void*)child_sp_addr, frame, frame_bytes);
    interrupt_frame_t* child_frame = (interrupt_frame_t*)child_sp_addr;
    child_frame->eax = 0;

    task_t* child = (task_t*)kmalloc(sizeof(task_t));
    if (!child) {
        task_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        tmp.kstack_top = stack_top_addr;
        task_free_kstack(&tmp);
        task_free_vm_areas(vm_clone);
        free_user_pages_in_directory(child_dir);
        irq_restore(irq_flags);
        return -ENOMEM;
    }

    memset(child, 0, sizeof(*child));
    child->id = ++next_id;
    child->ppid = current_task->id;
    child->pgid = current_task->pgid;
    child->esp = child_sp_addr;
    child->kstack_top = stack_top_addr;
    child->page_directory = child_dir;
    child->user = true;
    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->user_brk = current_task->user_brk;
    child->user_brk_min = current_task->user_brk_min;
    child->vm_areas = vm_clone;
    child->mmap_top = current_task->mmap_top;
    strncpy(child->cwd, current_task->cwd, sizeof(child->cwd) - 1u);
    child->cwd[sizeof(child->cwd) - 1u] = '\0';
    child->tty = current_task->tty;
    child->tty_line_len = 0;
    child->tty_line_off = 0;
    child->tty_line_ready = false;
    child->sig_pending = 0;
    child->sig_mask = current_task->sig_mask;
    memcpy(child->sig_handlers, current_task->sig_handlers, sizeof(child->sig_handlers));
    child->state = TASK_STATE_RUNNABLE;
    child->wake_tick = 0;
    child->wait_pid = 0;
    child->wait_status_user = NULL;
    child->wait_return_pid = false;
    child->exit_code = 0;
    child->waited = false;
    child->kill_pending = false;
    child->kill_exit_code = 0;
    child->alarm_tick = current_task->alarm_tick;
    child->cpu_ticks = 0;
    task_set_name(child, current_task->name);
    fd_clone(child, current_task);
    child->next = NULL;

    task_append(child);
    int32_t pid = (int32_t)child->id;
    irq_restore(irq_flags);
    return pid;
}

static void task_close_cloexec_fds(void) {
    if (!current_task) {
        return;
    }

    for (int32_t fd = 0; fd < (int32_t)TASK_MAX_FDS; fd++) {
        uint32_t flags = current_task->fds[fd].fd_flags;
        if ((flags & VOS_FD_CLOEXEC) != 0) {
            (void)tasking_fd_close(fd);
        }
    }
}

int32_t tasking_execve(interrupt_frame_t* frame, const char* path, const char* const* argv, uint32_t argc) {
    if (!enabled || !current_task || !frame) {
        return -EINVAL;
    }
    if (!current_task->user || !frame_from_user(frame)) {
        return -EPERM;
    }
    if (!path) {
        return -EINVAL;
    }
    if (argc > VOS_EXEC_MAX_ARGS) {
        return -EINVAL;
    }

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

    const char* argv0 = abs;
    const char* const* kargv = argv;
    uint32_t kargc = argc;
    if (argc == 0 || !argv) {
        kargv = &argv0;
        kargc = 1u;
    }

    uint32_t entry = 0;
    uint32_t user_esp = 0;
    uint32_t brk = 0;
    uint32_t* user_dir = paging_create_user_directory();
    if (!user_dir) {
        kfree(image);
        return -ENOMEM;
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

    // Close file descriptors flagged close-on-exec.
    task_close_cloexec_fds();

    // Tear down the previous user image.
    uint32_t* old_dir = current_task->page_directory;
    vm_area_t* old_areas = current_task->vm_areas;
    current_task->vm_areas = NULL;

    current_task->page_directory = user_dir;
    current_task->user_brk = brk;
    current_task->user_brk_min = brk;
    current_task->mmap_top = USER_STACK_TOP - (USER_STACK_PAGES + 1u) * PAGE_SIZE;
    current_task->sig_pending = 0;
    current_task->kill_pending = false;
    current_task->kill_exit_code = 0;
    current_task->wait_pid = 0;
    current_task->wait_status_user = NULL;
    current_task->wait_return_pid = false;

    // Switch to the new address space and update the user context to start
    // executing the new program.
    paging_switch_directory(user_dir);
    frame->eax = 0;
    frame->eip = entry;
    frame_set_user_esp(frame, user_esp);

    // Free user pages from the old address space and any mmap metadata.
    free_user_pages_in_directory(old_dir);
    task_free_vm_areas(old_areas);

    return 0;
}

int32_t tasking_spawn_exec(const char* path, const char* const* argv, uint32_t argc) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    if (argc > VOS_EXEC_MAX_ARGS) {
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

    const char* argv0 = abs;
    const char* const* kargv = argv;
    uint32_t kargc = argc;
    if (argc == 0 || !argv) {
        kargv = &argv0;
        kargc = 1u;
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
            current_task->fds[fd].fd_flags = 0;
            current_task->fds[fd].fl_flags = flags;
            current_task->fds[fd].handle = h;
            current_task->fds[fd].pipe = NULL;
            current_task->fds[fd].pipe_write_end = false;
            current_task->fds[fd].pending_len = 0;
            current_task->fds[fd].pending_off = 0;
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
    ent->fd_flags = 0;
    ent->fl_flags = 0;
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
    if (tty_foreground_pgid == 0) {
        return false;
    }

    task_t* fg = task_find_any_by_pgid(tty_foreground_pgid);
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

    // Deliver SIGINT to the foreground process group.
    (void)tasking_kill(-(int32_t)tty_foreground_pgid, 2);
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
        case KEY_F1:
            seq[0] = 0x1Bu; seq[1] = 'O'; seq[2] = 'P';
            return 3;
        case KEY_F2:
            seq[0] = 0x1Bu; seq[1] = 'O'; seq[2] = 'Q';
            return 3;
        case KEY_F3:
            seq[0] = 0x1Bu; seq[1] = 'O'; seq[2] = 'R';
            return 3;
        case KEY_F4:
            seq[0] = 0x1Bu; seq[1] = 'O'; seq[2] = 'S';
            return 3;
        case KEY_F5:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '1'; seq[3] = '5'; seq[4] = '~';
            return 5;
        case KEY_F6:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '1'; seq[3] = '7'; seq[4] = '~';
            return 5;
        case KEY_F7:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '1'; seq[3] = '8'; seq[4] = '~';
            return 5;
        case KEY_F8:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '1'; seq[3] = '9'; seq[4] = '~';
            return 5;
        case KEY_F9:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '2'; seq[3] = '0'; seq[4] = '~';
            return 5;
        case KEY_F10:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '2'; seq[3] = '1'; seq[4] = '~';
            return 5;
        case KEY_F11:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '2'; seq[3] = '3'; seq[4] = '~';
            return 5;
        case KEY_F12:
            seq[0] = 0x1Bu; seq[1] = '['; seq[2] = '2'; seq[3] = '4'; seq[4] = '~';
            return 5;
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

static int32_t tty_read_canonical(void* dst_user, uint32_t len, bool nonblock) {
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

    if (nonblock) {
        return -EAGAIN;
    }

    // Start a fresh line.
    current_task->tty_line_len = 0;
    current_task->tty_line_off = 0;
    current_task->tty_line_ready = false;

    for (;;) {
        if (tasking_current_should_interrupt()) {
            return -EINTR;
        }
        int8_t key = (int8_t)keyboard_getchar(); // blocks
        if (key == 0 && tasking_current_should_interrupt()) {
            return -EINTR;
        }

        if (screen_scrollback_active()) {
            screen_scrollback_reset();
        }

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

static bool tty_try_getchar_any(char* out) {
    if (!out) {
        return false;
    }
    if (keyboard_try_getchar(out)) {
        return true;
    }
    if (serial_try_read_char(out)) {
        return true;
    }
    return false;
}

// Wait up to timeout_ticks for a character from the keyboard buffer or COM1.
// Returns true if a character was read into *out, false on timeout.
static bool tty_wait_getchar_timeout(uint32_t timeout_ticks, char* out) {
    if (!out) {
        return false;
    }

    uint32_t hz = timer_get_hz();
    uint32_t start = timer_get_ticks();
    uint32_t deadline = start + timeout_ticks;

    bool were_enabled = irq_are_enabled();
    if (!were_enabled) {
        sti();
    }

    for (;;) {
        if (tty_try_getchar_any(out)) {
            if (!were_enabled) {
                cli();
            }
            return true;
        }
        if (tasking_current_should_interrupt()) {
            if (!were_enabled) {
                cli();
            }
            return false;
        }
        if (timeout_ticks == 0 || hz == 0) {
            if (!were_enabled) {
                cli();
            }
            return false;
        }
        if ((int32_t)(timer_get_ticks() - deadline) >= 0) {
            if (!were_enabled) {
                cli();
            }
            return false;
        }
        hlt();
        keyboard_idle_poll();
    }
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
        uint32_t fl_flags = ent->fl_flags;
        irq_restore(irq_flags);

        bool nonblock = (fl_flags & VOS_O_NONBLOCK) != 0;
        bool canon = (current_task->tty.c_lflag & VOS_ICANON) != 0;
        if (canon) {
            return tty_read_canonical(dst_user, len, nonblock);
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

            if (read == 0 && screen_scrollback_active()) {
                screen_scrollback_reset();
            }

            if (!copy_to_user(dst + read, &b, 1u)) {
                return (read != 0) ? (int32_t)read : -EFAULT;
            }
            read++;
        }

        // Non-canonical timeout semantics (partial, but enough for ports like ne):
        // - VMIN=0, VTIME=0: non-blocking poll
        // - VMIN=0, VTIME>0: wait up to VTIME*0.1s for the first byte, then return
        if (nonblock) {
            vmin = 0;
            vtime = 0;
        }
        const bool poll_mode = (vmin == 0 && vtime == 0);
        const bool first_byte_timeout = (vmin == 0 && vtime != 0);
        bool block = (read == 0) && !poll_mode && !first_byte_timeout;

        uint32_t first_timeout_ticks = 0;
        if (first_byte_timeout) {
            uint32_t hz = timer_get_hz();
            if (hz != 0) {
                // VTIME is in tenths of a second.
                // Round up so small timeouts still wait at least one tick.
                first_timeout_ticks = ((uint32_t)vtime * hz + 9u) / 10u;
                if (first_timeout_ticks == 0) {
                    first_timeout_ticks = 1;
                }
            }
        }
        while (read < len) {
            if (tasking_current_should_interrupt()) {
                return (read != 0) ? (int32_t)read : -EINTR;
            }
            char c = 0;
            if (block) {
                c = keyboard_getchar(); // guarantees progress (also checks serial)
            } else if (poll_mode || read != 0) {
                if (!tty_try_getchar_any(&c)) {
                    break;
                }
            } else if (first_byte_timeout) {
                if (!tty_wait_getchar_timeout(first_timeout_ticks, &c)) {
                    if (tasking_current_should_interrupt()) {
                        return -EINTR;
                    }
                    break;
                }
            } else {
                if (!tty_try_getchar_any(&c)) {
                    break;
                }
            }
            if (c == 0 && tasking_current_should_interrupt()) {
                return (read != 0) ? (int32_t)read : -EINTR;
            }
            block = false;

            if (screen_scrollback_active()) {
                screen_scrollback_reset();
            }

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

        if (nonblock && read == 0) {
            return -EAGAIN;
        }
        return (int32_t)read;
    }

    if (ent->kind == FD_KIND_PIPE && ent->pipe) {
        pipe_obj_t* p = ent->pipe;
        uint32_t fl_flags = ent->fl_flags;
        irq_restore(irq_flags);

        uint32_t total = 0;
        uint8_t tmp[128];
        while (total < len) {
            if (tasking_current_should_interrupt()) {
                return (total != 0) ? (int32_t)total : -EINTR;
            }
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
            if ((fl_flags & VOS_O_NONBLOCK) != 0) {
                return -EAGAIN;
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
    uint32_t fl_flags = ent->fl_flags;
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
            if (tasking_current_should_interrupt()) {
                return (total != 0) ? (int32_t)total : -EINTR;
            }
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
            if ((fl_flags & VOS_O_NONBLOCK) != 0) {
                return -EAGAIN;
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

int32_t tasking_lstat(const char* path, void* st_user) {
    if (!current_task || !path || !st_user) {
        return -EINVAL;
    }

    vfs_stat_t st;
    int32_t rc = vfs_lstat_path(current_task->cwd, path, &st);
    if (rc < 0) {
        return rc;
    }
    if (!copy_to_user(st_user, &st, (uint32_t)sizeof(st))) {
        return -EFAULT;
    }
    return 0;
}

int32_t tasking_statfs(const char* path, void* st_user) {
    if (!current_task || !path || !st_user) {
        return -EINVAL;
    }

    vfs_statfs_t st;
    int32_t rc = vfs_statfs_path(current_task->cwd, path, &st);
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

int32_t tasking_symlink(const char* target, const char* linkpath) {
    if (!current_task || !target || !linkpath) {
        return -EINVAL;
    }
    return vfs_symlink_path(current_task->cwd, target, linkpath);
}

int32_t tasking_readlink(const char* path, void* dst_user, uint32_t cap) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    if (cap != 0 && !dst_user) {
        return -EFAULT;
    }

    uint32_t kcap = cap;
    if (kcap > 4096u) {
        kcap = 4096u;
    }

    char* tmp = (char*)kmalloc(kcap ? kcap : 1u);
    if (!tmp) {
        return -ENOMEM;
    }

    int32_t n = vfs_readlink_path(current_task->cwd, path, tmp, kcap);
    if (n < 0) {
        kfree(tmp);
        return n;
    }

    if (n != 0 && !copy_to_user(dst_user, tmp, (uint32_t)n)) {
        kfree(tmp);
        return -EFAULT;
    }

    kfree(tmp);
    return n;
}

int32_t tasking_chmod(const char* path, uint16_t mode) {
    if (!current_task || !path) {
        return -EINVAL;
    }
    return vfs_chmod_path(current_task->cwd, path, mode);
}

int32_t tasking_fd_fchmod(int32_t fd, uint16_t mode) {
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
    return vfs_fchmod(h, mode);
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
    dst->fd_flags = 0; // dup() clears close-on-exec
    dst->fl_flags = src->fl_flags;
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
    dst->fd_flags = 0; // dup2() clears close-on-exec
    dst->fl_flags = src->fl_flags;
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

int32_t tasking_fd_fcntl(int32_t fd, int32_t cmd, int32_t arg) {
    if (!current_task) {
        return -EINVAL;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -EBADF;
    }

    if (cmd == VOS_F_DUPFD || cmd == VOS_F_DUPFD_CLOEXEC) {
        int32_t minfd = arg;
        if (minfd < 0) {
            minfd = 0;
        }
        if (minfd >= (int32_t)TASK_MAX_FDS) {
            return -EINVAL;
        }

        vfs_handle_t* h = NULL;
        pipe_obj_t* p = NULL;
        bool pipe_we = false;

        uint32_t irq_flags = irq_save();
        fd_entry_t* src = &current_task->fds[fd];
        if (src->kind == FD_KIND_FREE) {
            irq_restore(irq_flags);
            return -EBADF;
        }

        int32_t newfd = -1;
        for (int32_t cand = minfd; cand < (int32_t)TASK_MAX_FDS; cand++) {
            if (current_task->fds[cand].kind == FD_KIND_FREE) {
                newfd = cand;
                break;
            }
        }
        if (newfd < 0) {
            irq_restore(irq_flags);
            return -EMFILE;
        }

        fd_entry_t* dst = &current_task->fds[newfd];
        dst->kind = src->kind;
        dst->fd_flags = (cmd == VOS_F_DUPFD_CLOEXEC) ? VOS_FD_CLOEXEC : 0;
        dst->fl_flags = src->fl_flags;
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

    uint32_t irq_flags = irq_save();
    fd_entry_t* ent = &current_task->fds[fd];
    if (ent->kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -EBADF;
    }

    switch (cmd) {
        case VOS_F_GETFD: {
            int32_t v = (int32_t)ent->fd_flags;
            irq_restore(irq_flags);
            return v;
        }
        case VOS_F_SETFD:
            ent->fd_flags = (uint32_t)arg & VOS_FD_CLOEXEC;
            irq_restore(irq_flags);
            return 0;
        case VOS_F_GETFL: {
            if (ent->kind == FD_KIND_VFS && ent->handle) {
                uint32_t v = vfs_handle_flags(ent->handle);
                irq_restore(irq_flags);
                return (int32_t)v;
            }
            int32_t v = (int32_t)ent->fl_flags;
            irq_restore(irq_flags);
            return v;
        }
        case VOS_F_SETFL: {
            uint32_t mask = VOS_O_APPEND | VOS_O_NONBLOCK;
            if (ent->kind == FD_KIND_VFS && ent->handle) {
                uint32_t old = vfs_handle_flags(ent->handle);
                uint32_t next = (old & ~mask) | ((uint32_t)arg & mask);
                (void)vfs_handle_set_flags(ent->handle, next);
                ent->fl_flags = next;
                irq_restore(irq_flags);
                return 0;
            }

            uint32_t old = ent->fl_flags;
            ent->fl_flags = (old & ~mask) | ((uint32_t)arg & mask);
            irq_restore(irq_flags);
            return 0;
        }
        default:
            irq_restore(irq_flags);
            return -EINVAL;
    }
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
    r->fd_flags = 0;
    r->fl_flags = 0; // O_RDONLY
    r->handle = NULL;
    r->pipe = p;
    r->pipe_write_end = false;
    r->pending_len = 0;
    r->pending_off = 0;

    fd_entry_t* w = &current_task->fds[wfd];
    w->kind = FD_KIND_PIPE;
    w->fd_flags = 0;
    w->fl_flags = 1; // O_WRONLY
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
            uint32_t pgid = tty_foreground_pgid;
            if (!copy_to_user(argp_user, &pgid, (uint32_t)sizeof(pgid))) {
                return -EFAULT;
            }
            return 0;
        }
        case VOS_TIOCSPGRP: {
            uint32_t pgid = 0;
            if (!copy_from_user(&pgid, argp_user, (uint32_t)sizeof(pgid))) {
                return -EFAULT;
            }
            if (pgid == 0) {
                tty_foreground_pgid = 0;
                return 0;
            }
            task_t* fg = task_find_any_by_pgid(pgid);
            if (!fg) {
                return -ESRCH;
            }
            if (!fg->user) {
                return -EPERM;
            }
            if (current_task->uid != 0u && fg->uid != current_task->uid) {
                return -EPERM;
            }
            tty_foreground_pgid = pgid;
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

            // Only TCSAFLUSH (TCSETSF) flushes queued input. Programs like ne temporarily
            // tweak VMIN/VTIME to disambiguate escape sequences; dropping buffered bytes
            // here breaks multi-byte keys (arrows, function keys, etc.).
            if (req == VOS_TCSETSF) {
                irq_flags = irq_save();
                ent = &current_task->fds[fd];
                ent->pending_len = 0;
                ent->pending_off = 0;
                irq_restore(irq_flags);
                current_task->tty_line_len = 0;
                current_task->tty_line_off = 0;
                current_task->tty_line_ready = false;
            }
            return 0;
        }
        default:
            return -ENOTTY;
    }
}
