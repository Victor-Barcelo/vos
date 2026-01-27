#include "statusbar.h"
#include "screen.h"
#include "timer.h"
#include "rtc.h"
#include "system.h"
#include "pmm.h"
#include "task.h"
#include "minixfs.h"

// Emoji codepoints for statusbar icons
#define EMOJI_STAR      0x2B50   // ⭐ - time
#define EMOJI_FIRE      0x1F525  // 🔥 - memory
#define EMOJI_HEART     0x2764   // ❤️ - disk
#define EMOJI_SUN       0x2600   // ☀️ - CPU
#define EMOJI_CHECK     0x2705   // ✅ - tasks
#define EMOJI_LIGHTNING 0x26A1   // ⚡ - uptime
#define EMOJI_RAINBOW   0x1F308  // 🌈 - console

static uint32_t last_drawn_tick = 0xFFFFFFFFu;
static uint32_t prev_ctx_switches = 0;
static uint32_t cpu_activity = 0;  // 0-10 scale

static uint8_t color_bg(void) {
    return (uint8_t)(VGA_WHITE | (VGA_BLACK << 4));
}

static uint8_t color_accent(void) {
    return (uint8_t)(VGA_LIGHT_CYAN | (VGA_BLACK << 4));
}

static uint8_t color_bar_fill(void) {
    return (uint8_t)(VGA_LIGHT_GREEN | (VGA_BLACK << 4));
}

static uint8_t color_bar_empty(void) {
    return (uint8_t)(VGA_DARK_GREY | (VGA_BLACK << 4));
}

static uint8_t color_sep(void) {
    return (uint8_t)(VGA_DARK_GREY | (VGA_BLACK << 4));
}

static uint8_t color_cpu_high(void) {
    return (uint8_t)(VGA_LIGHT_RED | (VGA_BLACK << 4));
}

static uint8_t color_cpu_med(void) {
    return (uint8_t)(VGA_YELLOW | (VGA_BLACK << 4));
}

static void put_char(int x, int y, char c, uint8_t color) {
    if (x >= 0 && x < screen_cols()) {
        screen_write_char_at_batch(x, y, c, color);
    }
}

static int put_str(int x, int y, const char* s, uint8_t color) {
    int cols = screen_cols();
    while (*s && x < cols) {
        screen_write_char_at_batch(x++, y, *s++, color);
    }
    return x;
}

static int put_num(int x, int y, uint32_t val, uint8_t color) {
    char buf[12];
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
    } else {
        char tmp[11];
        int j = 0;
        while (val > 0) {
            tmp[j++] = (char)('0' + (val % 10));
            val /= 10;
        }
        while (j > 0) buf[i++] = tmp[--j];
    }
    buf[i] = '\0';
    return put_str(x, y, buf, color);
}

static int put_2d(int x, int y, uint32_t val, uint8_t color) {
    put_char(x++, y, (char)('0' + ((val / 10) % 10)), color);
    put_char(x++, y, (char)('0' + (val % 10)), color);
    return x;
}

static void draw_bar(int x, int y, int width, int filled, uint8_t fill_color, uint8_t empty_color) {
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            put_char(x + i, y, '|', fill_color);
        } else {
            put_char(x + i, y, '-', empty_color);
        }
    }
}

static void update_cpu_activity(void) {
    uint32_t ctx = tasking_context_switch_count();
    uint32_t delta = ctx - prev_ctx_switches;
    prev_ctx_switches = ctx;

    // Scale: 0-2 switches = 1 bar, 3-5 = 2 bars, etc.
    // Max out at 10 bars for 20+ switches per update
    if (delta == 0) {
        cpu_activity = 0;
    } else if (delta < 3) {
        cpu_activity = 1;
    } else if (delta < 6) {
        cpu_activity = 2;
    } else if (delta < 10) {
        cpu_activity = 3;
    } else if (delta < 15) {
        cpu_activity = 5;
    } else if (delta < 25) {
        cpu_activity = 7;
    } else {
        cpu_activity = 10;
    }
}

static uint8_t color_disk(void) {
    return (uint8_t)(VGA_LIGHT_BLUE | (VGA_BLACK << 4));
}

static void put_emoji(int x, int y, uint32_t codepoint, uint8_t color) {
    screen_write_emoji_at_batch(x, y, codepoint, color);
}

static void draw_statusbar(void) {
    int cols = screen_cols();
    int row = screen_rows() - 1;
    if (cols < 1 || row < 0) return;

    uint8_t bg = color_bg();
    uint8_t accent = color_accent();
    uint8_t sep = color_sep();
    uint8_t fill = color_bar_fill();
    uint8_t empty = color_bar_empty();
    uint8_t disk_color = color_disk();

    int x = 0;

    // Left margin with star emoji for time
    put_char(x++, row, ' ', bg);
    put_emoji(x, row, EMOJI_STAR, bg);
    x += 2;  // Emoji is 2 cells wide

    // Time: HH:MM
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        x = put_2d(x, row, dt.hour, accent);
        put_char(x++, row, ':', bg);
        x = put_2d(x, row, dt.minute, accent);
    }

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // Uptime with lightning emoji
    put_emoji(x, row, EMOJI_LIGHTNING, bg);
    x += 2;
    uint32_t up_sec = timer_uptime_ms() / 1000u;
    uint32_t up_min = up_sec / 60u;
    uint32_t up_hr = up_min / 60u;
    if (up_hr > 0) {
        x = put_num(x, row, up_hr, bg);
        put_char(x++, row, 'h', bg);
    }
    x = put_num(x, row, up_min % 60u, bg);
    put_char(x++, row, 'm', bg);

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // Memory bar with fire emoji
    put_emoji(x, row, EMOJI_FIRE, bg);
    x += 2;
    uint32_t total_frames = pmm_total_frames();
    uint32_t free_frames = pmm_free_frames();
    uint32_t used_frames = total_frames - free_frames;
    uint32_t mem_total_mb = (total_frames * 4) / 1024;
    uint32_t mem_used_mb = (used_frames * 4) / 1024;
    uint32_t mem_pct = (total_frames > 0) ? (used_frames * 100 / total_frames) : 0;

    x = put_str(x, row, "MEM", accent);
    put_char(x++, row, '[', sep);

    int bar_w = 8;
    int mem_filled = (int)((mem_pct * (uint32_t)bar_w) / 100u);
    if (mem_filled > bar_w) mem_filled = bar_w;
    draw_bar(x, row, bar_w, mem_filled, fill, empty);
    x += bar_w;

    put_char(x++, row, ']', sep);
    x = put_num(x, row, mem_used_mb, bg);
    put_char(x++, row, '/', bg);
    x = put_num(x, row, mem_total_mb, bg);
    x = put_str(x, row, "M", bg);

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // Disk usage with heart emoji
    put_emoji(x, row, EMOJI_HEART, bg);
    x += 2;
    uint32_t disk_total = 0, disk_free = 0, disk_inodes = 0, disk_inodes_free = 0;
    if (minixfs_statfs(&disk_total, &disk_free, &disk_inodes, &disk_inodes_free) && disk_total > 0) {
        uint32_t disk_used = disk_total - disk_free;
        // Convert to MB (1024 bytes per block)
        uint32_t disk_total_mb = disk_total / 1024;
        uint32_t disk_used_mb = disk_used / 1024;
        uint32_t disk_pct = (disk_used * 100) / disk_total;

        x = put_str(x, row, "DSK", disk_color);
        put_char(x++, row, '[', sep);

        int disk_filled = (int)((disk_pct * (uint32_t)bar_w) / 100u);
        if (disk_filled > bar_w) disk_filled = bar_w;
        draw_bar(x, row, bar_w, disk_filled, fill, empty);
        x += bar_w;

        put_char(x++, row, ']', sep);
        x = put_num(x, row, disk_used_mb, bg);
        put_char(x++, row, '/', bg);
        x = put_num(x, row, disk_total_mb, bg);
        x = put_str(x, row, "M", bg);
    } else {
        x = put_str(x, row, "DSK[--]", sep);
    }

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // CPU activity bar with sun emoji
    put_emoji(x, row, EMOJI_SUN, bg);
    x += 2;
    update_cpu_activity();
    x = put_str(x, row, "CPU", accent);
    put_char(x++, row, '[', sep);

    int cpu_bar_w = 6;
    int cpu_filled = (int)((cpu_activity * (uint32_t)cpu_bar_w) / 10u);
    if (cpu_filled > cpu_bar_w) cpu_filled = cpu_bar_w;

    // Color based on activity level
    uint8_t cpu_fill_color = fill;
    if (cpu_activity > 7) cpu_fill_color = color_cpu_high();
    else if (cpu_activity > 4) cpu_fill_color = color_cpu_med();

    draw_bar(x, row, cpu_bar_w, cpu_filled, cpu_fill_color, empty);
    x += cpu_bar_w;
    put_char(x++, row, ']', sep);

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // Task count with check emoji
    put_emoji(x, row, EMOJI_CHECK, bg);
    x += 2;
    uint32_t run = 0, sleep = 0, wait = 0, zomb = 0;
    tasking_get_state_counts(&run, &sleep, &wait, &zomb);
    uint32_t total_tasks = run + sleep + wait + zomb;

    x = put_num(x, row, run, accent);
    put_char(x++, row, '/', bg);
    x = put_num(x, row, total_tasks, bg);
    x = put_str(x, row, "T", bg);

    put_char(x++, row, ' ', bg);
    put_char(x++, row, '|', sep);
    put_char(x++, row, ' ', bg);

    // Virtual console indicator with rainbow emoji
    put_emoji(x, row, EMOJI_RAINBOW, bg);
    x += 2;
    x = put_str(x, row, "VC", accent);
    int vc = screen_console_active() + 1;  // 1-based for display
    x = put_num(x, row, (uint32_t)vc, bg);

    // Fill remaining columns with spaces (batch mode)
    while (x < cols) {
        put_char(x++, row, ' ', bg);
    }

    // Render cells without clearing first (flicker-free)
    screen_render_row_noclear(row);
}

void statusbar_init(void) {
    screen_set_reserved_bottom_rows(1);
    last_drawn_tick = 0xFFFFFFFFu;
    prev_ctx_switches = tasking_context_switch_count();
    draw_statusbar();
}

void statusbar_tick(void) {
    // Update every ~1 second (100 ticks at 100Hz)
    uint32_t tick = timer_get_ticks();
    if (tick - last_drawn_tick < 100) {
        return;
    }
    last_drawn_tick = tick;
    draw_statusbar();
}

void statusbar_refresh(void) {
    last_drawn_tick = 0xFFFFFFFFu;
    draw_statusbar();
}
