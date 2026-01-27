#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "syscall.h"

#define CLR_RESET "\x1b[0m"

// Theme colors
#define CLR_LOGO  "\x1b[36;1m" // bright cyan (logo)
#define CLR_KEY   "\x1b[33;1m" // bright yellow (labels)
#define CLR_VAL   "\x1b[37;1m" // bright white (values)

static void print_uptime_human(uint32_t ms) {
    uint32_t total = ms / 1000u;
    uint32_t days = total / 86400u;
    total %= 86400u;
    uint32_t hours = total / 3600u;
    total %= 3600u;
    uint32_t minutes = total / 60u;
    uint32_t seconds = total % 60u;

    if (days) {
        printf("%lud ", (unsigned long)days);
    }
    if (days || hours) {
        printf("%luh ", (unsigned long)hours);
    }
    if (days || hours || minutes) {
        printf("%lum ", (unsigned long)minutes);
    }
    printf("%lus", (unsigned long)seconds);
}

static void print_2d(unsigned int v) {
    putchar((int)('0' + ((v / 10u) % 10u)));
    putchar((int)('0' + (v % 10u)));
}

static void print_key(const char* k) {
    printf(CLR_KEY "%s" CLR_RESET, k);
}

static void trim_left(char** s) {
    if (!s || !*s) return;
    while (**s == ' ' || **s == '\t') (*s)++;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Simple clean ASCII logo (avoid backslashes which render oddly)
    static const char* const logo[] = {
        "   _    _  ____   _____ ",
        "  | |  | |/ __ \\ / ____|",
        "  | |  | | |  | | (___  ",
        "  | |  | | |  | |\\___ \\ ",
        "   \\ \\/ /| |__| |____) |",
        "    \\__/  \\____/|_____/ ",
        "",
    };
    const int logo_lines = (int)(sizeof(logo) / sizeof(logo[0]));

    int logo_width = 0;
    for (int i = 0; i < logo_lines; i++) {
        int len = (int)strlen(logo[i]);
        if (len > logo_width) logo_width = len;
    }

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    (void)ioctl(1, TIOCGWINSZ, &ws);

    uint32_t mem_kb = sys_mem_total_kb();

    char cpu_buf[128];
    cpu_buf[0] = '\0';
    (void)sys_cpu_brand(cpu_buf, (uint32_t)sizeof(cpu_buf));
    if (cpu_buf[0] == '\0') {
        (void)sys_cpu_vendor(cpu_buf, (uint32_t)sizeof(cpu_buf));
    }
    char* cpu = cpu_buf;
    trim_left(&cpu);

    vos_rtc_datetime_t dt;
    int rtc_rc = sys_rtc_get(&dt);

    int vfs_files = sys_vfs_file_count();
    if (vfs_files < 0) vfs_files = 0;

    int tasks = sys_task_count();
    if (tasks < 0) tasks = 0;

    const int info_lines = 10;
    int lines = (logo_lines > info_lines) ? logo_lines : info_lines;

    for (int line = 0; line < lines; line++) {
        const char* l = (line < logo_lines) ? logo[line] : "";
        int l_len = (int)strlen(l);

        printf(CLR_LOGO "%s" CLR_RESET, l);
        for (int i = l_len; i < logo_width; i++) putchar(' ');
        printf("  ");

        // Using only emoji from VOS supported list (kernel/emoji_data.c)
        switch (line) {
            case 0:
                // U+1F60E sunglasses face
                printf("\xF0\x9F\x98\x8E ");
                print_key("OS");
                printf(": " CLR_VAL "VOS 0.1.0" CLR_RESET " (i386)\n");
                break;
            case 1:
                // U+1F525 fire
                printf("\xF0\x9F\x94\xA5 ");
                print_key("Kernel");
                printf(": " CLR_VAL "VOS kernel" CLR_RESET " (Multiboot1)\n");
                break;
            case 2:
                // U+2B50 star
                printf("\xE2\xAD\x90 ");
                print_key("Display");
                if (ws.ws_xpixel && ws.ws_ypixel) {
                    printf(": " CLR_VAL "%ux%u" CLR_RESET " (%ux%u cells)\n",
                           (unsigned int)ws.ws_xpixel, (unsigned int)ws.ws_ypixel,
                           (unsigned int)ws.ws_col, (unsigned int)ws.ws_row);
                } else if (ws.ws_col && ws.ws_row) {
                    printf(": " CLR_VAL "%ux%u cells" CLR_RESET "\n",
                           (unsigned int)ws.ws_col, (unsigned int)ws.ws_row);
                } else {
                    printf(": unknown\n");
                }
                break;
            case 3:
                // U+1F3C6 trophy
                printf("\xF0\x9F\x8F\x86 ");
                print_key("Uptime");
                printf(": " CLR_VAL);
                print_uptime_human(sys_uptime_ms());
                printf(CLR_RESET "\n");
                break;
            case 4:
                // U+1F4A1 light bulb
                printf("\xF0\x9F\x92\xA1 ");
                print_key("Memory");
                if (mem_kb) {
                    printf(": " CLR_VAL "%lu MB" CLR_RESET "\n", (unsigned long)(mem_kb / 1024u));
                } else {
                    printf(": unknown\n");
                }
                break;
            case 5:
                // U+1F427 penguin
                printf("\xF0\x9F\x90\xA7 ");
                print_key("CPU");
                if (cpu && cpu[0]) {
                    printf(": " CLR_VAL "%s" CLR_RESET "\n", cpu);
                } else {
                    printf(": unknown\n");
                }
                break;
            case 6:
                // U+2600 sun (no variation selector)
                printf("\xE2\x98\x80  ");
                print_key("RTC");
                if (rtc_rc == 0) {
                    printf(": " CLR_VAL "%u-", (unsigned int)dt.year);
                    print_2d(dt.month);
                    putchar('-');
                    print_2d(dt.day);
                    putchar(' ');
                    print_2d(dt.hour);
                    putchar(':');
                    print_2d(dt.minute);
                    putchar(':');
                    print_2d(dt.second);
                    printf(CLR_RESET "\n");
                } else {
                    errno = -rtc_rc;
                    printf(": unavailable (%s)\n", strerror(errno));
                }
                break;
            case 7:
                // U+2705 check mark
                printf("\xE2\x9C\x85 ");
                print_key("VFS");
                printf(": " CLR_VAL "%d files" CLR_RESET "\n", vfs_files);
                break;
            case 8:
                // U+26A1 lightning
                printf("\xE2\x9A\xA1 ");
                print_key("Tasks");
                printf(": " CLR_VAL "%d" CLR_RESET "\n", tasks);
                break;
            default:
                putchar('\n');
                break;
        }
    }

    putchar('\n');
    return 0;
}
