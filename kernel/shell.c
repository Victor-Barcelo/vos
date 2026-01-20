#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"
#include "timer.h"
#include "rtc.h"
#include "ubasic.h"
#include "basic_programs.h"
#include "stdlib.h"

#define MAX_COMMAND_LENGTH 256
#define BASIC_PROGRAM_SIZE 4096

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

// Print the shell prompt
static void print_prompt(void) {
    screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    screen_print("vos");
    screen_set_color(VGA_WHITE, VGA_BLACK);
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
    } else {
        screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
        screen_print("Unknown command: ");
        screen_println(input);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_println("Type 'help' for available commands.");
    }
}

// Help command
static void cmd_help(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("Available commands:");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("  help          - Show this help message");
    screen_println("  clear, cls    - Clear the screen");
    screen_println("  echo <text>   - Print text to screen");
    screen_println("  info, about   - Show system information");
    screen_println("  uptime        - Show system uptime");
    screen_println("  sleep <ms>    - Sleep for N milliseconds");
    screen_println("  date          - Show RTC date/time");
    screen_println("  setdate <YYYY-MM-DD HH:MM:SS> - Set RTC date/time");
    screen_println("  color <0-15>  - Change text color");
    screen_println("  basic         - Start BASIC interpreter");
    screen_println("  reboot        - Reboot the system");
    screen_println("  halt          - Halt the system");
}

// Clear screen command
static void cmd_clear(void) {
    screen_clear();
}

// Echo command
static void cmd_echo(const char* args) {
    screen_println(args);
}

// System info command
static void cmd_info(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("=== VOS - Victor's Operating System ===");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("Version: 0.1.0");
    screen_println("Architecture: i386 (x86 32-bit)");
    screen_println("Features:");
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
        screen_set_color(color, VGA_BLACK);
        screen_println("Color changed.");
    } else {
        screen_println("Invalid color. Use 0-15.");
    }
}

static void cmd_uptime(void) {
    uint32_t uptime_ms = timer_uptime_ms();
    uint32_t seconds = uptime_ms / 1000u;
    uint32_t ms = uptime_ms % 1000u;

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_print("Uptime: ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
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

    timer_sleep_ms((uint32_t)ms);
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
}

// BASIC interpreter command
static char basic_program[BASIC_PROGRAM_SIZE];

// Show list of demo programs
static void basic_show_demos(void) {
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("=== Available Demo Programs ===");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    for (int i = 1; i <= BASIC_NUM_PROGRAMS; i++) {
        screen_set_color(VGA_YELLOW, VGA_BLACK);
        screen_print_dec(i);
        screen_print(". ");
        screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        screen_print(basic_get_program_name(i));
        screen_set_color(VGA_WHITE, VGA_BLACK);
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
        screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
        screen_println("Invalid program number. Use 1-10.");
        screen_set_color(VGA_WHITE, VGA_BLACK);
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

    screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    screen_print("Loaded: ");
    screen_println(basic_get_program_name(num));
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("  ");
    screen_println(basic_get_program_description(num));
    screen_println("Type LIST to view, RUN to execute.");
    return 1;
}

static void cmd_basic(void) {
    char line_buffer[MAX_COMMAND_LENGTH];
    int program_pos = 0;

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("=== uBASIC Interpreter ===");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("Commands:");
    screen_println("  RUN        - Execute the program");
    screen_println("  LIST       - Show current program");
    screen_println("  NEW        - Clear program");
    screen_println("  DEMOS      - Show example programs");
    screen_println("  LOAD <1-10> - Load an example program");
    screen_println("  EXIT       - Return to shell");
    screen_println("");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_println("Tip: Type DEMOS to see 10 example programs!");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("");

    // Clear program buffer
    memset(basic_program, 0, BASIC_PROGRAM_SIZE);
    program_pos = 0;

    while (1) {
        screen_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        screen_print("BASIC> ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
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
                screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
                ubasic_init(basic_program);
                while (!ubasic_finished()) {
                    ubasic_run();
                }
                screen_set_color(VGA_WHITE, VGA_BLACK);
                screen_println("--- Program ended ---");
            }
        } else if (strcmp(line_buffer, "LIST") == 0 || strcmp(line_buffer, "list") == 0) {
            if (program_pos == 0) {
                screen_println("No program loaded. Use DEMOS to see examples.");
            } else {
                screen_set_color(VGA_YELLOW, VGA_BLACK);
                screen_println(basic_program);
                screen_set_color(VGA_WHITE, VGA_BLACK);
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
                screen_set_color(VGA_LIGHT_RED, VGA_BLACK);
                screen_println("Program too large!");
                screen_set_color(VGA_WHITE, VGA_BLACK);
            }
        }
    }
}

void shell_run(void) {
    char command_buffer[MAX_COMMAND_LENGTH];

    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_println("Welcome to VOS Shell!");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_println("Type 'help' for available commands.\n");

    while (1) {
        print_prompt();
        keyboard_getline(command_buffer, MAX_COMMAND_LENGTH);
        execute_command(command_buffer);
    }
}
