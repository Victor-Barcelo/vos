// sysview - VOS System Introspection Tool
// Real-time view of kernel internals for educational purposes

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "syscall.h"

// ANSI color codes (bright colors for visibility on blue background)
#define CLR_RESET   "\x1b[0m"
#define CLR_TITLE   "\x1b[36;1m"    // Bright cyan
#define CLR_LABEL   "\x1b[33;1m"    // Bright yellow
#define CLR_VALUE   "\x1b[37;1m"    // Bright white
#define CLR_DIM     "\x1b[37m"      // Normal white
#define CLR_GOOD    "\x1b[32;1m"    // Bright green
#define CLR_WARN    "\x1b[33;1m"    // Bright yellow
#define CLR_BAD     "\x1b[31;1m"    // Bright red
#define CLR_HEADER  "\x1b[35;1m"    // Bright magenta
#define CLR_BOX     "\x1b[34;1m"    // Bright blue

// View modes
typedef enum {
    VIEW_OVERVIEW = 0,
    VIEW_MEMORY,
    VIEW_PROCESSES,
    VIEW_INTERRUPTS,
    VIEW_SYSCALLS,
    VIEW_HELP,
    VIEW_COUNT
} view_mode_t;

static view_mode_t current_view = VIEW_OVERVIEW;

// Terminal control
static void clear_screen(void) {
    fputs("\x1b[2J\x1b[H", stdout);
}

static void hide_cursor(void) {
    fputs("\x1b[?25l", stdout);
}

static void show_cursor(void) {
    fputs("\x1b[?25h", stdout);
}

// Format helpers
static void format_size_kb(uint32_t kb, char* buf, size_t len) {
    if (kb >= 1024 * 1024) {
        snprintf(buf, len, "%lu GB", (unsigned long)(kb / (1024 * 1024)));
    } else if (kb >= 1024) {
        snprintf(buf, len, "%lu MB", (unsigned long)(kb / 1024));
    } else {
        snprintf(buf, len, "%lu KB", (unsigned long)kb);
    }
}

static void format_uptime(uint32_t ms, char* buf, size_t len) {
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    secs %= 60;
    mins %= 60;
    if (hours > 0) {
        snprintf(buf, len, "%luh %lum %lus", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    } else if (mins > 0) {
        snprintf(buf, len, "%lum %lus", (unsigned long)mins, (unsigned long)secs);
    } else {
        snprintf(buf, len, "%lus", (unsigned long)secs);
    }
}

static const char* state_str(uint32_t state) {
    switch (state) {
        case 0: return "RUN  ";
        case 1: return "SLEEP";
        case 2: return "WAIT ";
        case 3: return "ZOMB ";
        default: return "?    ";
    }
}

static void print_bar(uint32_t used, uint32_t total, int width) {
    if (total == 0) total = 1;
    int filled = (int)((used * (uint32_t)width) / total);
    if (filled > width) filled = width;

    uint32_t pct = (used * 100) / total;
    const char* color = CLR_GOOD;
    if (pct >= 80) color = CLR_BAD;
    else if (pct >= 50) color = CLR_WARN;

    fputs(color, stdout);
    putchar('[');
    for (int i = 0; i < width; i++) {
        putchar(i < filled ? '#' : '-');
    }
    printf("] %3lu%%", (unsigned long)pct);
    fputs(CLR_RESET, stdout);
}

static void print_header(void) {
    printf("%s", CLR_TITLE);
    puts("================================================================================");
    printf("          VOS System Viewer v1.1  |  ");
    printf("%s'q'%s quit  %s'Tab'%s cycle views  %s'1-6'%s jump\n",
           CLR_VALUE, CLR_TITLE, CLR_VALUE, CLR_TITLE, CLR_VALUE, CLR_TITLE);
    puts("================================================================================");
    printf("%s", CLR_RESET);

    // Show current view indicator
    printf("  View: ");
    const char* views[] = {"[1]Overview", "[2]Memory", "[3]Procs", "[4]IRQ", "[5]Syscalls", "[6]Help"};
    for (int i = 0; i < VIEW_COUNT; i++) {
        if (i == (int)current_view) {
            printf("%s%s%s ", CLR_HEADER, views[i], CLR_RESET);
        } else {
            printf("%s%s%s ", CLR_DIM, views[i], CLR_RESET);
        }
    }
    puts("\n");
}

static void render_overview(void) {
    vos_pmm_info_t pmm;
    vos_heap_info_t heap;
    vos_timer_info_t timer;
    vos_sched_stats_t sched;
    vos_irq_stats_t irq;
    vos_descriptor_info_t desc;
    char buf[64];

    sys_pmm_info(&pmm);
    sys_heap_info(&heap);
    sys_timer_info(&timer);
    sys_sched_stats(&sched);
    sys_irq_stats(&irq);
    sys_descriptor_info(&desc);

    // Left column: Memory
    printf("%s[MEMORY]%s\n", CLR_HEADER, CLR_RESET);
    printf("  %sPhysical RAM:%s\n", CLR_LABEL, CLR_RESET);

    uint32_t total_kb = pmm.total_frames * 4;
    uint32_t free_kb = pmm.free_frames * 4;
    uint32_t used_kb = total_kb - free_kb;

    format_size_kb(total_kb, buf, sizeof(buf));
    printf("    Total:  %s%6lu%s frames (%s)\n", CLR_VALUE, (unsigned long)pmm.total_frames, CLR_RESET, buf);
    format_size_kb(free_kb, buf, sizeof(buf));
    printf("    Free:   %s%6lu%s frames (%s)\n", CLR_VALUE, (unsigned long)pmm.free_frames, CLR_RESET, buf);
    format_size_kb(used_kb, buf, sizeof(buf));
    printf("    Used:   %s%6lu%s frames (%s)\n", CLR_VALUE, (unsigned long)(pmm.total_frames - pmm.free_frames), CLR_RESET, buf);
    printf("    ");
    print_bar(pmm.total_frames - pmm.free_frames, pmm.total_frames, 20);
    puts("");

    printf("\n  %sKernel Heap:%s 0x%08lX\n", CLR_LABEL, CLR_RESET, (unsigned long)heap.heap_base);
    uint32_t heap_size = heap.heap_end - heap.heap_base;
    uint32_t heap_used = heap_size - heap.total_free_bytes;
    format_size_kb(heap_size / 1024, buf, sizeof(buf));
    printf("    Size: %s%s%s  |  ", CLR_VALUE, buf, CLR_RESET);
    format_size_kb(heap.total_free_bytes / 1024, buf, sizeof(buf));
    printf("Free: %s%s%s  |  Blocks: %s%lu%s\n",
           CLR_VALUE, buf, CLR_RESET,
           CLR_VALUE, (unsigned long)heap.free_block_count, CLR_RESET);
    printf("    ");
    print_bar(heap_used, heap_size, 20);
    puts("\n");

    // Right column: CPU/Timer
    printf("%s[CPU/TIMER]%s\n", CLR_HEADER, CLR_RESET);
    char cpu_brand[64] = {0};
    sys_cpu_brand(cpu_brand, sizeof(cpu_brand));
    printf("  CPU: %s%.40s%s\n", CLR_VALUE, cpu_brand[0] ? cpu_brand : "Unknown", CLR_RESET);
    printf("  Ticks: %s%lu%s  (%s%lu%s Hz)\n",
           CLR_VALUE, (unsigned long)timer.ticks, CLR_RESET,
           CLR_VALUE, (unsigned long)timer.hz, CLR_RESET);
    format_uptime(timer.uptime_ms, buf, sizeof(buf));
    printf("  Uptime: %s%s%s\n", CLR_VALUE, buf, CLR_RESET);
    printf("  Context Switches: %s%lu%s\n\n", CLR_VALUE, (unsigned long)sched.context_switches, CLR_RESET);

    // Interrupts summary
    printf("%s[INTERRUPTS]%s\n", CLR_HEADER, CLR_RESET);
    printf("  IRQ0  %-10s %s%10lu%s\n", "Timer:", CLR_VALUE, (unsigned long)irq.counts[0], CLR_RESET);
    printf("  IRQ1  %-10s %s%10lu%s\n", "Keyboard:", CLR_VALUE, (unsigned long)irq.counts[1], CLR_RESET);
    printf("  IRQ12 %-10s %s%10lu%s\n\n", "Mouse:", CLR_VALUE, (unsigned long)irq.counts[12], CLR_RESET);

    // Processes summary
    printf("%s[PROCESSES]%s ", CLR_HEADER, CLR_RESET);
    printf("Total: %s%lu%s | Run: %s%lu%s | Sleep: %s%lu%s | Wait: %s%lu%s | Zombie: %s%lu%s\n",
           CLR_VALUE, (unsigned long)sched.task_count, CLR_RESET,
           CLR_GOOD, (unsigned long)sched.runnable, CLR_RESET,
           CLR_LABEL, (unsigned long)sched.sleeping, CLR_RESET,
           CLR_WARN, (unsigned long)sched.waiting, CLR_RESET,
           CLR_BAD, (unsigned long)sched.zombie, CLR_RESET);

    int cur_pid = getpid();
    int count = sys_task_count();
    printf("  %sPID   STATE  TICKS      EIP        NAME%s\n", CLR_DIM, CLR_RESET);
    for (int i = 0; i < count && i < 8; i++) {
        vos_task_info_t ti;
        if (sys_task_info((uint32_t)i, &ti) < 0) continue;
        char mark = (ti.pid == (uint32_t)cur_pid) ? '*' : ' ';
        printf("  %c%-4lu %s%-5s%s %s%-10lu%s 0x%08lx %s%s%s\n",
               mark,
               (unsigned long)ti.pid,
               ti.state == 0 ? CLR_GOOD : CLR_DIM,
               state_str(ti.state),
               CLR_RESET,
               CLR_VALUE, (unsigned long)ti.cpu_ticks, CLR_RESET,
               (unsigned long)ti.eip,
               CLR_VALUE, ti.name, CLR_RESET);
    }
    if (count > 8) {
        printf("  %s... and %d more%s\n", CLR_DIM, count - 8, CLR_RESET);
    }

    // Descriptors
    printf("\n%s[DESCRIPTORS]%s\n", CLR_HEADER, CLR_RESET);
    printf("  GDT: %s%lu%s entries @ 0x%08lX\n",
           CLR_VALUE, (unsigned long)desc.gdt_entries, CLR_RESET, (unsigned long)desc.gdt_base);
    printf("  IDT: %s%lu%s entries @ 0x%08lX\n",
           CLR_VALUE, (unsigned long)desc.idt_entries, CLR_RESET, (unsigned long)desc.idt_base);
    printf("  TSS ESP0: %s0x%08lX%s\n", CLR_VALUE, (unsigned long)desc.tss_esp0, CLR_RESET);
}

static void render_memory_view(void) {
    vos_pmm_info_t pmm;
    vos_heap_info_t heap;
    char buf[64];

    sys_pmm_info(&pmm);
    sys_heap_info(&heap);

    printf("%s=== PHYSICAL MEMORY ===%s\n\n", CLR_HEADER, CLR_RESET);

    uint32_t total_kb = pmm.total_frames * 4;
    uint32_t free_kb = pmm.free_frames * 4;
    uint32_t used_frames = pmm.total_frames - pmm.free_frames;

    printf("  %sPage Size:%s      %lu bytes\n", CLR_LABEL, CLR_RESET, (unsigned long)pmm.page_size);
    printf("  %sTotal Frames:%s   %lu\n", CLR_LABEL, CLR_RESET, (unsigned long)pmm.total_frames);
    printf("  %sFree Frames:%s    %lu\n", CLR_LABEL, CLR_RESET, (unsigned long)pmm.free_frames);
    printf("  %sUsed Frames:%s    %lu\n\n", CLR_LABEL, CLR_RESET, (unsigned long)used_frames);

    format_size_kb(total_kb, buf, sizeof(buf));
    printf("  %sTotal Memory:%s   %s\n", CLR_LABEL, CLR_RESET, buf);
    format_size_kb(free_kb, buf, sizeof(buf));
    printf("  %sFree Memory:%s    %s\n", CLR_LABEL, CLR_RESET, buf);
    format_size_kb(total_kb - free_kb, buf, sizeof(buf));
    printf("  %sUsed Memory:%s    %s\n\n", CLR_LABEL, CLR_RESET, buf);

    printf("  Usage: ");
    print_bar(used_frames, pmm.total_frames, 40);
    puts("\n");

    printf("%s=== KERNEL HEAP ===%s\n\n", CLR_HEADER, CLR_RESET);

    uint32_t heap_size = heap.heap_end - heap.heap_base;
    uint32_t heap_used = heap_size - heap.total_free_bytes;

    printf("  %sBase Address:%s   0x%08lX\n", CLR_LABEL, CLR_RESET, (unsigned long)heap.heap_base);
    printf("  %sEnd Address:%s    0x%08lX\n", CLR_LABEL, CLR_RESET, (unsigned long)heap.heap_end);
    printf("  %sHeap Size:%s      %lu bytes\n", CLR_LABEL, CLR_RESET, (unsigned long)heap_size);
    printf("  %sFree Bytes:%s     %lu\n", CLR_LABEL, CLR_RESET, (unsigned long)heap.total_free_bytes);
    printf("  %sFree Blocks:%s    %lu\n\n", CLR_LABEL, CLR_RESET, (unsigned long)heap.free_block_count);

    printf("  Usage: ");
    print_bar(heap_used, heap_size, 40);
    puts("");
}

static void render_process_view(void) {
    vos_sched_stats_t sched;
    sys_sched_stats(&sched);

    printf("%s=== PROCESS LIST ===%s\n\n", CLR_HEADER, CLR_RESET);

    printf("  Total: %s%lu%s | Runnable: %s%lu%s | Sleeping: %s%lu%s | Waiting: %s%lu%s | Zombie: %s%lu%s\n\n",
           CLR_VALUE, (unsigned long)sched.task_count, CLR_RESET,
           CLR_GOOD, (unsigned long)sched.runnable, CLR_RESET,
           CLR_LABEL, (unsigned long)sched.sleeping, CLR_RESET,
           CLR_WARN, (unsigned long)sched.waiting, CLR_RESET,
           CLR_BAD, (unsigned long)sched.zombie, CLR_RESET);

    int cur_pid = getpid();
    int count = sys_task_count();

    printf("  %s  PID   USER  STATE  CPU TICKS   EIP        ESP        NAME%s\n", CLR_DIM, CLR_RESET);
    printf("  %s  ----  ----  -----  ---------   --------   --------   ----------------%s\n", CLR_DIM, CLR_RESET);

    for (int i = 0; i < count; i++) {
        vos_task_info_t ti;
        if (sys_task_info((uint32_t)i, &ti) < 0) continue;

        char mark = (ti.pid == (uint32_t)cur_pid) ? '*' : ' ';
        const char* user = ti.user ? "user" : "kern";
        const char* st_color = (ti.state == 0) ? CLR_GOOD :
                               (ti.state == 3) ? CLR_BAD : CLR_DIM;

        printf("  %c %-4lu  %-4s  %s%-5s%s  %-10lu  0x%08lx 0x%08lx %s%s%s\n",
               mark,
               (unsigned long)ti.pid,
               user,
               st_color, state_str(ti.state), CLR_RESET,
               (unsigned long)ti.cpu_ticks,
               (unsigned long)ti.eip,
               (unsigned long)ti.esp,
               CLR_VALUE, ti.name, CLR_RESET);
    }
}

static void render_interrupt_view(void) {
    vos_irq_stats_t irq;
    vos_timer_info_t timer;
    sys_irq_stats(&irq);
    sys_timer_info(&timer);

    printf("%s=== INTERRUPT STATISTICS ===%s\n\n", CLR_HEADER, CLR_RESET);

    const char* irq_names[16] = {
        "Timer (PIT)",      // 0
        "Keyboard",         // 1
        "Cascade",          // 2
        "COM2/COM4",        // 3
        "COM1/COM3",        // 4
        "LPT2",             // 5
        "Floppy",           // 6
        "LPT1/Spurious",    // 7
        "RTC",              // 8
        "ACPI",             // 9
        "Available",        // 10
        "Available",        // 11
        "PS/2 Mouse",       // 12
        "FPU/Coproc",       // 13
        "Primary ATA",      // 14
        "Secondary ATA"     // 15
    };

    printf("  %sIRQ#  Description          Count%s\n", CLR_DIM, CLR_RESET);
    printf("  %s----  -----------------    ----------%s\n", CLR_DIM, CLR_RESET);

    for (int i = 0; i < 16; i++) {
        const char* color = (irq.counts[i] > 0) ? CLR_VALUE : CLR_DIM;
        printf("  %s%-4d  %-18s   %10lu%s\n",
               color, i, irq_names[i], (unsigned long)irq.counts[i], CLR_RESET);
    }

    printf("\n  %sTimer Frequency:%s %lu Hz\n", CLR_LABEL, CLR_RESET, (unsigned long)timer.hz);
    printf("  %sTotal Ticks:%s     %lu\n", CLR_LABEL, CLR_RESET, (unsigned long)timer.ticks);
}

// Previous syscall counts for delta calculation
static vos_syscall_stats_t prev_syscall_stats = {0};
static int have_prev_stats = 0;

static void render_syscall_view(void) {
    vos_syscall_stats_t stats;
    sys_syscall_stats(&stats);

    printf("%s=== SYSCALL ACTIVITY ===%s\n\n", CLR_HEADER, CLR_RESET);
    printf("  Watch syscalls being called in real-time!\n");
    printf("  Compile & run a C program with TCC to see syscalls.\n\n");

    // Calculate total calls
    uint32_t total = 0;
    for (uint32_t i = 0; i < stats.num_syscalls; i++) {
        total += stats.counts[i];
    }
    printf("  %sTotal Syscalls:%s %lu\n\n", CLR_LABEL, CLR_RESET, (unsigned long)total);

    // Show active syscalls (those with non-zero counts), sorted by activity
    printf("  %s#    Name            Count       Delta%s\n", CLR_DIM, CLR_RESET);
    printf("  %s---  --------------  ----------  ------%s\n", CLR_DIM, CLR_RESET);

    // Find and display syscalls with activity (most recent first by showing deltas)
    int displayed = 0;
    for (uint32_t i = 0; i < stats.num_syscalls && displayed < 20; i++) {
        if (stats.counts[i] > 0 && stats.names[i][0] != '\0') {
            uint32_t delta = 0;
            if (have_prev_stats && i < prev_syscall_stats.num_syscalls) {
                delta = stats.counts[i] - prev_syscall_stats.counts[i];
            }

            const char* color = CLR_VALUE;
            const char* delta_color = CLR_DIM;
            if (delta > 0) {
                color = CLR_GOOD;       // Green for recently active
                delta_color = CLR_GOOD;
            }

            printf("  %s%-3lu  %-14s  %10lu%s  ",
                   color, (unsigned long)i, stats.names[i],
                   (unsigned long)stats.counts[i], CLR_RESET);

            if (delta > 0) {
                printf("%s+%lu%s", delta_color, (unsigned long)delta, CLR_RESET);
            }
            puts("");
            displayed++;
        }
    }

    if (displayed == 0) {
        printf("  %sNo syscalls recorded yet.%s\n", CLR_DIM, CLR_RESET);
    }

    // Save for next delta calculation
    prev_syscall_stats = stats;
    have_prev_stats = 1;

    printf("\n  %sTip:%s Run 'tcc -run /bin/hello.c' to see syscalls!\n", CLR_LABEL, CLR_RESET);
}

static void render_help_view(void) {
    printf("%s=== VOS SYSTEM INTERNALS EXPLAINED ===%s\n\n", CLR_HEADER, CLR_RESET);

    printf("%s[PHYSICAL MEMORY]%s\n", CLR_LABEL, CLR_RESET);
    puts("  Memory is divided into 4KB pages called \"frames\".");
    puts("  Total frames = Total RAM / 4096");
    puts("  The PMM (Physical Memory Manager) tracks which frames are free");
    puts("  using a bitmap - each bit represents one 4KB frame.\n");

    printf("%s[KERNEL HEAP]%s\n", CLR_LABEL, CLR_RESET);
    puts("  Virtual address 0xD0000000 is where kmalloc() gets memory.");
    puts("  The heap grows on demand when more kernel memory is needed.");
    puts("  Free blocks are kept in a linked list for reuse (coalescing).\n");

    printf("%s[TIMER]%s\n", CLR_LABEL, CLR_RESET);
    puts("  The PIT (Programmable Interval Timer) fires IRQ0 at 100 Hz.");
    puts("  Each tick = 10ms; used for scheduling and sleep().");
    puts("  Timer ticks drive the preemptive multitasking scheduler.\n");

    printf("%s[CONTEXT SWITCH]%s\n", CLR_LABEL, CLR_RESET);
    puts("  When the CPU switches from one process to another.");
    puts("  Happens on timer tick (preemptive) or voluntarily (yield/sleep).");
    puts("  The kernel saves all registers and restores the next task's.\n");

    printf("%s[IRQs - Hardware Interrupt Requests]%s\n", CLR_LABEL, CLR_RESET);
    puts("  IRQ0  = Timer (PIT)        IRQ1  = Keyboard");
    puts("  IRQ2  = Cascade (to slave) IRQ12 = PS/2 Mouse");
    puts("  IRQ14 = Primary ATA        IRQ15 = Secondary ATA\n");

    printf("%s[DESCRIPTORS]%s\n", CLR_LABEL, CLR_RESET);
    puts("  GDT = Global Descriptor Table (memory segments for ring 0/3)");
    puts("  IDT = Interrupt Descriptor Table (256 interrupt handlers)");
    puts("  TSS = Task State Segment (kernel stack for syscalls/interrupts)\n");

    printf("%s[SYSCALLS]%s\n", CLR_LABEL, CLR_RESET);
    puts("  User programs can't directly access hardware or kernel memory.");
    puts("  They use 'int 0x80' to request services from the kernel.");
    puts("  Examples: write() to print, read() to get input, fork() to spawn.");
    puts("  View [5] shows syscalls in real-time - compile a program to see!");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Setup raw terminal mode
    struct termios orig;
    int have_termios = (tcgetattr(0, &orig) == 0);
    if (have_termios) {
        struct termios raw = orig;
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(0, TCSAFLUSH, &raw);
    }

    hide_cursor();

    // Main loop
    for (;;) {
        // Check for input
        char c = 0;
        if (read(0, &c, 1) == 1) {
            if (c == 'q' || c == 'Q' || c == 3) {  // q, Q, or Ctrl+C
                break;
            }
            if (c == '\t') {
                current_view = (view_mode_t)((current_view + 1) % VIEW_COUNT);
            }
            if (c >= '1' && c <= '6') {
                current_view = (view_mode_t)(c - '1');
            }
        }

        // Render
        clear_screen();
        print_header();

        switch (current_view) {
            case VIEW_OVERVIEW:   render_overview(); break;
            case VIEW_MEMORY:     render_memory_view(); break;
            case VIEW_PROCESSES:  render_process_view(); break;
            case VIEW_INTERRUPTS: render_interrupt_view(); break;
            case VIEW_SYSCALLS:   render_syscall_view(); break;
            case VIEW_HELP:       render_help_view(); break;
            default: break;
        }

        fflush(stdout);

        // Sleep 500ms between updates
        sys_sleep(500);
    }

    show_cursor();
    clear_screen();

    if (have_termios) {
        tcsetattr(0, TCSAFLUSH, &orig);
    }

    return 0;
}
