#include "task.h"
#include "kheap.h"
#include "string.h"
#include "gdt.h"
#include "timer.h"
#include "io.h"
#include "paging.h"
#include "pmm.h"
#include "usercopy.h"
#include "vfs.h"
#include "keyboard.h"

#define KSTACK_SIZE (16u * 1024u)
#define TASK_NAME_LEN 15
#define TASK_MAX_SCAN 256
#define KSTACK_REGION_BASE 0xF0000000u
#define TASK_MAX_FDS 16

typedef enum {
    FD_KIND_FREE = 0,
    FD_KIND_STDIN = 1,
    FD_KIND_STDOUT = 2,
    FD_KIND_STDERR = 3,
    FD_KIND_VFS_FILE = 4,
} fd_kind_t;

typedef struct fd_entry {
    fd_kind_t kind;
    const uint8_t* data;
    uint32_t size;
    uint32_t off;
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
    task_state_t state;
    uint32_t wake_tick;
    uint32_t wait_pid;
    int32_t exit_code;
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

static uint32_t* push32(uint32_t* sp, uint32_t v) {
    sp--;
    *sp = v;
    return sp;
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

bool tasking_kill(uint32_t pid, int32_t exit_code) {
    if (!enabled || !current_task || pid == 0) {
        return false;
    }
    if (pid == current_task->id) {
        return false;
    }

    uint32_t flags = irq_save();

    task_t* t = current_task;
    for (uint32_t i = 0; i < TASK_MAX_SCAN; i++) {
        if (!t) {
            break;
        }
        if (t->id == pid) {
            if (t->state != TASK_STATE_ZOMBIE) {
                t->state = TASK_STATE_ZOMBIE;
                t->exit_code = exit_code;
                wake_waiters(pid, exit_code);
            }
            irq_restore(flags);
            return true;
        }
        t = t->next;
        if (t == current_task) {
            break;
        }
    }

    irq_restore(flags);
    return false;
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
    task_append(t);
    return t->id;
}

bool tasking_spawn_user(uint32_t entry, uint32_t user_esp, uint32_t* page_directory, uint32_t user_brk) {
    return tasking_spawn_user_pid(entry, user_esp, page_directory, user_brk) != 0;
}

int32_t tasking_fd_open(const char* path, uint32_t flags) {
    (void)flags;
    if (!current_task || !path) {
        return -1;
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(path, &data, &size) || !data) {
        return -1;
    }

    uint32_t irq_flags = irq_save();
    for (int32_t fd = 3; fd < (int32_t)TASK_MAX_FDS; fd++) {
        if (current_task->fds[fd].kind == FD_KIND_FREE) {
            current_task->fds[fd].kind = FD_KIND_VFS_FILE;
            current_task->fds[fd].data = data;
            current_task->fds[fd].size = size;
            current_task->fds[fd].off = 0;
            irq_restore(irq_flags);
            return fd;
        }
    }
    irq_restore(irq_flags);
    return -1;
}

int32_t tasking_fd_close(int32_t fd) {
    if (!current_task) {
        return -1;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -1;
    }

    uint32_t irq_flags = irq_save();
    if (current_task->fds[fd].kind == FD_KIND_FREE) {
        irq_restore(irq_flags);
        return -1;
    }
    current_task->fds[fd].kind = FD_KIND_FREE;
    current_task->fds[fd].data = NULL;
    current_task->fds[fd].size = 0;
    current_task->fds[fd].off = 0;
    irq_restore(irq_flags);
    return 0;
}

int32_t tasking_fd_read(int32_t fd, void* dst_user, uint32_t len) {
    if (!current_task || !dst_user) {
        return -1;
    }
    if (fd < 0 || fd >= (int32_t)TASK_MAX_FDS) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    uint32_t irq_flags = irq_save();
    fd_entry_t ent = current_task->fds[fd];

    if (ent.kind == FD_KIND_STDIN) {
        irq_restore(irq_flags);

        uint8_t* dst = (uint8_t*)dst_user;
        uint32_t read = 0;

        // Block until we can return at least 1 byte.
        char c = keyboard_getchar();
        if (!copy_to_user(dst, &c, 1u)) {
            return -1;
        }
        dst++;
        read++;

        while (read < len) {
            char next = 0;
            if (!keyboard_try_getchar(&next)) {
                break;
            }
            if (!copy_to_user(dst, &next, 1u)) {
                return -1;
            }
            dst++;
            read++;
        }

        return (int32_t)read;
    }

    if (ent.kind != FD_KIND_VFS_FILE) {
        irq_restore(irq_flags);
        return -1;
    }
    if (!ent.data) {
        irq_restore(irq_flags);
        return -1;
    }

    uint32_t off = ent.off;
    uint32_t size = ent.size;
    const uint8_t* src_data = ent.data;

    if (off >= size) {
        irq_restore(irq_flags);
        return 0;
    }

    uint32_t avail = size - off;
    uint32_t to_copy = len;
    if (to_copy > avail) {
        to_copy = avail;
    }
    current_task->fds[fd].off = off + to_copy;
    irq_restore(irq_flags);

    uint32_t remaining = to_copy;
    const uint8_t* src = src_data + off;
    uint8_t* dst = (uint8_t*)dst_user;
    while (remaining) {
        uint32_t chunk = remaining;
        if (chunk > 256u) {
            chunk = 256u;
        }
        if (!copy_to_user(dst, src, chunk)) {
            return -1;
        }
        dst += chunk;
        src += chunk;
        remaining -= chunk;
    }
    return (int32_t)to_copy;
}
