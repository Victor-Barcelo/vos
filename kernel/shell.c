#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"

#define MAX_COMMAND_LENGTH 256

// Forward declarations for commands
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char* args);
static void cmd_info(void);
static void cmd_reboot(void);
static void cmd_halt(void);
static void cmd_color(const char* args);

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
    screen_println("  color <0-15>  - Change text color");
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
