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
    puts("  theme            # interactive menu (arrows + enter)");
    puts("  theme list       # list available color themes");
    puts("  theme set <id>   # set theme by numeric id");
    puts("  theme set <name> # set theme by name");
}

typedef struct {
    char name[64];
} theme_info_t;

static int fetch_themes(theme_info_t* infos, int count) {
    if (!infos || count <= 0) return -1;
    for (int i = 0; i < count; i++) {
        memset(&infos[i], 0, sizeof(infos[i]));
        int rc = sys_theme_info((uint32_t)i, infos[i].name, sizeof(infos[i].name));
        if (rc < 0) {
            snprintf(infos[i].name, sizeof(infos[i].name), "theme-%d", i);
        }
    }
    return 0;
}

static int find_theme_by_name(const theme_info_t* infos, int count, const char* name) {
    if (!infos || count <= 0 || !name || !*name) return -1;
    for (int i = 0; i < count; i++) {
        if (strcasecmp(infos[i].name, name) == 0) {
            return i;
        }
    }
    // Partial match
    for (int i = 0; i < count; i++) {
        if (strstr(infos[i].name, name) != NULL) {
            return i;
        }
    }
    return -1;
}

static int interactive_menu(theme_info_t* infos, int count) {
    int cur = sys_theme_get_current();
    if (cur < 0) {
        errno = -cur;
        fprintf(stderr, "theme: %s\n", strerror(errno));
        return 1;
    }

    struct termios orig;
    if (tcgetattr(0, &orig) != 0) {
        perror("theme: tcgetattr");
        return 1;
    }

    struct termios raw = orig;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    (void)tcsetattr(0, TCSAFLUSH, &raw);

    // Hide cursor
    fputs("\x1b[?25l", stdout);

    int sel = cur;
    if (sel < 0) sel = 0;
    if (sel >= count) sel = count - 1;

    for (;;) {
        // Clear + home
        fputs("\x1b[2J\x1b[H", stdout);
        puts("VOS color theme selector (use \x1b[1mUp/Down\x1b[0m, Enter to apply, q to quit)\n");

        for (int i = 0; i < count; i++) {
            if (i == sel) {
                fputs("\x1b[7m", stdout);
            }

            printf("%c %2d) %-20s",
                   (i == cur) ? '*' : ' ',
                   i,
                   infos[i].name);

            if (i == sel) {
                fputs("\x1b[0m", stdout);
            }
            putchar('\n');
        }

        // Show color preview
        printf("\n\x1b[1mColor preview:\x1b[0m ");
        printf("\x1b[30m\x1b[40m  \x1b[0m");  // black on black
        printf("\x1b[31m\x1b[40mR \x1b[0m");  // red
        printf("\x1b[32m\x1b[40mG \x1b[0m");  // green
        printf("\x1b[33m\x1b[40mY \x1b[0m");  // yellow
        printf("\x1b[34m\x1b[40mB \x1b[0m");  // blue
        printf("\x1b[35m\x1b[40mM \x1b[0m");  // magenta
        printf("\x1b[36m\x1b[40mC \x1b[0m");  // cyan
        printf("\x1b[37m\x1b[40mW \x1b[0m");  // white
        printf("\x1b[90m\x1b[40mD \x1b[0m");  // bright black
        printf("\x1b[91m\x1b[40mR \x1b[0m");  // bright red
        printf("\x1b[92m\x1b[40mG \x1b[0m");  // bright green
        printf("\x1b[93m\x1b[40mY \x1b[0m");  // bright yellow
        printf("\x1b[94m\x1b[40mB \x1b[0m");  // bright blue
        printf("\x1b[95m\x1b[40mM \x1b[0m");  // bright magenta
        printf("\x1b[96m\x1b[40mC \x1b[0m");  // bright cyan
        printf("\x1b[97m\x1b[40mW \x1b[0m");  // bright white
        putchar('\n');

        fflush(stdout);

        char c = 0;
        if (read(0, &c, 1u) != 1) {
            break;
        }

        if (c == 'q' || c == 'Q') {
            break;
        }

        if (c == '\r' || c == '\n') {
            int rc = sys_theme_set((uint32_t)sel);
            if (rc < 0) {
                errno = -rc;
                fprintf(stderr, "\ntheme: %s\n", strerror(errno));
                (void)sys_sleep(1200u);
                continue;
            }
            cur = sel;
            // Screen will be redrawn with new colors
            continue;
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

    // Show cursor and restore settings
    fputs("\x1b[?25h\x1b[0m", stdout);
    fflush(stdout);
    (void)tcsetattr(0, TCSAFLUSH, &orig);
    return 0;
}

int main(int argc, char** argv) {
    int count = sys_theme_count();
    if (count < 0) {
        errno = -count;
        fprintf(stderr, "theme: %s\n", strerror(errno));
        return 1;
    }
    if (count == 0) {
        fputs("theme: no themes available\n", stderr);
        return 1;
    }

    if (count > 64) {
        count = 64;
    }

    theme_info_t* infos = (theme_info_t*)malloc((size_t)count * sizeof(*infos));
    if (!infos) {
        fputs("theme: out of memory\n", stderr);
        return 1;
    }
    (void)fetch_themes(infos, count);

    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        usage();
        free(infos);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "list") == 0) {
        int cur = sys_theme_get_current();
        for (int i = 0; i < count; i++) {
            char mark = (i == cur) ? '*' : ' ';
            printf("%c%2d  %s\n", mark, i, infos[i].name);
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
            idx = find_theme_by_name(infos, count, arg);
        }

        if (idx < 0 || idx >= count) {
            fprintf(stderr, "theme: unknown theme '%s'\n", arg);
            usage();
            free(infos);
            return 1;
        }

        int rc = sys_theme_set((uint32_t)idx);
        if (rc < 0) {
            errno = -rc;
            fprintf(stderr, "theme: %s\n", strerror(errno));
            free(infos);
            return 1;
        }

        printf("Theme set to: %s\n", infos[idx].name);
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
