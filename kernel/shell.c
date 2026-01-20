#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"
#include "timer.h"
#include "rtc.h"
#include "statusbar.h"
#include "vfs.h"
#include "elf.h"
#include "paging.h"
#include "task.h"
#include "system.h"
#include "ctype.h"
#include "ubasic.h"
#include "basic_programs.h"
#include "stdlib.h"
#include "ramfs.h"
#include "editor.h"
#include "microrl.h"
#include "fatdisk.h"
#include "kheap.h"

#define MAX_COMMAND_LENGTH 256
#define BASIC_PROGRAM_SIZE 4096
#define VOS_VERSION "0.1.0"
#define SHELL_PATH_MAX 128
#define LS_MAX_ENTRIES 128

enum {
    SYS_WRITE = 0,
    SYS_EXIT  = 1,
    SYS_YIELD = 2,
    SYS_SLEEP = 3,
    SYS_WAIT  = 4,
    SYS_KILL  = 5,
};

// Forward declarations for commands
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char* args);
static void cmd_info(void);
static void cmd_fetch(void);
static void cmd_reboot(void);
static void cmd_halt(void);
static void cmd_color(const char* args);
static void cmd_basic(void);
static void cmd_uptime(void);
static void cmd_sleep(const char* args);
static void cmd_date(void);
static void cmd_setdate(const char* args);
static void cmd_ls(const char* args);
static void cmd_cat(const char* args);
static void cmd_run(const char* args);
static void cmd_ps(void);
static void cmd_top(void);
static void cmd_kill(const char* args);
static void cmd_wait(const char* args);
static void cmd_pwd(void);
static void cmd_cd(const char* args);
static void cmd_mkdir(const char* args);
static void cmd_cp(const char* args);
static void cmd_mv(const char* args);
static void cmd_nano(const char* args);

static void execute_command(char* input);

static char shell_cwd[SHELL_PATH_MAX] = "/";

static microrl_t shell_rl;
static char shell_prompt_buf[256];

static void shell_update_prompt(void) {
    size_t pos = 0;
    const char* pfx = "vos:";
    while (*pfx && pos + 1 < sizeof(shell_prompt_buf)) {
        shell_prompt_buf[pos++] = *pfx++;
    }
    const char* cwd = shell_cwd;
    while (cwd && *cwd && pos + 1 < sizeof(shell_prompt_buf)) {
        shell_prompt_buf[pos++] = *cwd++;
    }
    const char* sfx = "> ";
    while (*sfx && pos + 1 < sizeof(shell_prompt_buf)) {
        shell_prompt_buf[pos++] = *sfx++;
    }
    shell_prompt_buf[pos] = '\0';
    microrl_set_prompt(&shell_rl, shell_prompt_buf, (int)pos);
}

static void shell_rl_print(const char* s) {
    if (!s) {
        return;
    }
    screen_print(s);
}

static void join_argv(char* out, size_t out_len, int argc, const char* const* argv) {
    if (!out || out_len == 0) {
        return;
    }
    size_t pos = 0;
    out[0] = '\0';

    for (int i = 0; i < argc; i++) {
        const char* token = argv[i] ? argv[i] : "";
        if (i > 0 && pos + 1 < out_len) {
            out[pos++] = ' ';
        }
        while (*token && pos + 1 < out_len) {
            out[pos++] = *token++;
        }
    }

    out[pos] = '\0';
}

static void microrl_feed_seq(microrl_t* rl, const char* seq) {
    if (!rl || !seq) {
        return;
    }
    while (*seq) {
        microrl_insert_char(rl, (unsigned char)*seq++);
    }
}

static void microrl_feed_arrow(microrl_t* rl, char final) {
    char seq[4] = { 27, '[', final, 0 };
    microrl_feed_seq(rl, seq);
}

static void microrl_feed_home_end(microrl_t* rl, bool home) {
    char seq[5] = { 27, '[', (char)(home ? '7' : '8'), '~', 0 };
    microrl_feed_seq(rl, seq);
}

static int shell_rl_execute(int argc, const char* const* argv) {
    char line[MAX_COMMAND_LENGTH];
    join_argv(line, sizeof(line), argc, argv);
    execute_command(line);

    screen_set_color(VGA_WHITE, VGA_BLUE);
    shell_update_prompt();
    return 0;
}

static void print_spaces(int count) {
    for (int i = 0; i < count; i++) {
        screen_putchar(' ');
    }
}

static const char* skip_slashes(const char* p) {
    while (p && *p == '/') {
        p++;
    }
    return p ? p : "";
}

static bool ci_eq(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
    }
}

static bool ci_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }
    while (*prefix) {
        char cs = *s++;
        char cp = *prefix++;
        if (tolower((unsigned char)cs) != tolower((unsigned char)cp)) {
            return false;
        }
    }
    return true;
}

static bool is_ram_path_abs(const char* abs_path);
static bool is_disk_path_abs(const char* abs_path);

static bool resolve_path(const char* cwd, const char* in, char* out, uint32_t out_cap) {
    if (!out || out_cap == 0) {
        return false;
    }
    if (!cwd || cwd[0] != '/') {
        cwd = "/";
    }
    if (!in || in[0] == '\0') {
        in = ".";
    }

    char tmp[SHELL_PATH_MAX];
    tmp[0] = '\0';

    if (in[0] == '/') {
        strncpy(tmp, in, sizeof(tmp) - 1u);
        tmp[sizeof(tmp) - 1u] = '\0';
    } else {
        if (strcmp(cwd, "/") == 0) {
            strncpy(tmp, "/", sizeof(tmp) - 1u);
            tmp[sizeof(tmp) - 1u] = '\0';
        } else {
            strncpy(tmp, cwd, sizeof(tmp) - 1u);
            tmp[sizeof(tmp) - 1u] = '\0';
        }
        if (strlen(tmp) + 1u < sizeof(tmp)) {
            if (tmp[strlen(tmp) - 1u] != '/') {
                strncat(tmp, "/", sizeof(tmp) - strlen(tmp) - 1u);
            }
        }
        strncat(tmp, in, sizeof(tmp) - strlen(tmp) - 1u);
    }

    uint32_t out_len = 0;
    out[out_len++] = '/';

    uint32_t saved[32];
    uint32_t depth = 0;

    const char* p = tmp;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;

        const char* seg = p;
        uint32_t seg_len = 0;
        while (p[seg_len] && p[seg_len] != '/') seg_len++;
        p += seg_len;

        if (seg_len == 1 && seg[0] == '.') {
            continue;
        }
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (depth) {
                out_len = saved[--depth];
            }
            continue;
        }

        if (depth >= (uint32_t)(sizeof(saved) / sizeof(saved[0]))) {
            return false;
        }
        saved[depth++] = out_len;

        uint32_t need = seg_len + ((out_len > 1u) ? 1u : 0u) + 1u;
        if (out_len + need > out_cap) {
            return false;
        }

        if (out_len > 1u) {
            out[out_len++] = '/';
        }
        for (uint32_t i = 0; i < seg_len; i++) {
            out[out_len++] = seg[i];
        }
    }

    if (out_len >= out_cap) {
        return false;
    }
    out[out_len] = '\0';
    return true;
}

static bool vfs_dir_exists(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return false;
    }
    const char* rel = skip_slashes(abs_path);
    if (rel[0] == '\0') {
        return true; // root
    }

    if (ramfs_is_dir(abs_path)) {
        return true;
    }

    if (is_disk_path_abs(abs_path)) {
        return fatdisk_is_dir(abs_path);
    }

    uint32_t count = vfs_file_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* name = vfs_file_name(i);
        if (!name) {
            continue;
        }
        const char* n = skip_slashes(name);
        if (!ci_starts_with(n, rel)) {
            continue;
        }
        uint32_t rel_len = (uint32_t)strlen(rel);
        if (n[rel_len] == '/') {
            return true;
        }
    }
    return false;
}

static bool vfs_file_exists(const char* abs_path) {
    if (is_disk_path_abs(abs_path)) {
        return fatdisk_is_file(abs_path);
    }
    const uint8_t* data = NULL;
    uint32_t size = 0;
    return vfs_read_file(abs_path, &data, &size) && data != NULL;
}

static int split_args_inplace(char* s, char* argv[], int max) {
    if (!s || !argv || max <= 0) {
        return 0;
    }

    int argc = 0;
    while (*s && argc < max) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        argv[argc++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (*s) {
            *s++ = '\0';
        }
    }
    return argc;
}

static bool is_ram_path_abs(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return false;
    }
    const char* rel = skip_slashes(abs_path);
    if (ci_eq(rel, "ram")) {
        return true;
    }
    return ci_starts_with(rel, "ram/");
}

static bool is_disk_path_abs(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return false;
    }
    const char* rel = skip_slashes(abs_path);
    if (ci_eq(rel, "disk")) {
        return true;
    }
    return ci_starts_with(rel, "disk/");
}

static const char* path_basename(const char* abs_path) {
    if (!abs_path) {
        return "";
    }
    const char* last = strrchr(abs_path, '/');
    return last ? last + 1 : abs_path;
}

static bool path_join(char* out, uint32_t out_cap, const char* a, const char* b) {
    if (!out || out_cap == 0 || !a || !b) {
        return false;
    }
    uint32_t alen = (uint32_t)strlen(a);
    uint32_t blen = (uint32_t)strlen(b);
    bool need_slash = true;
    if (alen == 0 || a[alen - 1u] == '/') {
        need_slash = false;
    }
    uint32_t need = alen + (need_slash ? 1u : 0u) + blen + 1u;
    if (need > out_cap) {
        return false;
    }
    memcpy(out, a, alen);
    uint32_t pos = alen;
    if (need_slash) {
        out[pos++] = '/';
    }
    memcpy(out + pos, b, blen);
    out[pos + blen] = '\0';
    return true;
}

static void print_banner_key(const char* key) {
    screen_set_color(VGA_YELLOW, VGA_BLUE);
    screen_print(key);
    screen_set_color(VGA_WHITE, VGA_BLUE);
}

static void print_uptime_human(uint32_t uptime_ms) {
    uint32_t seconds = uptime_ms / 1000u;
    uint32_t days = seconds / 86400u;
    seconds %= 86400u;
    uint32_t hours = seconds / 3600u;
    seconds %= 3600u;
    uint32_t minutes = seconds / 60u;
    seconds %= 60u;

    bool printed = false;
    if (days) {
        screen_print_dec((int32_t)days);
        screen_print("d ");
        printed = true;
    }
    if (hours || printed) {
        screen_print_dec((int32_t)hours);
        screen_print("h ");
        printed = true;
    }
    if (minutes || printed) {
        screen_print_dec((int32_t)minutes);
        screen_print("m ");
    }
    screen_print_dec((int32_t)seconds);
    screen_print("s");
}

static void print_neofetch_like_banner(void) {
    static const char* const logo[] = {
        " _    __  ____   _____ ",
        "| |  / / / __ \\ / ____|",
        "| | / / | |  | | (___  ",
        "| |/ /  | |  | |\\___ \\ ",
        "|   <   | |__| |____) |",
        "|_|\\_\\   \\____/|_____/ ",
    };
    const int logo_lines = (int)(sizeof(logo) / sizeof(logo[0]));

    int logo_width = 0;
    for (int i = 0; i < logo_lines; i++) {
        int len = (int)strlen(logo[i]);
        if (len > logo_width) {
            logo_width = len;
        }
    }

    const int info_lines = 12;
    int lines = (logo_lines > info_lines) ? logo_lines : info_lines;

    for (int line = 0; line < lines; line++) {
        const char* l = (line < logo_lines) ? logo[line] : "";
        int l_len = (int)strlen(l);

        screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
        screen_print(l);
        if (l_len < logo_width) {
            print_spaces(logo_width - l_len);
        }
        screen_set_color(VGA_WHITE, VGA_BLUE);
        print_spaces(2);

        switch (line) {
            case 0: {
                screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
                screen_print("kernel@vos");
                screen_set_color(VGA_WHITE, VGA_BLUE);
                break;
            }
            case 1: {
                screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
                screen_print("----------");
                screen_set_color(VGA_WHITE, VGA_BLUE);
                break;
            }
            case 2: {
                print_banner_key("OS");
                screen_print(": VOS ");
                screen_print(VOS_VERSION);
                screen_print(" (i386)");
                break;
            }
            case 3: {
                print_banner_key("Kernel");
                screen_print(": VOS kernel (Multiboot1)");
                break;
            }
            case 4: {
                print_banner_key("Display");
                screen_print(": ");
                if (screen_is_framebuffer()) {
                    uint32_t w = screen_framebuffer_width();
                    uint32_t h = screen_framebuffer_height();
                    uint32_t bpp = screen_framebuffer_bpp();
                    screen_print_dec((int32_t)w);
                    screen_putchar('x');
                    screen_print_dec((int32_t)h);
                    if (bpp) {
                        screen_print("x");
                        screen_print_dec((int32_t)bpp);
                    }
                    screen_print(" (");
                    screen_print_dec((int32_t)screen_cols());
                    screen_putchar('x');
                    screen_print_dec((int32_t)screen_rows());
                    screen_print(" cells)");
                } else {
                    screen_print("VGA text (");
                    screen_print_dec((int32_t)screen_cols());
                    screen_putchar('x');
                    screen_print_dec((int32_t)screen_rows());
                    screen_print(" cells)");
                }
                break;
            }
            case 5: {
                print_banner_key("Font");
                screen_print(": ");
                if (screen_is_framebuffer()) {
                    screen_print("PSF2 ");
                    screen_print_dec((int32_t)screen_font_width());
                    screen_putchar('x');
                    screen_print_dec((int32_t)screen_font_height());
                    screen_print(" px");
                } else {
                    screen_print("VGA text mode");
                }
                break;
            }
            case 6: {
                print_banner_key("Uptime");
                screen_print(": ");
                print_uptime_human(timer_uptime_ms());
                break;
            }
            case 7: {
                print_banner_key("Memory");
                screen_print(": ");
                uint32_t kb = system_mem_total_kb();
                if (kb) {
                    screen_print_dec((int32_t)(kb / 1024u));
                    screen_print(" MB");
                } else {
                    screen_print("unknown");
                }
                break;
            }
            case 8: {
                print_banner_key("CPU");
                screen_print(": ");
                const char* cpu = system_cpu_brand();
                if (!cpu || cpu[0] == '\0') {
                    cpu = system_cpu_vendor();
                }
                while (cpu && (*cpu == ' ' || *cpu == '\t')) {
                    cpu++;
                }
                if (cpu && cpu[0] != '\0') {
                    screen_print(cpu);
                } else {
                    screen_print("unknown");
                }
                break;
            }
            case 9: {
                print_banner_key("RTC");
                screen_print(": ");
                rtc_datetime_t dt;
                if (rtc_read_datetime(&dt)) {
                    screen_print_dec((int32_t)dt.year);
                    screen_putchar('-');
                    if (dt.month < 10) screen_putchar('0');
                    screen_print_dec((int32_t)dt.month);
                    screen_putchar('-');
                    if (dt.day < 10) screen_putchar('0');
                    screen_print_dec((int32_t)dt.day);
                    screen_putchar(' ');
                    if (dt.hour < 10) screen_putchar('0');
                    screen_print_dec((int32_t)dt.hour);
                    screen_putchar(':');
                    if (dt.minute < 10) screen_putchar('0');
                    screen_print_dec((int32_t)dt.minute);
                    screen_putchar(':');
                    if (dt.second < 10) screen_putchar('0');
                    screen_print_dec((int32_t)dt.second);
                } else {
                    screen_print("unavailable");
                }
                break;
            }
            case 10: {
                print_banner_key("VFS");
                screen_print(": ");
                if (vfs_is_ready()) {
                    screen_print_dec((int32_t)vfs_file_count());
                    screen_print(" files");
                } else {
                    screen_print("not loaded");
                }
                break;
            }
            case 11: {
                print_banner_key("Tasking");
                screen_print(": ");
                screen_print(tasking_is_enabled() ? "enabled" : "disabled");
                break;
            }
            default:
                break;
        }

        screen_putchar('\n');
    }

    screen_putchar('\n');
}

static void print_help_cmd(const char* cmd, const char* desc) {
    screen_set_color(VGA_YELLOW, VGA_BLUE);
    screen_print("  ");
    screen_print(cmd);

    int pad = 14 - (int)strlen(cmd);
    if (pad < 1) pad = 1;
    for (int i = 0; i < pad; i++) {
        screen_putchar(' ');
    }

    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("- ");
    screen_println(desc);
}

static void shell_idle_hook(void) {
    statusbar_tick();

    static bool cursor_on = true;
    static uint32_t next_toggle_tick = 0;

    uint32_t hz = timer_get_hz();
    if (hz == 0) {
        return;
    }

    uint32_t now = timer_get_ticks();
    if ((int32_t)(now - next_toggle_tick) < 0) {
        return;
    }

    cursor_on = !cursor_on;
    screen_cursor_set_enabled(cursor_on);

    uint32_t interval = hz / 2u;
    if (interval == 0) {
        interval = 1;
    }
    next_toggle_tick = now + interval;
}

// Parse and execute a command
static void execute_command(char* input) {
    // Skip leading whitespace
    while (*input == ' ') input++;

    // Empty command
    if (*input == '\0') {
        return;
    }

    // Find the command and arguments
    char* args = input;
    while (*args && *args != ' ') args++;
    if (*args == ' ') {
        *args = '\0';
        args++;
        // Skip whitespace after command
        while (*args == ' ') args++;
    }

    // Match commands
    if (strcmp(input, "help") == 0) {
        cmd_help();
    } else if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) {
        cmd_clear();
    } else if (strcmp(input, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(input, "info") == 0 || strcmp(input, "about") == 0) {
        cmd_info();
    } else if (strcmp(input, "fetch") == 0 || strcmp(input, "neofetch") == 0) {
        cmd_fetch();
    } else if (strcmp(input, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(input, "halt") == 0 || strcmp(input, "shutdown") == 0) {
        cmd_halt();
    } else if (strcmp(input, "color") == 0) {
        cmd_color(args);
    } else if (strcmp(input, "basic") == 0) {
        cmd_basic();
    } else if (strcmp(input, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(input, "sleep") == 0) {
        cmd_sleep(args);
    } else if (strcmp(input, "date") == 0) {
        cmd_date();
    } else if (strcmp(input, "setdate") == 0) {
        cmd_setdate(args);
    } else if (strcmp(input, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(input, "cd") == 0) {
        cmd_cd(args);
    } else if (strcmp(input, "ls") == 0) {
        cmd_ls(args);
    } else if (strcmp(input, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(input, "run") == 0) {
        cmd_run(args);
    } else if (strcmp(input, "ps") == 0) {
        cmd_ps();
    } else if (strcmp(input, "top") == 0) {
        cmd_top();
    } else if (strcmp(input, "kill") == 0) {
        cmd_kill(args);
    } else if (strcmp(input, "wait") == 0) {
        cmd_wait(args);
    } else if (strcmp(input, "mkdir") == 0) {
        cmd_mkdir(args);
    } else if (strcmp(input, "cp") == 0) {
        cmd_cp(args);
    } else if (strcmp(input, "mv") == 0) {
        cmd_mv(args);
    } else if (strcmp(input, "nano") == 0 || strcmp(input, "edit") == 0) {
        cmd_nano(args);
    } else {
        screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
        screen_print("Unknown command: ");
        screen_println(input);
        screen_set_color(VGA_WHITE, VGA_BLUE);
        screen_println("Type 'help' for available commands.");
    }
}

// Help command
static void cmd_help(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Available commands:");

    print_help_cmd("help", "Show this help message");
    print_help_cmd("clear, cls", "Clear the screen");
    print_help_cmd("echo <text>", "Print text to screen");
    print_help_cmd("info, about", "Show system information");
    print_help_cmd("fetch, neofetch", "Show neofetch-like banner");
    print_help_cmd("uptime", "Show system uptime");
    print_help_cmd("sleep <ms>", "Sleep for N milliseconds");
    print_help_cmd("date", "Show RTC date/time");
    print_help_cmd("setdate", "Set RTC date/time (YYYY-MM-DD HH:MM:SS)");
    print_help_cmd("pwd", "Print current directory");
    print_help_cmd("cd <dir>", "Change directory");
    print_help_cmd("ls [path]", "List directory contents");
    print_help_cmd("cat <file>", "Print a file");
    print_help_cmd("run <elf>", "Run a user-mode ELF (foreground)");
    print_help_cmd("mkdir <dir>", "Create directory (/ram or /disk)");
    print_help_cmd("cp <src> <dst>", "Copy file (/ram or /disk)");
    print_help_cmd("mv <src> <dst>", "Move/rename (within /ram or /disk)");
    print_help_cmd("nano <file>", "Edit a file (/ram or /disk)");
    print_help_cmd("ps", "List running tasks");
    print_help_cmd("top", "Live task view (press q)");
    print_help_cmd("kill <pid> [code]", "Kill a task");
    print_help_cmd("wait <pid>", "Wait for a task to exit");
    print_help_cmd("color <0-15>", "Change text color");
    print_help_cmd("basic", "Start BASIC interpreter");
    print_help_cmd("reboot", "Reboot the system");
    print_help_cmd("halt", "Halt the system");
}

// Clear screen command
static void cmd_clear(void) {
    screen_clear();
    statusbar_refresh();
}

// Echo command
static void cmd_echo(const char* args) {
    screen_println(args);
}

// System info command
static void cmd_info(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("=== VOS - Victor's Operating System ===");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Version: 0.1.0");
    screen_println("Architecture: i386 (x86 32-bit)");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Features:");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("  - VGA text mode display (80x25)");
    screen_println("  - PS/2 keyboard input");
    screen_println("  - PIT timer + uptime");
    screen_println("  - CMOS RTC date/time");
    screen_println("  - Simple command shell");
    screen_println("");
    screen_println("This is a minimal educational OS.");
}

static void cmd_fetch(void) {
    print_neofetch_like_banner();
}

// Reboot command
static void cmd_reboot(void) {
    screen_println("Rebooting...");

    // Try keyboard controller reset
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);

    // If that didn't work, halt
    hlt();
}

// Halt command
static void cmd_halt(void) {
    screen_println("System halted. You can safely power off.");
    cli();
    for (;;) {
        hlt();
    }
}

// Color command
static void cmd_color(const char* args) {
    if (*args == '\0') {
        screen_println("Usage: color <0-15>");
        screen_println("Colors: 0=Black, 1=Blue, 2=Green, 3=Cyan,");
        screen_println("        4=Red, 5=Magenta, 6=Brown, 7=LightGrey,");
        screen_println("        8=DarkGrey, 9=LightBlue, 10=LightGreen,");
        screen_println("        11=LightCyan, 12=LightRed, 13=LightMagenta,");
        screen_println("        14=Yellow, 15=White");
        return;
    }

    // Parse number
    int color = 0;
    while (*args >= '0' && *args <= '9') {
        color = color * 10 + (*args - '0');
        args++;
    }

    if (color >= 0 && color <= 15) {
        screen_set_color(color, VGA_BLUE);
        screen_println("Color changed.");
    } else {
        screen_println("Invalid color. Use 0-15.");
    }
}

static void cmd_uptime(void) {
    uint32_t uptime_ms = timer_uptime_ms();
    uint32_t seconds = uptime_ms / 1000u;
    uint32_t ms = uptime_ms % 1000u;

    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("Uptime: ");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print_dec((int32_t)seconds);
    screen_print(".");
    if (ms < 100) screen_putchar('0');
    if (ms < 10) screen_putchar('0');
    screen_print_dec((int32_t)ms);
    screen_println("s");
}

static void cmd_sleep(const char* args) {
    if (*args == '\0') {
        screen_println("Usage: sleep <ms>");
        return;
    }

    int ms = atoi(args);
    if (ms <= 0) {
        screen_println("Usage: sleep <ms>");
        return;
    }

    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"((uint32_t)SYS_SLEEP), "b"((uint32_t)ms) : "memory");
    (void)ret;
}

static void print_2d(uint8_t v) {
    if (v < 10) screen_putchar('0');
    screen_print_dec((int32_t)v);
}

static void cmd_date(void) {
    rtc_datetime_t dt;
    if (!rtc_read_datetime(&dt)) {
        screen_println("RTC read failed.");
        return;
    }

    screen_print_dec((int32_t)dt.year);
    screen_putchar('-');
    print_2d(dt.month);
    screen_putchar('-');
    print_2d(dt.day);
    screen_putchar(' ');
    print_2d(dt.hour);
    screen_putchar(':');
    print_2d(dt.minute);
    screen_putchar(':');
    print_2d(dt.second);
    screen_putchar('\n');
}

static void skip_spaces(const char** p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static bool parse_n_digits(const char** p, int n, int* out) {
    int value = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    *p += n;
    *out = value;
    return true;
}

static void cmd_setdate(const char* args) {
    const char* p = args;
    skip_spaces(&p);

    int year, month, day, hour, minute, second;
    if (!parse_n_digits(&p, 4, &year) || *p++ != '-' ||
        !parse_n_digits(&p, 2, &month) || *p++ != '-' ||
        !parse_n_digits(&p, 2, &day) || (*p != ' ' && *p != 'T')) {
        screen_println("Usage: setdate <YYYY-MM-DD HH:MM:SS>");
        return;
    }
    p++;
    if (!parse_n_digits(&p, 2, &hour) || *p++ != ':' ||
        !parse_n_digits(&p, 2, &minute) || *p++ != ':' ||
        !parse_n_digits(&p, 2, &second)) {
        screen_println("Usage: setdate <YYYY-MM-DD HH:MM:SS>");
        return;
    }

    rtc_datetime_t dt;
    dt.year = (uint16_t)year;
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.hour = (uint8_t)hour;
    dt.minute = (uint8_t)minute;
    dt.second = (uint8_t)second;

    if (!rtc_set_datetime(&dt)) {
        screen_println("RTC set failed (invalid time or unsupported year).");
        return;
    }

    screen_println("RTC updated.");
    statusbar_refresh();
}

static void cmd_pwd(void) {
    screen_println(shell_cwd);
}

static void cmd_cd(const char* args) {
    const char* target = args;
    if (!target || target[0] == '\0') {
        target = "/";
    }

    char path[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, target, path, sizeof(path))) {
        screen_println("Invalid path.");
        return;
    }

    if (!vfs_dir_exists(path)) {
        screen_println("No such directory.");
        return;
    }

    strncpy(shell_cwd, path, sizeof(shell_cwd) - 1u);
    shell_cwd[sizeof(shell_cwd) - 1u] = '\0';
}

typedef struct {
    char name[64];
    bool is_dir;
    uint32_t size;
} ls_entry_t;

static int ls_find_entry(ls_entry_t* entries, int n, const char* name) {
    for (int i = 0; i < n; i++) {
        if (ci_eq(entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static void cmd_ls(const char* args) {
    if (!vfs_is_ready()) {
        screen_println("VFS not ready.");
        return;
    }

    char path[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, args && args[0] ? args : ".", path, sizeof(path))) {
        screen_println("Invalid path.");
        return;
    }

    if (is_disk_path_abs(path)) {
        if (!fatdisk_is_ready()) {
            screen_println("disk: not mounted");
            return;
        }

        bool is_dir = false;
        uint32_t size = 0;
        if (!fatdisk_stat(path, &is_dir, &size)) {
            screen_println("No such file or directory.");
            return;
        }

        if (!is_dir) {
            screen_set_color(VGA_YELLOW, VGA_BLUE);
            screen_print_dec((int32_t)size);
            screen_set_color(VGA_WHITE, VGA_BLUE);
            screen_print("  ");
            screen_println(path);
            return;
        }

        fatdisk_dirent_t dents[LS_MAX_ENTRIES];
        uint32_t n = fatdisk_list_dir(path, dents, (uint32_t)LS_MAX_ENTRIES);
        for (uint32_t i = 0; i < n; i++) {
            if (dents[i].is_dir) {
                screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
                screen_print(dents[i].name);
                screen_println("/");
            } else {
                screen_set_color(VGA_YELLOW, VGA_BLUE);
                screen_print_dec((int32_t)dents[i].size);
                screen_set_color(VGA_WHITE, VGA_BLUE);
                screen_print("  ");
                screen_println(dents[i].name);
            }
        }
        screen_set_color(VGA_WHITE, VGA_BLUE);
        return;
    }

    if (vfs_file_exists(path)) {
        const uint8_t* data = NULL;
        uint32_t size = 0;
        vfs_read_file(path, &data, &size);
        screen_set_color(VGA_YELLOW, VGA_BLUE);
        screen_print_dec((int32_t)size);
        screen_set_color(VGA_WHITE, VGA_BLUE);
        screen_print("  ");
        screen_println(path);
        return;
    }

    if (!vfs_dir_exists(path)) {
        screen_println("No such directory.");
        return;
    }

    const char* dir_rel = skip_slashes(path);
    uint32_t dir_len = (uint32_t)strlen(dir_rel);
    bool is_root = (dir_len == 0);

    ls_entry_t entries[LS_MAX_ENTRIES];
    int entry_count = 0;
    memset(entries, 0, sizeof(entries));

    uint32_t count = vfs_file_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* full = vfs_file_name(i);
        if (!full) {
            continue;
        }
        const char* n = skip_slashes(full);

        const char* rem = NULL;
        if (is_root) {
            rem = n;
        } else {
            if (!ci_starts_with(n, dir_rel) || n[dir_len] != '/') {
                continue;
            }
            rem = n + dir_len + 1u;
        }

        if (rem[0] == '\0') {
            continue;
        }

        char seg[64];
        uint32_t seg_len = 0;
        while (rem[seg_len] && rem[seg_len] != '/' && seg_len + 1u < sizeof(seg)) {
            seg[seg_len] = rem[seg_len];
            seg_len++;
        }
        seg[seg_len] = '\0';
        if (seg[0] == '\0') {
            continue;
        }

        bool is_dir = rem[seg_len] == '/';
        uint32_t size = is_dir ? 0u : vfs_file_size(i);

        int idx = ls_find_entry(entries, entry_count, seg);
        if (idx >= 0) {
            entries[idx].is_dir = entries[idx].is_dir || is_dir;
            continue;
        }
        if (entry_count >= LS_MAX_ENTRIES) {
            continue;
        }
        strncpy(entries[entry_count].name, seg, sizeof(entries[entry_count].name) - 1u);
        entries[entry_count].name[sizeof(entries[entry_count].name) - 1u] = '\0';
        entries[entry_count].is_dir = is_dir;
        entries[entry_count].size = size;
        entry_count++;
    }

    if (is_root) {
        // Mount point for writable storage.
        int idx = ls_find_entry(entries, entry_count, "ram");
        if (idx < 0 && entry_count < LS_MAX_ENTRIES) {
            strncpy(entries[entry_count].name, "ram", sizeof(entries[entry_count].name) - 1u);
            entries[entry_count].name[sizeof(entries[entry_count].name) - 1u] = '\0';
            entries[entry_count].is_dir = true;
            entries[entry_count].size = 0;
            entry_count++;
        } else if (idx >= 0) {
            entries[idx].is_dir = true;
        }

        if (fatdisk_is_ready()) {
            idx = ls_find_entry(entries, entry_count, "disk");
            if (idx < 0 && entry_count < LS_MAX_ENTRIES) {
                strncpy(entries[entry_count].name, "disk", sizeof(entries[entry_count].name) - 1u);
                entries[entry_count].name[sizeof(entries[entry_count].name) - 1u] = '\0';
                entries[entry_count].is_dir = true;
                entries[entry_count].size = 0;
                entry_count++;
            } else if (idx >= 0) {
                entries[idx].is_dir = true;
            }
        }
    }

    if (ramfs_is_dir(path)) {
        ramfs_dirent_t rents[LS_MAX_ENTRIES];
        uint32_t n = ramfs_list_dir(path, rents, (uint32_t)LS_MAX_ENTRIES);
        for (uint32_t i = 0; i < n; i++) {
            int idx = ls_find_entry(entries, entry_count, rents[i].name);
            if (idx >= 0) {
                entries[idx].is_dir = entries[idx].is_dir || rents[i].is_dir;
                continue;
            }
            if (entry_count >= LS_MAX_ENTRIES) {
                break;
            }
            strncpy(entries[entry_count].name, rents[i].name, sizeof(entries[entry_count].name) - 1u);
            entries[entry_count].name[sizeof(entries[entry_count].name) - 1u] = '\0';
            entries[entry_count].is_dir = rents[i].is_dir;
            entries[entry_count].size = rents[i].size;
            entry_count++;
        }
    }

    // Print directories first, then files.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < entry_count; i++) {
            if ((pass == 0) != entries[i].is_dir) {
                continue;
            }
            if (entries[i].is_dir) {
                screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
                screen_print(entries[i].name);
                screen_println("/");
            } else {
                screen_set_color(VGA_YELLOW, VGA_BLUE);
                screen_print_dec((int32_t)entries[i].size);
                screen_set_color(VGA_WHITE, VGA_BLUE);
                screen_print("  ");
                screen_println(entries[i].name);
            }
        }
    }
    screen_set_color(VGA_WHITE, VGA_BLUE);
}

static void cmd_mkdir(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: mkdir <dir>");
        return;
    }

    char path[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, args, path, sizeof(path))) {
        screen_println("Invalid path.");
        return;
    }

    if (is_ram_path_abs(path)) {
        if (!ramfs_mkdir(path)) {
            screen_println("mkdir failed.");
        }
        return;
    }

    if (is_disk_path_abs(path)) {
        if (!fatdisk_mkdir(path)) {
            screen_println("mkdir failed.");
        }
        return;
    }

    screen_println("mkdir: only supported under /ram or /disk");
}

static void cmd_cp(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: cp <src> <dst>");
        return;
    }

    char* argv[3] = {0};
    int argc = split_args_inplace((char*)args, argv, 3);
    if (argc != 2) {
        screen_println("Usage: cp <src> <dst>");
        return;
    }

    char src[SHELL_PATH_MAX];
    char dst[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, argv[0], src, sizeof(src)) || !resolve_path(shell_cwd, argv[1], dst, sizeof(dst))) {
        screen_println("Invalid path.");
        return;
    }

    bool dst_dir_hint = false;
    uint32_t dst_token_len = (uint32_t)strlen(argv[1]);
    if (dst_token_len && argv[1][dst_token_len - 1u] == '/') {
        dst_dir_hint = true;
    }

    char dst_file[SHELL_PATH_MAX];
    if (dst_dir_hint || vfs_dir_exists(dst)) {
        const char* base = path_basename(src);
        if (!path_join(dst_file, sizeof(dst_file), dst, base)) {
            screen_println("Destination too long.");
            return;
        }
    } else {
        strncpy(dst_file, dst, sizeof(dst_file) - 1u);
        dst_file[sizeof(dst_file) - 1u] = '\0';
    }

    bool dst_ram = is_ram_path_abs(dst_file);
    bool dst_disk = is_disk_path_abs(dst_file);
    if (!dst_ram && !dst_disk) {
        screen_println("cp: destination must be under /ram or /disk");
        return;
    }

    uint8_t* disk_buf = NULL;
    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (is_disk_path_abs(src)) {
        if (!fatdisk_read_file_alloc(src, &disk_buf, &size) || !disk_buf) {
            screen_println("cp: source not found.");
            return;
        }
        data = disk_buf;
    } else {
        if (!vfs_read_file(src, &data, &size) || !data) {
            screen_println("cp: source not found.");
            return;
        }
    }

    bool ok = false;
    if (dst_ram) {
        ok = ramfs_write_file(dst_file, data, size, false);
    } else {
        ok = fatdisk_write_file(dst_file, data, size, false);
    }

    if (disk_buf) {
        kfree(disk_buf);
    }

    if (!ok) {
        screen_println("cp failed (exists? out of space?).");
        return;
    }
}

static void cmd_mv(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: mv <src> <dst>");
        return;
    }

    char* argv[3] = {0};
    int argc = split_args_inplace((char*)args, argv, 3);
    if (argc != 2) {
        screen_println("Usage: mv <src> <dst>");
        return;
    }

    char src[SHELL_PATH_MAX];
    char dst[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, argv[0], src, sizeof(src)) || !resolve_path(shell_cwd, argv[1], dst, sizeof(dst))) {
        screen_println("Invalid path.");
        return;
    }

    bool src_ram = is_ram_path_abs(src);
    bool src_disk = is_disk_path_abs(src);
    if (!src_ram && !src_disk) {
        screen_println("mv: only supported under /ram or /disk");
        return;
    }

    bool dst_dir_hint = false;
    uint32_t dst_token_len = (uint32_t)strlen(argv[1]);
    if (dst_token_len && argv[1][dst_token_len - 1u] == '/') {
        dst_dir_hint = true;
    }

    char dst_file[SHELL_PATH_MAX];
    if (dst_dir_hint || vfs_dir_exists(dst)) {
        const char* base = path_basename(src);
        if (!path_join(dst_file, sizeof(dst_file), dst, base)) {
            screen_println("Destination too long.");
            return;
        }
    } else {
        strncpy(dst_file, dst, sizeof(dst_file) - 1u);
        dst_file[sizeof(dst_file) - 1u] = '\0';
    }

    bool dst_ram = is_ram_path_abs(dst_file);
    bool dst_disk = is_disk_path_abs(dst_file);
    if (!dst_ram && !dst_disk) {
        screen_println("mv: destination must be under /ram or /disk");
        return;
    }
    if ((src_ram && !dst_ram) || (src_disk && !dst_disk)) {
        screen_println("mv: cross-filesystem move not supported (use cp)");
        return;
    }

    bool ok = false;
    if (src_ram) {
        if (!ramfs_is_file(src)) {
            screen_println("mv: source not found.");
            return;
        }
        ok = ramfs_rename(src, dst_file);
    } else {
        bool is_dir = false;
        uint32_t sz = 0;
        if (!fatdisk_stat(src, &is_dir, &sz) || is_dir) {
            screen_println("mv: source not found.");
            return;
        }
        ok = fatdisk_rename(src, dst_file);
    }

    if (!ok) {
        screen_println("mv failed.");
        return;
    }
}

static void cmd_nano(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: nano <file>");
        return;
    }

    char src[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, args, src, sizeof(src))) {
        screen_println("Invalid path.");
        return;
    }

    // If the user asked to edit a directory, refuse.
    if (!vfs_file_exists(src) && vfs_dir_exists(src)) {
        screen_println("nano: is a directory");
        return;
    }

    if (is_disk_path_abs(src)) {
        const char* base = path_basename(src);
        if (!base || base[0] == '\0') {
            base = "untitled.txt";
        }

        char tmp_name[96];
        strcpy(tmp_name, "__diskedit_");
        strncat(tmp_name, base, sizeof(tmp_name) - strlen(tmp_name) - 1u);

        char tmp_path[SHELL_PATH_MAX];
        if (!path_join(tmp_path, sizeof(tmp_path), "/ram", tmp_name)) {
            screen_println("nano: temp path too long");
            return;
        }

        uint8_t* disk_data = NULL;
        uint32_t disk_size = 0;
        if (fatdisk_is_file(src)) {
            if (!fatdisk_read_file_alloc(src, &disk_data, &disk_size) || !disk_data) {
                screen_println("nano: read failed");
                return;
            }
        }

        (void)ramfs_write_file(tmp_path, disk_data, disk_size, true);
        if (disk_data) {
            kfree(disk_data);
        }

        bool saved = editor_nano(tmp_path);
        if (saved) {
            const uint8_t* buf = NULL;
            uint32_t sz = 0;
            if (ramfs_read_file(tmp_path, &buf, &sz) && buf) {
                if (!fatdisk_write_file(src, buf, sz, true)) {
                    screen_println("nano: write to /disk failed");
                }
            }
        }
    } else {
        char dst[SHELL_PATH_MAX];
        if (is_ram_path_abs(src)) {
            strncpy(dst, src, sizeof(dst) - 1u);
            dst[sizeof(dst) - 1u] = '\0';
        } else {
            const char* base = path_basename(src);
            if (!base || base[0] == '\0') {
                base = "untitled.txt";
            }
            if (!path_join(dst, sizeof(dst), "/ram", base)) {
                screen_println("nano: destination too long");
                return;
            }

            // If the source exists and destination doesn't, seed it.
            if (!ramfs_is_file(dst)) {
                const uint8_t* data = NULL;
                uint32_t size = 0;
                if (vfs_read_file(src, &data, &size) && data) {
                    (void)ramfs_write_file(dst, data, size, false);
                } else {
                    (void)ramfs_write_file(dst, NULL, 0, false);
                }
            }
        }

        if (ramfs_is_dir(dst) && !ramfs_is_file(dst)) {
            screen_println("nano: is a directory");
            return;
        }

        (void)editor_nano(dst);
    }

    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_clear();
    statusbar_refresh();
}

static void cmd_cat(const char* args) {
    if (!vfs_is_ready()) {
        screen_println("VFS not ready.");
        return;
    }
    if (!args || args[0] == '\0') {
        screen_println("Usage: cat <file>");
        return;
    }

    char path[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, args, path, sizeof(path))) {
        screen_println("Invalid path.");
        return;
    }

    if (is_disk_path_abs(path)) {
        if (!fatdisk_is_ready()) {
            screen_println("disk: not mounted");
            return;
        }

        uint8_t* buf = NULL;
        uint32_t sz = 0;
        if (!fatdisk_read_file_alloc(path, &buf, &sz) || !buf) {
            screen_println("File not found.");
            return;
        }

        uint32_t max = sz;
        if (max > 4096u) {
            max = 4096u;
        }
        for (uint32_t i = 0; i < max; i++) {
            screen_putchar((char)buf[i]);
        }
        if (max != 0 && buf[max - 1] != '\n') {
            screen_putchar('\n');
        }
        if (sz > max) {
            screen_println("[...truncated...]");
        }
        kfree(buf);
        return;
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(path, &data, &size) || !data) {
        screen_println("File not found.");
        return;
    }

    uint32_t max = size;
    if (max > 4096u) {
        max = 4096u;
    }
    for (uint32_t i = 0; i < max; i++) {
        screen_putchar((char)data[i]);
    }
    if (max != 0 && data[max - 1] != '\n') {
        screen_putchar('\n');
    }
    if (size > max) {
        screen_println("[...truncated...]");
    }
}

static void cmd_run(const char* args) {
    if (!vfs_is_ready()) {
        screen_println("VFS not ready.");
        return;
    }
    if (!args || args[0] == '\0') {
        screen_println("Usage: run <file>");
        return;
    }

    char path[SHELL_PATH_MAX];
    if (!resolve_path(shell_cwd, args, path, sizeof(path))) {
        screen_println("Invalid path.");
        return;
    }

    uint8_t* disk_buf = NULL;
    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (is_disk_path_abs(path)) {
        if (!fatdisk_is_ready()) {
            screen_println("disk: not mounted");
            return;
        }
        if (!fatdisk_read_file_alloc(path, &disk_buf, &size) || !disk_buf || size == 0) {
            screen_println("File not found.");
            return;
        }
        data = disk_buf;
    } else {
        if (!vfs_read_file(path, &data, &size) || !data || size == 0) {
            screen_println("File not found.");
            return;
        }
    }

    uint32_t entry = 0;
    uint32_t user_esp = 0;
    uint32_t brk = 0;
    uint32_t* user_dir = paging_create_user_directory();
    if (!user_dir) {
        screen_println("Out of memory (page directory).");
        return;
    }

    uint32_t flags = irq_save();
    paging_switch_directory(user_dir);
    bool ok = elf_load_user_image(data, size, &entry, &user_esp, &brk);
    paging_switch_directory(paging_kernel_directory());
    irq_restore(flags);
    if (!ok) {
        if (disk_buf) kfree(disk_buf);
        screen_println("ELF load failed.");
        return;
    }

    uint32_t pid = tasking_spawn_user_pid(entry, user_esp, user_dir, brk);
    if (pid == 0) {
        if (disk_buf) kfree(disk_buf);
        screen_println("Failed to spawn task.");
        return;
    }

    // Foreground: wait for exit so output/input doesn't race the shell.
    int exit_code = 0;
    __asm__ volatile ("int $0x80" : "=a"(exit_code) : "a"((uint32_t)SYS_WAIT), "b"(pid) : "memory");
    if (disk_buf) kfree(disk_buf);
    screen_print("Program exited with code ");
    screen_print_dec(exit_code);
    screen_putchar('\n');
}

static const char* task_state_str(task_state_t state) {
    switch (state) {
        case TASK_STATE_RUNNABLE: return "RUN";
        case TASK_STATE_SLEEPING: return "SLEEP";
        case TASK_STATE_WAITING:  return "WAIT";
        case TASK_STATE_ZOMBIE:   return "ZOMB";
        default:                  return "?";
    }
}

static void cmd_ps(void) {
    uint32_t count = tasking_task_count();
    uint32_t cur = tasking_current_pid();

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_print("PID   USER  STATE  TICKS    EIP       NAME");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_putchar('\n');

    for (uint32_t i = 0; i < count; i++) {
        task_info_t info;
        if (!tasking_get_task_info(i, &info)) {
            continue;
        }

        if (info.pid == cur) {
            screen_set_color(VGA_YELLOW, VGA_BLUE);
        } else {
            screen_set_color(VGA_WHITE, VGA_BLUE);
        }

        screen_print_dec((int32_t)info.pid);
        screen_print((info.pid < 10) ? "     " : (info.pid < 100) ? "    " : (info.pid < 1000) ? "   " : "  ");

        screen_print(info.user ? "user  " : "kern  ");
        screen_print(task_state_str(info.state));
        screen_print((strlen(task_state_str(info.state)) < 5) ? "   " : "  ");

        screen_print_dec((int32_t)info.cpu_ticks);
        screen_print("  ");

        screen_print_hex(info.eip);
        screen_print("  ");

        screen_println(info.name);
    }

    screen_set_color(VGA_WHITE, VGA_BLUE);
}

static void cmd_top(void) {
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("top: press 'q' to quit");

    for (;;) {
        // Exit if user typed q.
        if (keyboard_has_key()) {
            char c = keyboard_getchar();
            if (c == 'q' || c == 'Q') {
                return;
            }
        }

        screen_clear();
        statusbar_refresh();

        cmd_ps();

        // Sleep ~1s in small chunks so 'q' feels responsive.
        for (int i = 0; i < 10; i++) {
            if (keyboard_has_key()) {
                char c = keyboard_getchar();
                if (c == 'q' || c == 'Q') {
                    return;
                }
            }
            int ret;
            __asm__ volatile ("int $0x80" : "=a"(ret) : "a"((uint32_t)SYS_SLEEP), "b"(100u) : "memory");
            (void)ret;
        }
    }
}

static void cmd_kill(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: kill <pid> [code]");
        return;
    }

    int pid = atoi(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    int code = 0;
    if (*args) {
        code = atoi(args);
    }

    bool ok = tasking_kill((uint32_t)pid, (int32_t)code);
    screen_println(ok ? "OK" : "Failed");
}

static void cmd_wait(const char* args) {
    if (!args || args[0] == '\0') {
        screen_println("Usage: wait <pid>");
        return;
    }

    int pid = atoi(args);
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"((uint32_t)SYS_WAIT), "b"((uint32_t)pid) : "memory");

    screen_print("exit_code=");
    screen_print_dec((int32_t)ret);
    screen_putchar('\n');
}

// BASIC interpreter command
static char basic_program[BASIC_PROGRAM_SIZE];

// Show list of demo programs
static void basic_show_demos(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("=== Available Demo Programs ===");
    screen_set_color(VGA_WHITE, VGA_BLUE);

    for (int i = 1; i <= BASIC_NUM_PROGRAMS; i++) {
        screen_set_color(VGA_YELLOW, VGA_BLUE);
        screen_print_dec(i);
        screen_set_color(VGA_WHITE, VGA_BLUE);
        screen_print(". ");
        screen_print(basic_get_program_name(i));
        screen_print(" - ");
        screen_println(basic_get_program_description(i));
    }
    screen_println("");
    screen_println("Use LOAD <number> to load a program.");
}

// Load a demo program by number
static int basic_load_demo(int num, int* program_pos) {
    const char* prog = basic_get_program(num);
    if (prog == 0) {
        screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
        screen_println("Invalid program number. Use 1-10.");
        screen_set_color(VGA_WHITE, VGA_BLUE);
        return 0;
    }

    // Copy program
    memset(basic_program, 0, BASIC_PROGRAM_SIZE);
    int len = strlen(prog);
    if (len >= BASIC_PROGRAM_SIZE) {
        len = BASIC_PROGRAM_SIZE - 1;
    }
    memcpy(basic_program, prog, len);
    basic_program[len] = '\0';
    *program_pos = len;

    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("Loaded: ");
    screen_println(basic_get_program_name(num));
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("  ");
    screen_println(basic_get_program_description(num));
    screen_println("Type LIST to view, RUN to execute.");
    return 1;
}

static void cmd_basic(void) {
    char line_buffer[MAX_COMMAND_LENGTH];
    int program_pos = 0;

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("=== uBASIC Interpreter ===");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Commands:");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("  RUN        - Execute the program");
    screen_println("  LIST       - Show current program");
    screen_println("  NEW        - Clear program");
    screen_println("  DEMOS      - Show example programs");
    screen_println("  LOAD <1-10> - Load an example program");
    screen_println("  EXIT       - Return to shell");
    screen_println("");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Tip: Type DEMOS to see 10 example programs!");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("");

    // Clear program buffer
    memset(basic_program, 0, BASIC_PROGRAM_SIZE);
    program_pos = 0;

    while (1) {
        screen_set_color(VGA_YELLOW, VGA_BLUE);
        screen_print("BASIC> ");
        screen_set_color(VGA_WHITE, VGA_BLUE);
        keyboard_getline(line_buffer, MAX_COMMAND_LENGTH);

        // Check for commands (case insensitive)
        if (strcmp(line_buffer, "EXIT") == 0 || strcmp(line_buffer, "exit") == 0) {
            screen_println("Returning to shell...");
            return;
        } else if (strcmp(line_buffer, "RUN") == 0 || strcmp(line_buffer, "run") == 0) {
            if (program_pos == 0) {
                screen_println("No program to run. Use DEMOS to see examples.");
            } else {
                screen_println("--- Running program ---");
                screen_set_color(VGA_WHITE, VGA_BLUE);
                ubasic_init(basic_program);
                while (!ubasic_finished()) {
                    ubasic_run();
                }
                screen_set_color(VGA_WHITE, VGA_BLUE);
                screen_println("--- Program ended ---");
            }
        } else if (strcmp(line_buffer, "LIST") == 0 || strcmp(line_buffer, "list") == 0) {
            if (program_pos == 0) {
                screen_println("No program loaded. Use DEMOS to see examples.");
            } else {
                screen_set_color(VGA_WHITE, VGA_BLUE);
                screen_println(basic_program);
                screen_set_color(VGA_WHITE, VGA_BLUE);
            }
        } else if (strcmp(line_buffer, "NEW") == 0 || strcmp(line_buffer, "new") == 0) {
            memset(basic_program, 0, BASIC_PROGRAM_SIZE);
            program_pos = 0;
            screen_println("Program cleared.");
        } else if (strcmp(line_buffer, "DEMOS") == 0 || strcmp(line_buffer, "demos") == 0) {
            basic_show_demos();
        } else if (strncmp(line_buffer, "LOAD ", 5) == 0 || strncmp(line_buffer, "load ", 5) == 0) {
            // Parse number after LOAD
            int num = 0;
            const char* p = line_buffer + 5;
            while (*p == ' ') p++;
            while (*p >= '0' && *p <= '9') {
                num = num * 10 + (*p - '0');
                p++;
            }
            if (num >= 1 && num <= 10) {
                basic_load_demo(num, &program_pos);
            } else {
                screen_println("Usage: LOAD <1-10>");
            }
        } else if (line_buffer[0] != '\0') {
            // Add line to program
            int line_len = strlen(line_buffer);
            if (program_pos + line_len + 2 < BASIC_PROGRAM_SIZE) {
                strcpy(basic_program + program_pos, line_buffer);
                program_pos += line_len;
                basic_program[program_pos++] = '\n';
                basic_program[program_pos] = '\0';
            } else {
                screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
                screen_println("Program too large!");
                screen_set_color(VGA_WHITE, VGA_BLUE);
            }
        }
    }
}

void shell_run(void) {
    statusbar_init();
    keyboard_set_idle_hook(shell_idle_hook);

    screen_set_color(VGA_WHITE, VGA_BLUE);
    print_neofetch_like_banner();

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Welcome to VOS Shell!");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Type 'help' for available commands.\n");

    microrl_init(&shell_rl, shell_rl_print);
    microrl_set_execute_callback(&shell_rl, shell_rl_execute);
    shell_update_prompt();
    screen_set_color(VGA_WHITE, VGA_BLUE);
    microrl_print_prompt(&shell_rl);

    while (1) {
        char c = keyboard_getchar();
        int8_t key = (int8_t)c;

        int page = screen_rows() - 1;
        if (page < 1) page = 1;

        if (screen_scrollback_active() && key != KEY_PGUP && key != KEY_PGDN) {
            screen_scrollback_reset();
        }

        if (key == KEY_PGUP) {
            screen_scrollback_lines(page);
            continue;
        }
        if (key == KEY_PGDN) {
            screen_scrollback_lines(-page);
            continue;
        }

        if (key == KEY_UP) {
            microrl_feed_arrow(&shell_rl, 'A');
        } else if (key == KEY_DOWN) {
            microrl_feed_arrow(&shell_rl, 'B');
        } else if (key == KEY_RIGHT) {
            microrl_feed_arrow(&shell_rl, 'C');
        } else if (key == KEY_LEFT) {
            microrl_feed_arrow(&shell_rl, 'D');
        } else if (key == KEY_HOME) {
            microrl_feed_home_end(&shell_rl, true);
        } else if (key == KEY_END) {
            microrl_feed_home_end(&shell_rl, false);
        } else {
            microrl_insert_char(&shell_rl, (unsigned char)c);
        }
    }
}
