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
#include "ubasic.h"
#include "basic_programs.h"
#include "stdlib.h"

#define MAX_COMMAND_LENGTH 256
#define BASIC_PROGRAM_SIZE 4096
#define VOS_VERSION "0.1.0"

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
static void cmd_reboot(void);
static void cmd_halt(void);
static void cmd_color(const char* args);
static void cmd_basic(void);
static void cmd_uptime(void);
static void cmd_sleep(const char* args);
static void cmd_date(void);
static void cmd_setdate(const char* args);
static void cmd_ls(void);
static void cmd_cat(const char* args);
static void cmd_run(const char* args);
static void cmd_ps(void);
static void cmd_top(void);
static void cmd_kill(const char* args);
static void cmd_wait(const char* args);

static void print_spaces(int count) {
    for (int i = 0; i < count; i++) {
        screen_putchar(' ');
    }
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

// Print the shell prompt
static void print_prompt(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_print("vos");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_print("> ");
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
    } else if (strcmp(input, "ls") == 0) {
        cmd_ls();
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
    print_help_cmd("uptime", "Show system uptime");
    print_help_cmd("sleep <ms>", "Sleep for N milliseconds");
    print_help_cmd("date", "Show RTC date/time");
    print_help_cmd("setdate", "Set RTC date/time (YYYY-MM-DD HH:MM:SS)");
    print_help_cmd("ls", "List initramfs files");
    print_help_cmd("cat <file>", "Print a file from initramfs");
    print_help_cmd("run <elf>", "Run a user-mode ELF from initramfs");
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

static void cmd_ls(void) {
    if (!vfs_is_ready()) {
        screen_println("VFS not ready.");
        return;
    }

    uint32_t count = vfs_file_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* name = vfs_file_name(i);
        if (name) {
            uint32_t size = vfs_file_size(i);
            screen_set_color(VGA_YELLOW, VGA_BLUE);
            screen_print_dec((int32_t)size);
            screen_set_color(VGA_WHITE, VGA_BLUE);
            screen_print("  ");
            screen_println(name);
        }
    }
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

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(args, &data, &size) || !data) {
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

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(args, &data, &size) || !data || size == 0) {
        screen_println("File not found.");
        return;
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
        screen_println("ELF load failed.");
        return;
    }

    if (!tasking_spawn_user(entry, user_esp, user_dir, brk)) {
        screen_println("Failed to spawn task.");
        return;
    }

    screen_println("Spawned user task.");

    // Give the new task a chance to run immediately.
    __asm__ volatile ("int $0x80" : : "a"(2u) : "memory");
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
    char command_buffer[MAX_COMMAND_LENGTH];

    statusbar_init();
    keyboard_set_idle_hook(shell_idle_hook);

    screen_set_color(VGA_WHITE, VGA_BLUE);
    print_neofetch_like_banner();

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLUE);
    screen_println("Welcome to VOS Shell!");
    screen_set_color(VGA_WHITE, VGA_BLUE);
    screen_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();
        keyboard_getline(command_buffer, MAX_COMMAND_LENGTH);
        execute_command(command_buffer);
    }
}
