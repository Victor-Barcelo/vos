// sysview - VOS System Monitor
// Beautiful real-time system introspection using Termbox2

#define TB_IMPL
#include <termbox2.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "syscall.h"

// Colors
#define C_TITLE    TB_CYAN | TB_BOLD
#define C_LABEL    TB_YELLOW | TB_BOLD
#define C_VALUE    TB_WHITE | TB_BOLD
#define C_DIM      TB_WHITE
#define C_GOOD     TB_GREEN | TB_BOLD
#define C_WARN     TB_YELLOW | TB_BOLD
#define C_BAD      TB_RED | TB_BOLD
#define C_BOX      TB_BLUE | TB_BOLD
#define C_HEADER   TB_MAGENTA | TB_BOLD
#define C_BG       TB_DEFAULT

// Box drawing characters (ASCII fallback)
#define BOX_H      '-'
#define BOX_V      '|'
#define BOX_TL     '+'
#define BOX_TR     '+'
#define BOX_BL     '+'
#define BOX_BR     '+'
#define BOX_T      '+'
#define BOX_B      '+'
#define BOX_L      '+'
#define BOX_R      '+'
#define BOX_X      '+'
#define BLOCK_FULL '#'
#define BLOCK_EMPTY '-'

// Screen dimensions
static int width, height;

// View modes
typedef enum {
    VIEW_DASHBOARD = 0,
    VIEW_MEMORY,
    VIEW_PROCESSES,
    VIEW_INTERRUPTS,
    VIEW_SYSCALLS,
    VIEW_HELP,
    VIEW_COUNT
} view_t;

static view_t current_view = VIEW_DASHBOARD;
static const char* view_names[] = {
    "Dashboard", "Memory", "Processes", "Interrupts", "Syscalls", "Help"
};

// Previous syscall stats for delta
static vos_syscall_stats_t prev_stats = {0};
static int have_prev_stats = 0;

// Helper to draw a string at position
static void draw_str(int x, int y, uintattr_t fg, const char* str) {
    tb_print(x, y, fg, C_BG, str);
}

// Helper to draw a formatted string
static void draw_fmt(int x, int y, uintattr_t fg, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tb_print(x, y, fg, C_BG, buf);
}

// Draw horizontal line
static void draw_hline(int x, int y, int len, uintattr_t fg) {
    for (int i = 0; i < len; i++) {
        tb_set_cell(x + i, y, BOX_H, fg, C_BG);
    }
}

// Draw vertical line
static void draw_vline(int x, int y, int len, uintattr_t fg) {
    for (int i = 0; i < len; i++) {
        tb_set_cell(x, y + i, BOX_V, fg, C_BG);
    }
}

// Draw a box
static void draw_box(int x, int y, int w, int h, uintattr_t fg, const char* title) {
    // Corners
    tb_set_cell(x, y, BOX_TL, fg, C_BG);
    tb_set_cell(x + w - 1, y, BOX_TR, fg, C_BG);
    tb_set_cell(x, y + h - 1, BOX_BL, fg, C_BG);
    tb_set_cell(x + w - 1, y + h - 1, BOX_BR, fg, C_BG);

    // Edges
    draw_hline(x + 1, y, w - 2, fg);
    draw_hline(x + 1, y + h - 1, w - 2, fg);
    draw_vline(x, y + 1, h - 2, fg);
    draw_vline(x + w - 1, y + 1, h - 2, fg);

    // Title
    if (title && title[0]) {
        int tx = x + 2;
        tb_set_cell(tx - 1, y, '[', fg, C_BG);
        tb_print(tx, y, C_TITLE, C_BG, title);
        tb_set_cell(tx + (int)strlen(title), y, ']', fg, C_BG);
    }
}

// Draw progress bar
static void draw_bar(int x, int y, int w, uint32_t used, uint32_t total) {
    if (total == 0) total = 1;
    int filled = (int)((used * (uint32_t)w) / total);
    if (filled > w) filled = w;

    uint32_t pct = (used * 100) / total;
    uintattr_t color = C_GOOD;
    if (pct >= 80) color = C_BAD;
    else if (pct >= 50) color = C_WARN;

    tb_set_cell(x, y, '[', color, C_BG);
    for (int i = 0; i < w; i++) {
        tb_set_cell(x + 1 + i, y, i < filled ? BLOCK_FULL : BLOCK_EMPTY, color, C_BG);
    }
    tb_set_cell(x + w + 1, y, ']', color, C_BG);

    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%3lu%%", (unsigned long)pct);
    tb_print(x + w + 3, y, color, C_BG, pct_str);
}

// Format size in KB to human readable
static void format_size(uint32_t kb, char* buf, int len) {
    if (kb >= 1024 * 1024) {
        snprintf(buf, len, "%lu GB", (unsigned long)(kb / (1024 * 1024)));
    } else if (kb >= 1024) {
        snprintf(buf, len, "%lu MB", (unsigned long)(kb / 1024));
    } else {
        snprintf(buf, len, "%lu KB", (unsigned long)kb);
    }
}

// Format uptime
static void format_uptime(uint32_t ms, char* buf, int len) {
    uint32_t s = ms / 1000;
    uint32_t m = s / 60;
    uint32_t h = m / 60;
    s %= 60; m %= 60;
    if (h > 0) snprintf(buf, len, "%luh %lum %lus", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else if (m > 0) snprintf(buf, len, "%lum %lus", (unsigned long)m, (unsigned long)s);
    else snprintf(buf, len, "%lus", (unsigned long)s);
}

// Draw header bar
static void draw_header(void) {
    // Title bar
    for (int i = 0; i < width; i++) {
        tb_set_cell(i, 0, ' ', C_BG, TB_CYAN);
    }

    const char* title = " VOS System Monitor v2.0 ";
    int tx = (width - (int)strlen(title)) / 2;
    tb_print(tx, 0, TB_BLACK | TB_BOLD, TB_CYAN, title);

    // View tabs
    int tab_x = 1;
    for (int i = 0; i < VIEW_COUNT; i++) {
        char tab[20];
        snprintf(tab, sizeof(tab), " %d:%s ", i + 1, view_names[i]);

        if (i == (int)current_view) {
            tb_print(tab_x, 1, TB_BLACK | TB_BOLD, TB_WHITE, tab);
        } else {
            tb_print(tab_x, 1, C_DIM, C_BG, tab);
        }
        tab_x += strlen(tab) + 1;
    }

    // Help hint
    draw_str(width - 20, 1, C_DIM, "q:Quit  Tab:Next");
}

// Draw dashboard view
static void draw_dashboard(void) {
    vos_pmm_info_t pmm;
    vos_heap_info_t heap;
    vos_timer_info_t timer;
    vos_sched_stats_t sched;
    vos_irq_stats_t irq;
    char buf[64], buf2[64];

    sys_pmm_info(&pmm);
    sys_heap_info(&heap);
    sys_timer_info(&timer);
    sys_sched_stats(&sched);
    sys_irq_stats(&irq);

    int col1 = 1, col2 = width / 2;
    int row = 3;

    // === MEMORY BOX ===
    draw_box(col1, row, width/2 - 2, 10, C_BOX, "MEMORY");
    row++;

    uint32_t total_kb = pmm.total_frames * 4;
    uint32_t used_kb = (pmm.total_frames - pmm.free_frames) * 4;

    format_size(total_kb, buf, sizeof(buf));
    draw_fmt(col1 + 2, row + 1, C_LABEL, "Physical:");
    draw_fmt(col1 + 14, row + 1, C_VALUE, "%s total", buf);

    format_size(used_kb, buf, sizeof(buf));
    format_size(pmm.free_frames * 4, buf2, sizeof(buf2));
    draw_fmt(col1 + 2, row + 2, C_DIM, "  Used: %s  Free: %s", buf, buf2);
    draw_bar(col1 + 2, row + 3, 20, pmm.total_frames - pmm.free_frames, pmm.total_frames);

    uint32_t heap_size = heap.heap_end - heap.heap_base;
    uint32_t heap_used = heap_size - heap.total_free_bytes;
    format_size(heap_size / 1024, buf, sizeof(buf));
    draw_fmt(col1 + 2, row + 5, C_LABEL, "Kernel Heap:");
    draw_fmt(col1 + 16, row + 5, C_VALUE, "%s", buf);
    draw_bar(col1 + 2, row + 6, 20, heap_used, heap_size);

    // === CPU BOX ===
    row = 3;
    draw_box(col2, row, width/2 - 1, 10, C_BOX, "CPU / TIMER");

    char cpu[64] = {0};
    sys_cpu_brand(cpu, sizeof(cpu));
    if (cpu[0]) {
        // Truncate long CPU names
        if (strlen(cpu) > 30) cpu[30] = '\0';
        draw_fmt(col2 + 2, row + 1, C_VALUE, "%s", cpu);
    }

    format_uptime(timer.uptime_ms, buf, sizeof(buf));
    draw_fmt(col2 + 2, row + 3, C_LABEL, "Uptime:");
    draw_fmt(col2 + 11, row + 3, C_VALUE, "%s", buf);

    draw_fmt(col2 + 2, row + 4, C_LABEL, "Ticks:");
    draw_fmt(col2 + 11, row + 4, C_VALUE, "%lu @ %luHz", (unsigned long)timer.ticks, (unsigned long)timer.hz);

    draw_fmt(col2 + 2, row + 6, C_LABEL, "Ctx Sw:");
    draw_fmt(col2 + 11, row + 6, C_VALUE, "%lu", (unsigned long)sched.context_switches);

    // === PROCESSES BOX ===
    row = 14;
    int proc_h = height - row - 1;
    if (proc_h < 8) proc_h = 8;
    draw_box(col1, row, width - 2, proc_h, C_BOX, "PROCESSES");

    draw_fmt(col1 + 2, row + 1, C_DIM, "Total: ");
    draw_fmt(col1 + 9, row + 1, C_VALUE, "%lu", (unsigned long)sched.task_count);
    draw_fmt(col1 + 15, row + 1, C_GOOD, "Run:%lu", (unsigned long)sched.runnable);
    draw_fmt(col1 + 25, row + 1, C_WARN, "Sleep:%lu", (unsigned long)sched.sleeping);
    draw_fmt(col1 + 37, row + 1, C_DIM, "Wait:%lu", (unsigned long)sched.waiting);
    draw_fmt(col1 + 48, row + 1, C_BAD, "Zomb:%lu", (unsigned long)sched.zombie);

    draw_str(col1 + 2, row + 3, C_DIM, "PID   STATE   TICKS       NAME");
    draw_hline(col1 + 2, row + 4, width - 6, C_DIM);

    int cur_pid = getpid();
    int count = sys_task_count();
    int max_procs = proc_h - 6;

    for (int i = 0; i < count && i < max_procs; i++) {
        vos_task_info_t ti;
        if (sys_task_info(i, &ti) < 0) continue;

        const char* st;
        uintattr_t sc;
        switch (ti.state) {
            case 0: st = "RUN  "; sc = C_GOOD; break;
            case 1: st = "SLEEP"; sc = C_WARN; break;
            case 2: st = "WAIT "; sc = C_DIM; break;
            case 3: st = "ZOMB "; sc = C_BAD; break;
            default: st = "?    "; sc = C_DIM; break;
        }

        char mark = (ti.pid == (uint32_t)cur_pid) ? '*' : ' ';
        draw_fmt(col1 + 2, row + 5 + i, C_VALUE, "%c%-4lu", mark, (unsigned long)ti.pid);
        draw_str(col1 + 8, row + 5 + i, sc, st);
        draw_fmt(col1 + 14, row + 5 + i, C_DIM, "%-10lu", (unsigned long)ti.cpu_ticks);
        draw_str(col1 + 26, row + 5 + i, C_VALUE, ti.name);
    }
}

// Draw memory view
static void draw_memory(void) {
    vos_pmm_info_t pmm;
    vos_heap_info_t heap;
    char buf[64];

    sys_pmm_info(&pmm);
    sys_heap_info(&heap);

    int row = 3;
    draw_box(1, row, width - 2, 12, C_BOX, "PHYSICAL MEMORY");

    uint32_t total_kb = pmm.total_frames * 4;
    uint32_t free_kb = pmm.free_frames * 4;
    uint32_t used_kb = total_kb - free_kb;

    draw_fmt(3, row + 2, C_LABEL, "Page Size:    ");
    draw_fmt(18, row + 2, C_VALUE, "%lu bytes", (unsigned long)pmm.page_size);

    draw_fmt(3, row + 3, C_LABEL, "Total Frames: ");
    draw_fmt(18, row + 3, C_VALUE, "%lu", (unsigned long)pmm.total_frames);
    format_size(total_kb, buf, sizeof(buf));
    draw_fmt(32, row + 3, C_DIM, "(%s)", buf);

    draw_fmt(3, row + 4, C_LABEL, "Free Frames:  ");
    draw_fmt(18, row + 4, C_GOOD, "%lu", (unsigned long)pmm.free_frames);
    format_size(free_kb, buf, sizeof(buf));
    draw_fmt(32, row + 4, C_DIM, "(%s)", buf);

    draw_fmt(3, row + 5, C_LABEL, "Used Frames:  ");
    draw_fmt(18, row + 5, C_WARN, "%lu", (unsigned long)(pmm.total_frames - pmm.free_frames));
    format_size(used_kb, buf, sizeof(buf));
    draw_fmt(32, row + 5, C_DIM, "(%s)", buf);

    draw_str(3, row + 7, C_LABEL, "Usage:");
    draw_bar(10, row + 7, 40, pmm.total_frames - pmm.free_frames, pmm.total_frames);

    // Visual memory map
    draw_str(3, row + 9, C_DIM, "Memory Map: ");
    int map_w = width - 20;
    if (map_w > 60) map_w = 60;
    uint32_t used_pct = ((pmm.total_frames - pmm.free_frames) * (uint32_t)map_w) / pmm.total_frames;
    for (int i = 0; i < map_w; i++) {
        tb_set_cell(15 + i, row + 9, BLOCK_FULL, i < (int)used_pct ? C_WARN : C_GOOD, C_BG);
    }

    row = 16;
    draw_box(1, row, width - 2, 10, C_BOX, "KERNEL HEAP");

    uint32_t heap_size = heap.heap_end - heap.heap_base;
    uint32_t heap_used = heap_size - heap.total_free_bytes;

    draw_fmt(3, row + 2, C_LABEL, "Base Address: ");
    draw_fmt(18, row + 2, C_VALUE, "0x%08lX", (unsigned long)heap.heap_base);

    draw_fmt(3, row + 3, C_LABEL, "End Address:  ");
    draw_fmt(18, row + 3, C_VALUE, "0x%08lX", (unsigned long)heap.heap_end);

    format_size(heap_size / 1024, buf, sizeof(buf));
    draw_fmt(3, row + 4, C_LABEL, "Total Size:   ");
    draw_fmt(18, row + 4, C_VALUE, "%s", buf);

    draw_fmt(3, row + 5, C_LABEL, "Free Blocks:  ");
    draw_fmt(18, row + 5, C_VALUE, "%lu", (unsigned long)heap.free_block_count);

    draw_str(3, row + 7, C_LABEL, "Usage:");
    draw_bar(10, row + 7, 40, heap_used, heap_size);
}

// Draw processes view
static void draw_processes(void) {
    vos_sched_stats_t sched;
    sys_sched_stats(&sched);

    int row = 3;
    draw_box(1, row, width - 2, height - 4, C_BOX, "PROCESS LIST");

    draw_fmt(3, row + 1, C_LABEL, "Total:");
    draw_fmt(10, row + 1, C_VALUE, "%lu", (unsigned long)sched.task_count);
    draw_fmt(16, row + 1, C_GOOD, "Runnable:%lu", (unsigned long)sched.runnable);
    draw_fmt(30, row + 1, C_WARN, "Sleeping:%lu", (unsigned long)sched.sleeping);
    draw_fmt(44, row + 1, C_DIM, "Waiting:%lu", (unsigned long)sched.waiting);
    draw_fmt(57, row + 1, C_BAD, "Zombie:%lu", (unsigned long)sched.zombie);

    draw_str(3, row + 3, C_DIM, "  PID  TYPE  STATE   CPU TICKS   EIP        ESP        NAME");
    draw_hline(3, row + 4, width - 8, C_DIM);

    int cur_pid = getpid();
    int count = sys_task_count();
    int max = height - row - 6;

    for (int i = 0; i < count && i < max; i++) {
        vos_task_info_t ti;
        if (sys_task_info(i, &ti) < 0) continue;

        const char* st;
        uintattr_t sc;
        switch (ti.state) {
            case 0: st = "RUN  "; sc = C_GOOD; break;
            case 1: st = "SLEEP"; sc = C_WARN; break;
            case 2: st = "WAIT "; sc = C_DIM; break;
            case 3: st = "ZOMB "; sc = C_BAD; break;
            default: st = "?    "; sc = C_DIM; break;
        }

        char mark = (ti.pid == (uint32_t)cur_pid) ? '*' : ' ';
        const char* type = ti.user ? "user" : "kern";

        draw_fmt(3, row + 5 + i, C_VALUE, "%c %-4lu", mark, (unsigned long)ti.pid);
        draw_str(10, row + 5 + i, C_DIM, type);
        draw_str(16, row + 5 + i, sc, st);
        draw_fmt(23, row + 5 + i, C_DIM, "%-10lu", (unsigned long)ti.cpu_ticks);
        draw_fmt(34, row + 5 + i, C_DIM, "0x%08lx", (unsigned long)ti.eip);
        draw_fmt(45, row + 5 + i, C_DIM, "0x%08lx", (unsigned long)ti.esp);
        draw_str(56, row + 5 + i, C_VALUE, ti.name);
    }
}

// Draw interrupts view
static void draw_interrupts(void) {
    vos_irq_stats_t irq;
    vos_timer_info_t timer;
    sys_irq_stats(&irq);
    sys_timer_info(&timer);

    int row = 3;
    draw_box(1, row, width - 2, 22, C_BOX, "HARDWARE INTERRUPTS (IRQs)");

    const char* names[16] = {
        "Timer (PIT)", "Keyboard", "Cascade", "COM2/COM4",
        "COM1/COM3", "LPT2", "Floppy", "LPT1/Spurious",
        "RTC", "ACPI", "Available", "Available",
        "PS/2 Mouse", "FPU/Coproc", "Primary ATA", "Secondary ATA"
    };

    draw_str(3, row + 2, C_DIM, "IRQ#  Description         Count");
    draw_hline(3, row + 3, 40, C_DIM);

    for (int i = 0; i < 16; i++) {
        uintattr_t c = irq.counts[i] > 0 ? C_VALUE : C_DIM;
        draw_fmt(3, row + 4 + i, c, "%-4d  %-18s  %lu", i, names[i], (unsigned long)irq.counts[i]);
    }

    draw_fmt(50, row + 4, C_LABEL, "Timer Frequency:");
    draw_fmt(68, row + 4, C_VALUE, "%lu Hz", (unsigned long)timer.hz);

    draw_fmt(50, row + 6, C_LABEL, "Total Ticks:");
    draw_fmt(68, row + 6, C_VALUE, "%lu", (unsigned long)timer.ticks);
}

// Draw syscalls view
static void draw_syscalls(void) {
    vos_syscall_stats_t stats;
    sys_syscall_stats(&stats);

    int row = 3;
    draw_box(1, row, width - 2, height - 4, C_BOX, "SYSCALL ACTIVITY");

    // Total
    uint32_t total = 0;
    for (uint32_t i = 0; i < stats.num_syscalls; i++) {
        total += stats.counts[i];
    }

    draw_fmt(3, row + 1, C_LABEL, "Total Syscalls:");
    draw_fmt(20, row + 1, C_VALUE, "%lu", (unsigned long)total);

    draw_str(3, row + 2, C_DIM, "Watch syscalls in real-time! Compile a C program to see activity.");

    draw_str(3, row + 4, C_DIM, "#    Name            Count       Delta");
    draw_hline(3, row + 5, 50, C_DIM);

    int displayed = 0;
    int max = height - row - 8;

    for (uint32_t i = 0; i < stats.num_syscalls && displayed < max; i++) {
        if (stats.counts[i] > 0 && stats.names[i][0]) {
            uint32_t delta = 0;
            if (have_prev_stats && i < prev_stats.num_syscalls) {
                delta = stats.counts[i] - prev_stats.counts[i];
            }

            uintattr_t c = delta > 0 ? C_GOOD : C_VALUE;
            draw_fmt(3, row + 6 + displayed, c, "%-3lu  %-14s  %-10lu",
                     (unsigned long)i, stats.names[i], (unsigned long)stats.counts[i]);

            if (delta > 0) {
                draw_fmt(40, row + 6 + displayed, C_GOOD, "+%lu", (unsigned long)delta);
            }
            displayed++;
        }
    }

    prev_stats = stats;
    have_prev_stats = 1;
}

// Draw help view
static void draw_help(void) {
    int row = 3;
    draw_box(1, row, width - 2, height - 4, C_BOX, "VOS INTERNALS EXPLAINED");

    int y = row + 2;

    draw_str(3, y++, C_LABEL, "[PHYSICAL MEMORY]");
    draw_str(3, y++, C_DIM, "Memory divided into 4KB pages (frames). PMM tracks free frames");
    draw_str(3, y++, C_DIM, "using a bitmap. Total RAM = frames x 4096 bytes.");
    y++;

    draw_str(3, y++, C_LABEL, "[KERNEL HEAP]");
    draw_str(3, y++, C_DIM, "Starting at 0xD0000000 - where kmalloc() allocates memory.");
    draw_str(3, y++, C_DIM, "Free blocks kept in linked list with coalescing.");
    y++;

    draw_str(3, y++, C_LABEL, "[TIMER & SCHEDULING]");
    draw_str(3, y++, C_DIM, "PIT fires IRQ0 at 100Hz (10ms ticks). Drives preemptive scheduler.");
    draw_str(3, y++, C_DIM, "Context switch saves/restores all registers between tasks.");
    y++;

    draw_str(3, y++, C_LABEL, "[IRQs - Hardware Interrupts]");
    draw_str(3, y++, C_DIM, "IRQ0=Timer  IRQ1=Keyboard  IRQ12=Mouse  IRQ14/15=ATA");
    y++;

    draw_str(3, y++, C_LABEL, "[SYSCALLS]");
    draw_str(3, y++, C_DIM, "User programs use 'int 0x80' to request kernel services.");
    draw_str(3, y++, C_DIM, "Examples: read(), write(), fork(), execve(), mmap()");
    y++;

    draw_str(3, y++, C_LABEL, "[KEYBOARD SHORTCUTS]");
    draw_str(3, y++, C_VALUE, "1-6: Switch views   Tab: Next view   q: Quit");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    int ret = tb_init();
    if (ret != 0) {
        printf("tb_init() failed: %d\n", ret);
        return 1;
    }

    tb_set_input_mode(TB_INPUT_ESC);
    tb_set_output_mode(TB_OUTPUT_NORMAL);

    width = tb_width();
    height = tb_height();

    struct tb_event ev;
    int running = 1;

    while (running) {
        tb_clear();

        draw_header();

        switch (current_view) {
            case VIEW_DASHBOARD:  draw_dashboard(); break;
            case VIEW_MEMORY:     draw_memory(); break;
            case VIEW_PROCESSES:  draw_processes(); break;
            case VIEW_INTERRUPTS: draw_interrupts(); break;
            case VIEW_SYSCALLS:   draw_syscalls(); break;
            case VIEW_HELP:       draw_help(); break;
            default: break;
        }

        tb_present();

        // Poll with 500ms timeout
        int res = tb_peek_event(&ev, 500);
        if (res == TB_OK) {
            if (ev.type == TB_EVENT_KEY) {
                if (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q') {
                    running = 0;
                } else if (ev.key == TB_KEY_TAB) {
                    current_view = (view_t)((current_view + 1) % VIEW_COUNT);
                } else if (ev.ch >= '1' && ev.ch <= '6') {
                    current_view = (view_t)(ev.ch - '1');
                }
            } else if (ev.type == TB_EVENT_RESIZE) {
                width = ev.w;
                height = ev.h;
            }
        }
    }

    tb_shutdown();
    return 0;
}
