#include "task.h"
#include "kheap.h"
#include "string.h"
#include "gdt.h"

#define KSTACK_SIZE (16u * 1024u)

typedef struct task {
    uint32_t id;
    uint32_t esp;            // saved stack pointer (points to interrupt frame)
    uint32_t kstack_top;     // top of kernel stack (for TSS.esp0)
    bool runnable;
    struct task* next;
} task_t;

extern uint8_t stack_top;

static task_t* current_task = NULL;
static bool enabled = false;
static uint32_t next_id = 1;
static uint32_t tick_div = 0;

static uint32_t* push32(uint32_t* sp, uint32_t v) {
    sp--;
    *sp = v;
    return sp;
}

static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static task_t* task_create_kernel(void (*entry)(void)) {
    uint8_t* stack = (uint8_t*)kmalloc(KSTACK_SIZE);
    if (!stack) {
        return NULL;
    }

    uint32_t stack_top_addr = (uint32_t)(stack + KSTACK_SIZE);
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
    t->runnable = true;
    t->next = NULL;
    return t;
}

static task_t* task_create_user(uint32_t entry, uint32_t user_esp) {
    uint8_t* stack = (uint8_t*)kmalloc(KSTACK_SIZE);
    if (!stack) {
        return NULL;
    }

    uint32_t stack_top_addr = (uint32_t)(stack + KSTACK_SIZE);
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
    t->runnable = true;
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
    boot->runnable = true;
    boot->next = boot;
    current_task = boot;

    task_t* idle = task_create_kernel(idle_thread);
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

    tick_div++;
    if (tick_div < 10u) {
        return frame;
    }
    tick_div = 0;

    // Save current context.
    current_task->esp = (uint32_t)frame;

    // Find next runnable task.
    task_t* next = current_task->next;
    if (!next || next == current_task) {
        return frame;
    }

    for (int i = 0; i < 64; i++) {
        if (next->runnable && next->esp != 0) {
            break;
        }
        next = next->next;
        if (!next || next == current_task) {
            return frame;
        }
    }

    current_task = next;
    tss_set_kernel_stack(current_task->kstack_top);
    return (interrupt_frame_t*)current_task->esp;
}

interrupt_frame_t* tasking_yield(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }

    current_task->esp = (uint32_t)frame;
    task_t* next = current_task->next;
    if (!next || next == current_task) {
        return frame;
    }

    for (int i = 0; i < 64; i++) {
        if (next->runnable && next->esp != 0) {
            break;
        }
        next = next->next;
        if (!next || next == current_task) {
            return frame;
        }
    }

    current_task = next;
    tss_set_kernel_stack(current_task->kstack_top);
    return (interrupt_frame_t*)current_task->esp;
}

interrupt_frame_t* tasking_exit(interrupt_frame_t* frame) {
    if (!enabled || !current_task || !frame) {
        return frame;
    }

    current_task->runnable = false;
    current_task->esp = (uint32_t)frame;
    return tasking_yield(frame);
}

bool tasking_spawn_user(uint32_t entry, uint32_t user_esp) {
    if (!current_task) {
        return false;
    }

    task_t* t = task_create_user(entry, user_esp);
    if (!t) {
        return false;
    }
    task_append(t);
    return true;
}
