#include "statusbar.h"
#include "screen.h"
#include "timer.h"
#include "rtc.h"
#include "system.h"

static uint32_t last_drawn_second = 0xFFFFFFFFu;

static uint8_t status_color(void) {
    return (uint8_t)(VGA_BLACK | (VGA_LIGHT_GREY << 4));
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
    char line[VGA_WIDTH + 1];
    for (int i = 0; i < VGA_WIDTH; i++) {
        line[i] = ' ';
    }
    line[VGA_WIDTH] = '\0';

    int pos = 0;

    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        append_u32_dec(line, &pos, VGA_WIDTH, (uint32_t)dt.year);
        append_char(line, &pos, VGA_WIDTH, '-');
        append_2d(line, &pos, VGA_WIDTH, dt.month);
        append_char(line, &pos, VGA_WIDTH, '-');
        append_2d(line, &pos, VGA_WIDTH, dt.day);
        append_char(line, &pos, VGA_WIDTH, ' ');
        append_2d(line, &pos, VGA_WIDTH, dt.hour);
        append_char(line, &pos, VGA_WIDTH, ':');
        append_2d(line, &pos, VGA_WIDTH, dt.minute);
        append_char(line, &pos, VGA_WIDTH, ':');
        append_2d(line, &pos, VGA_WIDTH, dt.second);
    } else {
        append_str(line, &pos, VGA_WIDTH, "RTC ?");
    }

    append_str(line, &pos, VGA_WIDTH, " | up ");
    append_u32_dec(line, &pos, VGA_WIDTH, timer_uptime_ms() / 1000u);
    append_char(line, &pos, VGA_WIDTH, 's');

    uint32_t mem_kb = system_mem_total_kb();
    if (mem_kb != 0) {
        append_str(line, &pos, VGA_WIDTH, " | mem ");
        append_u32_dec(line, &pos, VGA_WIDTH, mem_kb / 1024u);
        append_str(line, &pos, VGA_WIDTH, "MB");
    }

    const char* cpu = system_cpu_brand();
    if (!cpu || cpu[0] == '\0') {
        cpu = system_cpu_vendor();
    }
    while (cpu && (*cpu == ' ' || *cpu == '\t')) {
        cpu++;
    }
    if (cpu && cpu[0] != '\0') {
        append_str(line, &pos, VGA_WIDTH, " | CPU ");
        append_str(line, &pos, VGA_WIDTH, cpu);
    }

    uint8_t color = status_color();
    screen_fill_row(VGA_HEIGHT - 1, ' ', color);
    screen_write_string_at(0, VGA_HEIGHT - 1, line, color);
}

void statusbar_init(void) {
    screen_set_reserved_bottom_rows(1);
    last_drawn_second = 0xFFFFFFFFu;
    draw_statusbar();
}

void statusbar_tick(void) {
    uint32_t hz = timer_get_hz();
    uint32_t second = 0;
    if (hz != 0) {
        second = timer_get_ticks() / hz;
    }
    if (second == last_drawn_second) {
        return;
    }
    last_drawn_second = second;
    draw_statusbar();
}
