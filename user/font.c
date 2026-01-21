#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "syscall.h"

static void usage(void) {
    puts("usage:");
    puts("  font            # interactive menu (arrows + enter)");
    puts("  font list       # list available framebuffer fonts");
    puts("  font set <id>   # set font by numeric id");
    puts("  font set <name> # set font by name");
}

static int fetch_fonts(vos_font_info_t* infos, int count) {
    if (!infos || count <= 0) return -1;
    for (int i = 0; i < count; i++) {
        memset(&infos[i], 0, sizeof(infos[i]));
        int rc = sys_font_info((uint32_t)i, &infos[i]);
        if (rc < 0) {
            snprintf(infos[i].name, sizeof(infos[i].name), "font-%d", i);
            infos[i].width = 0;
            infos[i].height = 0;
        }
    }
    return 0;
}

static int find_font_by_name(const vos_font_info_t* infos, int count, const char* name) {
    if (!infos || count <= 0 || !name || !*name) return -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(infos[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void compute_cells(uint32_t px_w, uint32_t px_h, uint32_t font_w, uint32_t font_h, int* out_cols, int* out_rows) {
    if (out_cols) *out_cols = 0;
    if (out_rows) *out_rows = 0;
    if (font_w == 0 || font_h == 0) return;
    if (px_w == 0 || px_h == 0) return;

    int cols_total = (int)(px_w / font_w);
    int rows_total = (int)(px_h / font_h);
    int pad = 1;

    int cols = cols_total;
    int rows = rows_total;
    if (cols_total > (pad * 2)) cols = cols_total - (pad * 2);
    if (rows_total > (pad * 2)) rows = rows_total - (pad * 2);

    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (out_cols) *out_cols = cols;
    if (out_rows) *out_rows = rows;
}

static int interactive_menu(vos_font_info_t* infos, int count) {
    int cur = sys_font_get_current();
    if (cur < 0) {
        errno = -cur;
        fprintf(stderr, "font: %s\n", strerror(errno));
        return 1;
    }

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    (void)ioctl(1, TIOCGWINSZ, &ws);

    struct termios orig;
    if (tcgetattr(0, &orig) != 0) {
        perror("font: tcgetattr");
        return 1;
    }

    struct termios raw = orig;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    (void)tcsetattr(0, TCSAFLUSH, &raw);

    // Hide cursor.
    fputs("\x1b[?25l", stdout);

    int sel = cur;
    if (sel < 0) sel = 0;
    if (sel >= count) sel = count - 1;

    for (;;) {
        // Clear + home.
        fputs("\x1b[2J\x1b[H", stdout);
        puts("VOS font selector (use \x1b[1mUp/Down\x1b[0m, Enter to apply, q to quit)\n");

        for (int i = 0; i < count; i++) {
            int cols = 0, rows = 0;
            compute_cells((uint32_t)ws.ws_xpixel, (uint32_t)ws.ws_ypixel, infos[i].width, infos[i].height, &cols, &rows);

            if (i == sel) {
                fputs("\x1b[7m", stdout);
            }

            printf("%c %2d) %-20s %2lux%2lu px  ~%dx%d cells",
                   (i == cur) ? '*' : ' ',
                   i,
                   infos[i].name,
                   (unsigned long)infos[i].width,
                   (unsigned long)infos[i].height,
                   cols,
                   rows);

            if (i == sel) {
                fputs("\x1b[0m", stdout);
            }
            putchar('\n');
        }

        fflush(stdout);

        char c = 0;
        if (read(0, &c, 1u) != 1) {
            break;
        }

        if (c == 'q' || c == 'Q') {
            break;
        }

        if (c == '\r' || c == '\n') {
            int rc = sys_font_set((uint32_t)sel);
            if (rc < 0) {
                errno = -rc;
                fprintf(stderr, "\nfont: %s\n", strerror(errno));
                (void)sys_sleep(1200u);
                continue;
            }
            // The kernel clears/redraws the console on font switch; restore cursor and exit.
            fputs("\x1b[?25h\x1b[0m", stdout);
            fflush(stdout);
            (void)tcsetattr(0, TCSAFLUSH, &orig);
            return 0;
        }

        if (c == '\x1b') {
            char seq0 = 0;
            char seq1 = 0;
            if (read(0, &seq0, 1u) != 1) {
                break;
            }
            if (seq0 == '[') {
                if (read(0, &seq1, 1u) != 1) {
                    break;
                }
                if (seq1 == 'A') { // up
                    if (sel > 0) sel--;
                } else if (seq1 == 'B') { // down
                    if (sel + 1 < count) sel++;
                }
            }
        }
    }

    // Show cursor and restore settings.
    fputs("\x1b[?25h\x1b[0m", stdout);
    fflush(stdout);
    (void)tcsetattr(0, TCSAFLUSH, &orig);
    return 0;
}

int main(int argc, char** argv) {
    int count = sys_font_count();
    if (count < 0) {
        errno = -count;
        fprintf(stderr, "font: %s\n", strerror(errno));
        return 1;
    }
    if (count == 0) {
        fputs("font: no fonts available\n", stderr);
        return 1;
    }

    if (count > 64) {
        count = 64;
    }

    vos_font_info_t* infos = (vos_font_info_t*)malloc((size_t)count * sizeof(*infos));
    if (!infos) {
        fputs("font: out of memory\n", stderr);
        return 1;
    }
    (void)fetch_fonts(infos, count);

    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        usage();
        free(infos);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "list") == 0) {
        int cur = sys_font_get_current();
        for (int i = 0; i < count; i++) {
            char mark = (i == cur) ? '*' : ' ';
            printf("%c%2d  %-20s %lux%lu\n",
                   mark,
                   i,
                   infos[i].name,
                   (unsigned long)infos[i].width,
                   (unsigned long)infos[i].height);
        }
        free(infos);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set") == 0) {
        const char* arg = argv[2];
        char* end = NULL;
        long id = strtol(arg, &end, 10);
        int idx = -1;

        if (end && *end == '\0') {
            idx = (int)id;
        } else {
            idx = find_font_by_name(infos, count, arg);
        }

        if (idx < 0 || idx >= count) {
            fprintf(stderr, "font: unknown font '%s'\n", arg);
            usage();
            free(infos);
            return 1;
        }

        int rc = sys_font_set((uint32_t)idx);
        if (rc < 0) {
            errno = -rc;
            fprintf(stderr, "font: %s\n", strerror(errno));
            free(infos);
            return 1;
        }

        free(infos);
        return 0;
    }

    if (argc != 1) {
        usage();
        free(infos);
        return 1;
    }

    int rc = interactive_menu(infos, count);
    free(infos);
    return rc;
}
