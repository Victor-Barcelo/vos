#include "statusbar.h"
#include "screen.h"
#include "timer.h"
#include "rtc.h"
#include "system.h"

static uint32_t last_drawn_minute = 0xFFFFFFFFu;

static uint8_t status_color(void) {
    return (uint8_t)(VGA_WHITE | (VGA_BLUE << 4));
}

static void append_char(char* buf, int* pos, int max, char c) {
    if (*pos >= max) return;
    buf[*pos] = c;
    (*pos)++;
}

static void append_str(char* buf, int* pos, int max, const char* s) {
    if (!s) return;
    while (*s) {
        append_char(buf, pos, max, *s++);
    }
}

static void append_u32_dec(char* buf, int* pos, int max, uint32_t value) {
    char tmp[11];
    int i = 0;
    if (value == 0) {
        append_char(buf, pos, max, '0');
        return;
    }
    while (value > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) {
        append_char(buf, pos, max, tmp[--i]);
    }
}

static void append_2d(char* buf, int* pos, int max, uint32_t v) {
    append_char(buf, pos, max, (char)('0' + ((v / 10) % 10)));
    append_char(buf, pos, max, (char)('0' + (v % 10)));
}

static void draw_statusbar(void) {
    int cols = screen_cols();
    int rows = screen_rows();
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > 255) cols = 255;

    char line[256];
    for (int i = 0; i < cols; i++) {
        line[i] = ' ';
    }
    line[cols] = '\0';

    int pos = 0;

    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        append_u32_dec(line, &pos, cols, (uint32_t)dt.year);
        append_char(line, &pos, cols, '-');
        append_2d(line, &pos, cols, dt.month);
        append_char(line, &pos, cols, '-');
        append_2d(line, &pos, cols, dt.day);
        append_char(line, &pos, cols, ' ');
        append_2d(line, &pos, cols, dt.hour);
        append_char(line, &pos, cols, ':');
        append_2d(line, &pos, cols, dt.minute);
    } else {
        append_str(line, &pos, cols, "RTC ?");
    }

    uint32_t up_ms = timer_uptime_ms();
    uint32_t up_min = up_ms / 60000u;
    append_str(line, &pos, cols, " | up ");
    append_u32_dec(line, &pos, cols, up_min);
    append_char(line, &pos, cols, 'm');

    uint32_t mem_kb = system_mem_total_kb();
    if (mem_kb != 0) {
        append_str(line, &pos, cols, " | mem ");
        append_u32_dec(line, &pos, cols, mem_kb / 1024u);
        append_str(line, &pos, cols, "MB");
    }

    const char* cpu = system_cpu_brand();
    if (!cpu || cpu[0] == '\0') {
        cpu = system_cpu_vendor();
    }
    while (cpu && (*cpu == ' ' || *cpu == '\t')) {
        cpu++;
    }
    if (cpu && cpu[0] != '\0') {
        append_str(line, &pos, cols, " | CPU ");
        append_str(line, &pos, cols, cpu);
    }

    uint8_t color = status_color();
    screen_fill_row(rows - 1, ' ', color);
    screen_write_string_at(0, rows - 1, line, color);
}

void statusbar_init(void) {
    screen_set_reserved_bottom_rows(1);
    last_drawn_minute = 0xFFFFFFFFu;
    draw_statusbar();
}

void statusbar_tick(void) {
    uint32_t minute = timer_uptime_ms() / 60000u;
    if (minute == last_drawn_minute) {
        return;
    }
    last_drawn_minute = minute;
    draw_statusbar();
}

void statusbar_refresh(void) {
    last_drawn_minute = 0xFFFFFFFFu;
    draw_statusbar();
}
