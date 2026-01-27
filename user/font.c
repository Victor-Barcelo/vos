#define TB_IMPL
#include "../third_party/termbox2.h"
#include "syscall.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define FONT_CONFIG_DIR "/disk"
#define FONT_CONFIG_FILE "/disk/fontrc"

static void usage(void) {
    puts("usage:");
    puts("  font            # interactive menu");
    puts("  font list       # list available framebuffer fonts");
    puts("  font set <id>   # set font by numeric id");
    puts("  font set <name> # set font by name");
    puts("  font save       # save current font as default");
    puts("  font load       # load saved default font");
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

// Get the config file path (on persistent disk)
static const char* get_config_path(void) {
    return FONT_CONFIG_FILE;
}

// Save font index to config file
static int save_default_font(int font_idx) {
    const char* path = get_config_path();
    FILE* f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "%d\n", font_idx);
    fclose(f);
    return 0;
}

// Load font index from config file (-1 if not found)
static int load_default_font(void) {
    const char* path = get_config_path();
    FILE* f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    int font_idx = -1;
    if (fscanf(f, "%d", &font_idx) != 1) {
        font_idx = -1;
    }
    fclose(f);
    return font_idx;
}

// Draw a string at position (x, y) with given colors
static void tb_print_str(int x, int y, uint16_t fg, uint16_t bg, const char* str) {
    while (*str) {
        tb_set_cell(x++, y, (uint32_t)*str++, fg, bg);
    }
}

// Draw a centered string
static void tb_print_centered(int y, uint16_t fg, uint16_t bg, const char* str) {
    int w = tb_width();
    int len = (int)strlen(str);
    int x = (w - len) / 2;
    if (x < 0) x = 0;
    tb_print_str(x, y, fg, bg, str);
}

// Format number to string
static int num_to_str(char* buf, uint32_t val) {
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
    return i;
}

static void draw_ui(const vos_font_info_t* infos, int count, int sel, int cur, int scroll_offset, const char* message) {
    tb_clear();

    int w = tb_width();
    int h = tb_height();

    // Title bar
    for (int x = 0; x < w; x++) {
        tb_set_cell(x, 0, ' ', TB_WHITE, TB_BLUE);
    }
    tb_print_centered(0, TB_WHITE | TB_BOLD, TB_BLUE, " VOS Font Selector ");

    // Help line 1
    tb_print_str(2, 2, TB_CYAN, TB_DEFAULT, "Up/Down");
    tb_print_str(10, 2, TB_WHITE, TB_DEFAULT, ": Navigate  ");
    tb_print_str(22, 2, TB_CYAN, TB_DEFAULT, "a/Enter");
    tb_print_str(30, 2, TB_WHITE, TB_DEFAULT, ": Apply  ");
    tb_print_str(40, 2, TB_CYAN, TB_DEFAULT, "s");
    tb_print_str(42, 2, TB_WHITE, TB_DEFAULT, ": Save default  ");
    tb_print_str(58, 2, TB_CYAN, TB_DEFAULT, "q");
    tb_print_str(60, 2, TB_WHITE, TB_DEFAULT, ": Quit");

    // Table header
    int y = 4;
    tb_print_str(2, y, TB_YELLOW | TB_BOLD, TB_DEFAULT, "  #");
    tb_print_str(6, y, TB_YELLOW | TB_BOLD, TB_DEFAULT, "Name");
    tb_print_str(28, y, TB_YELLOW | TB_BOLD, TB_DEFAULT, "Size");

    // Separator
    y++;
    for (int x = 2; x < w - 2; x++) {
        tb_set_cell(x, y, '-', TB_WHITE, TB_DEFAULT);
    }

    // List fonts (scrollable)
    int list_start = 6;
    int list_max = h - 9;  // Leave room for header, footer, and message
    if (list_max < 1) list_max = 1;

    // Adjust scroll to keep selection visible
    int visible_sel = sel - scroll_offset;
    if (visible_sel < 0) {
        scroll_offset = sel;
    } else if (visible_sel >= list_max) {
        scroll_offset = sel - list_max + 1;
    }

    for (int i = 0; i < list_max && (scroll_offset + i) < count; i++) {
        int font_idx = scroll_offset + i;
        y = list_start + i;

        uint16_t fg = TB_WHITE;
        uint16_t bg = TB_DEFAULT;

        // Highlight selected row
        if (font_idx == sel) {
            fg = TB_BLACK;
            bg = TB_WHITE;
            // Fill entire row
            for (int x = 0; x < w; x++) {
                tb_set_cell(x, y, ' ', fg, bg);
            }
        }

        // Current font marker
        char marker = (font_idx == cur) ? '*' : ' ';
        tb_set_cell(2, y, (uint32_t)marker, fg == TB_BLACK ? TB_BLACK : TB_GREEN, bg);

        // Font number
        char num_buf[12];
        num_to_str(num_buf, (uint32_t)font_idx);
        int num_len = (int)strlen(num_buf);
        tb_print_str(5 - num_len, y, fg, bg, num_buf);

        // Font name
        tb_print_str(7, y, fg, bg, infos[font_idx].name);

        // Size
        char size_buf[32];
        num_to_str(size_buf, infos[font_idx].width);
        int pos = (int)strlen(size_buf);
        size_buf[pos++] = 'x';
        pos += num_to_str(size_buf + pos, infos[font_idx].height);
        tb_print_str(28, y, fg, bg, size_buf);
    }

    // Scroll indicators
    if (scroll_offset > 0) {
        tb_print_str(w - 4, list_start, TB_CYAN, TB_DEFAULT, "^^^");
    }
    if (scroll_offset + list_max < count) {
        tb_print_str(w - 4, list_start + list_max - 1, TB_CYAN, TB_DEFAULT, "vvv");
    }

    // Message line (above status)
    if (message && *message) {
        y = h - 3;
        tb_print_str(2, y, TB_GREEN | TB_BOLD, TB_DEFAULT, message);
    }

    // Status line
    y = h - 2;
    for (int x = 0; x < w; x++) {
        tb_set_cell(x, y, ' ', TB_WHITE, TB_BLUE);
    }
    char status[80];
    snprintf(status, sizeof(status), " Font %d/%d | Active: %s ", sel + 1, count, infos[cur].name);
    tb_print_str(2, y, TB_WHITE, TB_BLUE, status);

    tb_present();
}

static int interactive_menu(vos_font_info_t* infos, int count) {
    int cur = sys_font_get_current();
    if (cur < 0) {
        errno = -cur;
        fprintf(stderr, "font: %s\n", strerror(errno));
        return 1;
    }

    int rc = tb_init();
    if (rc != 0) {
        fprintf(stderr, "font: failed to initialize termbox: %d\n", rc);
        return 1;
    }

    int sel = cur;
    if (sel < 0) sel = 0;
    if (sel >= count) sel = count - 1;

    int scroll_offset = 0;
    int running = 1;
    char message[64] = "";

    draw_ui(infos, count, sel, cur, scroll_offset, message);

    struct tb_event ev;
    while (running) {
        rc = tb_poll_event(&ev);
        if (rc < 0) {
            break;
        }

        // Clear message on any key
        message[0] = '\0';

        if (ev.type == TB_EVENT_KEY) {
            if (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q') {
                running = 0;
            } else if (ev.key == TB_KEY_ARROW_UP || ev.ch == 'k') {
                if (sel > 0) {
                    sel--;
                    if (sel < scroll_offset) {
                        scroll_offset = sel;
                    }
                }
            } else if (ev.key == TB_KEY_ARROW_DOWN || ev.ch == 'j') {
                if (sel + 1 < count) {
                    sel++;
                    int h = tb_height();
                    int list_max = h - 9;
                    if (list_max < 1) list_max = 1;
                    if (sel >= scroll_offset + list_max) {
                        scroll_offset = sel - list_max + 1;
                    }
                }
            } else if (ev.key == TB_KEY_PGUP) {
                int h = tb_height();
                int list_max = h - 9;
                if (list_max < 1) list_max = 1;
                sel -= list_max;
                if (sel < 0) sel = 0;
                scroll_offset -= list_max;
                if (scroll_offset < 0) scroll_offset = 0;
            } else if (ev.key == TB_KEY_PGDN) {
                int h = tb_height();
                int list_max = h - 9;
                if (list_max < 1) list_max = 1;
                sel += list_max;
                if (sel >= count) sel = count - 1;
                scroll_offset += list_max;
                int max_scroll = count - list_max;
                if (max_scroll < 0) max_scroll = 0;
                if (scroll_offset > max_scroll) scroll_offset = max_scroll;
            } else if (ev.key == TB_KEY_HOME) {
                sel = 0;
                scroll_offset = 0;
            } else if (ev.key == TB_KEY_END) {
                sel = count - 1;
                int h = tb_height();
                int list_max = h - 9;
                if (list_max < 1) list_max = 1;
                scroll_offset = count - list_max;
                if (scroll_offset < 0) scroll_offset = 0;
            } else if (ev.key == TB_KEY_ENTER || ev.key == TB_KEY_CTRL_M ||
                       ev.ch == '\r' || ev.ch == '\n' || ev.ch == 0x0d ||
                       ev.ch == ' ' || ev.ch == 'a' || ev.ch == 'A') {
                // Apply selected font (but stay in menu)
                int set_rc = sys_font_set((uint32_t)sel);
                if (set_rc < 0) {
                    snprintf(message, sizeof(message), "Error applying font!");
                } else {
                    cur = sel;  // Update current
                    snprintf(message, sizeof(message), "Applied: %s", infos[sel].name);
                    // Font change affects screen size - reinit termbox
                    tb_shutdown();
                    if (tb_init() != 0) {
                        return 1;
                    }
                }
            } else if (ev.ch == 's' || ev.ch == 'S') {
                // Save current font as default
                if (save_default_font(cur) == 0) {
                    snprintf(message, sizeof(message), "Saved %s as default", infos[cur].name);
                } else {
                    snprintf(message, sizeof(message), "Error saving default!");
                }
            }

            draw_ui(infos, count, sel, cur, scroll_offset, message);
        } else if (ev.type == TB_EVENT_RESIZE) {
            draw_ui(infos, count, sel, cur, scroll_offset, message);
        }
    }

    tb_shutdown();
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

    if (argc >= 2 && strcmp(argv[1], "save") == 0) {
        int cur = sys_font_get_current();
        if (cur < 0) {
            fprintf(stderr, "font: cannot get current font\n");
            free(infos);
            return 1;
        }
        if (save_default_font(cur) == 0) {
            printf("Saved %s as default font\n", infos[cur].name);
        } else {
            fprintf(stderr, "font: failed to save default\n");
            free(infos);
            return 1;
        }
        free(infos);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "load") == 0) {
        int def = load_default_font();
        if (def < 0 || def >= count) {
            fprintf(stderr, "font: no saved default font\n");
            free(infos);
            return 1;
        }
        int rc = sys_font_set((uint32_t)def);
        if (rc < 0) {
            errno = -rc;
            fprintf(stderr, "font: %s\n", strerror(errno));
            free(infos);
            return 1;
        }
        printf("Loaded default font: %s\n", infos[def].name);
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
