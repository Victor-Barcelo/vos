#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "syscall.h"

#define CLR_RESET "\x1b[0m"

// Rainbow colors for logo
#define C_RED     "\x1b[31;1m"
#define C_ORANGE  "\x1b[33m"
#define C_YELLOW  "\x1b[33;1m"
#define C_GREEN   "\x1b[32;1m"
#define C_CYAN    "\x1b[36;1m"
#define C_BLUE    "\x1b[34;1m"
#define C_MAGENTA "\x1b[35;1m"
#define C_WHITE   "\x1b[37;1m"

// Theme colors
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

// Print the colorful logo line by line
static void print_logo_line(int line) {
    switch (line) {
        case 0:
            printf(C_RED    "        " C_MAGENTA "██╗   ██╗" C_BLUE " ██████╗ " C_CYAN "███████╗" CLR_RESET);
            break;
        case 1:
            printf(C_RED    "  " C_YELLOW "╔═══╗" C_MAGENTA " ██║   ██║" C_BLUE "██╔═══██╗" C_CYAN "██╔════╝" CLR_RESET);
            break;
        case 2:
            printf(C_ORANGE "  " C_YELLOW "║" C_GREEN "▓▓▓" C_YELLOW "║" C_MAGENTA " ██║   ██║" C_BLUE "██║   ██║" C_CYAN "███████╗" CLR_RESET);
            break;
        case 3:
            printf(C_ORANGE "  " C_YELLOW "║" C_GREEN "▓▓▓" C_YELLOW "║" C_MAGENTA " ╚██╗ ██╔╝" C_BLUE "██║   ██║" C_CYAN "╚════██║" CLR_RESET);
            break;
        case 4:
            printf(C_YELLOW "  " C_YELLOW "╚═══╝" C_MAGENTA "  ╚████╔╝ " C_BLUE "╚██████╔╝" C_CYAN "███████║" CLR_RESET);
            break;
        case 5:
            printf(C_GREEN  "   " C_RED "◢██◣" C_MAGENTA "  ╚═══╝  " C_BLUE " ╚═════╝ " C_CYAN "╚══════╝" CLR_RESET);
            break;
        case 6:
            printf(C_GREEN  "  " C_RED "◢" C_YELLOW "████" C_RED "◣ " C_WHITE "  Victor's Operating System " CLR_RESET);
            break;
        case 7:
            printf(C_CYAN   " " C_RED "◢" C_ORANGE "██" C_YELLOW "██" C_GREEN "██" C_RED "◣" C_CYAN "   ─────────────────────────" CLR_RESET);
            break;
        case 8:
            printf(C_BLUE   "◢" C_RED "██" C_ORANGE "██" C_YELLOW "██" C_GREEN "██" C_CYAN "██" C_RED "◣" C_MAGENTA "  ● " C_RED "● " C_ORANGE "● " C_YELLOW "● " C_GREEN "● " C_CYAN "● " C_BLUE "● " C_MAGENTA "●" CLR_RESET);
            break;
        default:
            printf("                                           ");
            break;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const int logo_lines = 10;

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

    printf("\n");  // Top padding

    for (int line = 0; line < lines; line++) {
        print_logo_line(line);
        printf("  ");

        // Using emoji from VOS supported list (kernel/emoji_data.c)
        switch (line) {
            case 0:
                printf("\xF0\x9F\x98\x8E ");  // sunglasses face
                print_key("OS");
                printf(": " CLR_VAL "VOS 0.1.0" CLR_RESET " (i386)\n");
                break;
            case 1:
                printf("\xF0\x9F\x94\xA5 ");  // fire
                print_key("Kernel");
                printf(": " CLR_VAL "VOS kernel" CLR_RESET " (Multiboot1)\n");
                break;
            case 2:
                printf("\xE2\xAD\x90 ");  // star
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
                printf("\xF0\x9F\x8F\x86 ");  // trophy
                print_key("Uptime");
                printf(": " CLR_VAL);
                print_uptime_human(sys_uptime_ms());
                printf(CLR_RESET "\n");
                break;
            case 4:
                printf("\xF0\x9F\x92\xA1 ");  // light bulb
                print_key("Memory");
                if (mem_kb) {
                    printf(": " CLR_VAL "%lu MB" CLR_RESET "\n", (unsigned long)(mem_kb / 1024u));
                } else {
                    printf(": unknown\n");
                }
                break;
            case 5:
                printf("\xF0\x9F\x90\xA7 ");  // penguin
                print_key("CPU");
                if (cpu && cpu[0]) {
                    printf(": " CLR_VAL "%s" CLR_RESET "\n", cpu);
                } else {
                    printf(": unknown\n");
                }
                break;
            case 6:
                printf("\xE2\x98\x80 ");  // sun
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
                printf("\xE2\x9C\x85 ");  // check mark
                print_key("VFS");
                printf(": " CLR_VAL "%d files" CLR_RESET "\n", vfs_files);
                break;
            case 8:
                printf("\xE2\x9A\xA1 ");  // lightning
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
