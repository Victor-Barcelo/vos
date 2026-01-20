#include "keyboard.h"
#include "screen.h"
#include "io.h"
#include "idt.h"
#include "string.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

// Keyboard buffer
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_start = 0;
static volatile size_t buffer_end = 0;

// Key states
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool altgr_pressed = false;
static bool caps_lock = false;
static bool extended_key = false;

// Spanish keyboard scancode to ASCII mapping (lowercase/unshifted)
// Scancode index: 0x00-0x3F
static const char scancode_to_ascii[] = {
    0, 27,                                      // 0x00-0x01: NULL, ESC
    '1', '2', '3', '4', '5', '6', '7', '8',     // 0x02-0x09
    '9', '0', '\'', 0,                          // 0x0A-0x0D: 9, 0, ', ¡ (skip ¡)
    '\b', '\t',                                 // 0x0E-0x0F: Backspace, Tab
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',     // 0x10-0x17
    'o', 'p', '`', '+',                         // 0x18-0x1B
    '\n', 0,                                    // 0x1C-0x1D: Enter, LCtrl
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k',     // 0x1E-0x25
    'l', 'n', '\'', '\\',                       // 0x26-0x29: l, n, ', backslash
    0, ']',                                     // 0x2A-0x2B: LShift, ]
    'z', 'x', 'c', 'v', 'b', 'n', 'm',          // 0x2C-0x32
    ',', '.', '-',                              // 0x33-0x35
    0, '*', 0, ' ', 0                           // 0x36-0x3A: RShift, *, LAlt, Space, CapsLock
};

// Spanish keyboard scancode to ASCII mapping (shifted)
static const char scancode_to_ascii_shift[] = {
    0, 27,                                      // 0x00-0x01: NULL, ESC
    '!', '"', '#', '$', '%', '&', '/', '(',     // 0x02-0x09 (shifted numbers)
    ')', '=', '?', 0,                           // 0x0A-0x0D
    '\b', '\t',                                 // 0x0E-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',     // 0x10-0x17
    'O', 'P', '^', '*',                         // 0x18-0x1B
    '\n', 0,                                    // 0x1C-0x1D
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K',     // 0x1E-0x25
    'L', 'N', '"', '|',                         // 0x26-0x29
    0, '[',                                     // 0x2A-0x2B
    'Z', 'X', 'C', 'V', 'B', 'N', 'M',          // 0x2C-0x32
    ';', ':', '_',                              // 0x33-0x35
    0, '*', 0, ' ', 0                           // 0x36-0x3A
};

// AltGr combinations (for @, #, etc.)
static const char scancode_to_ascii_altgr[] = {
    0, 0,                                       // 0x00-0x01
    '|', '@', '#', '~', 0, 0, '{', '[',         // 0x02-0x09
    ']', '}', '\\', 0,                          // 0x0A-0x0D
    0, 0,                                       // 0x0E-0x0F
    0, 0, 0, 0, 0, 0, 0, 0,                     // 0x10-0x17
    0, 0, '[', ']',                             // 0x18-0x1B
    0, 0,                                       // 0x1C-0x1D
    0, 0, 0, 0, 0, 0, 0, 0,                     // 0x1E-0x25
    0, 0, '{', '}',                             // 0x26-0x29
    0, 0,                                       // 0x2A-0x2B
    0, 0, 0, 0, 0, 0, 0,                        // 0x2C-0x32
    0, 0, 0,                                    // 0x33-0x35
    0, 0, 0, 0, 0                               // 0x36-0x3A
};

// Add character to buffer
static void buffer_push(char c) {
    size_t next = (buffer_end + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_start) {
        keyboard_buffer[buffer_end] = c;
        buffer_end = next;
    }
}

// Get character from buffer
static char buffer_pop(void) {
    if (buffer_start == buffer_end) {
        return 0;
    }
    char c = keyboard_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_init(void) {
    // Keyboard IRQ is already set up in idt_init()
    // Just flush any pending data from the keyboard controller
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }
}

void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Check for extended key prefix
    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }

    // Handle extended keys (arrow keys, etc.)
    if (extended_key) {
        extended_key = false;

        // Check for key release
        if (scancode & 0x80) {
            scancode &= 0x7F;
            // Right Alt (AltGr) released
            if (scancode == 0x38) {
                altgr_pressed = false;
            }
        } else {
            // Key press
            if (scancode == 0x38) {
                // Right Alt (AltGr) pressed
                altgr_pressed = true;
            } else {
                // Arrow keys - use special codes
                switch (scancode) {
                    case 0x48: buffer_push(KEY_UP);    break;  // Up arrow
                    case 0x50: buffer_push(KEY_DOWN);  break;  // Down arrow
                    case 0x4B: buffer_push(KEY_LEFT);  break;  // Left arrow
                    case 0x4D: buffer_push(KEY_RIGHT); break;  // Right arrow
                }
            }
        }
        outb(0x20, 0x20);
        return;
    }

    // Check for key release (bit 7 set)
    if (scancode & 0x80) {
        scancode &= 0x7F;
        // Left or right shift released
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = false;
        }
        // Left Ctrl released
        else if (scancode == 0x1D) {
            ctrl_pressed = false;
        }
    } else {
        // Key press
        if (scancode == 0x2A || scancode == 0x36) {
            // Left or right shift pressed
            shift_pressed = true;
        } else if (scancode == 0x1D) {
            // Left Ctrl pressed
            ctrl_pressed = true;
        } else if (scancode == 0x3A) {
            // Caps lock toggled
            caps_lock = !caps_lock;
        } else if (scancode < sizeof(scancode_to_ascii)) {
            char c = 0;

            // Check AltGr first
            if (altgr_pressed && scancode < sizeof(scancode_to_ascii_altgr)) {
                c = scancode_to_ascii_altgr[scancode];
            }

            // If no AltGr char, check shift/normal
            if (c == 0) {
                bool use_shift = shift_pressed;

                // For letters, caps lock inverts shift behavior
                if (scancode >= 0x10 && scancode <= 0x19) use_shift ^= caps_lock;  // Q-P
                if (scancode >= 0x1E && scancode <= 0x26) use_shift ^= caps_lock;  // A-L
                if (scancode >= 0x2C && scancode <= 0x32) use_shift ^= caps_lock;  // Z-M

                if (use_shift) {
                    c = scancode_to_ascii_shift[scancode];
                } else {
                    c = scancode_to_ascii[scancode];
                }
            }

            if (c != 0) {
                buffer_push(c);
            }
        }
    }

    // PIC EOI is sent by the common IRQ handler.
}

bool keyboard_has_key(void) {
    return buffer_start != buffer_end;
}

char keyboard_getchar(void) {
    // Wait for a key (busy wait)
    while (!keyboard_has_key()) {
        __asm__ volatile ("pause");  // Hint to CPU we're spinning
    }
    return buffer_pop();
}

// Command history
#define HISTORY_SIZE 10
#define HISTORY_LINE_SIZE 256
static char history[HISTORY_SIZE][HISTORY_LINE_SIZE];
static int history_count = 0;

// Add command to history
void keyboard_history_add(const char* cmd) {
    if (cmd[0] == '\0') return;  // Don't add empty commands

    // Don't add if same as last command
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_SIZE;
        if (strcmp(history[last], cmd) == 0) return;
    }

    int idx = history_count % HISTORY_SIZE;
    strncpy(history[idx], cmd, HISTORY_LINE_SIZE - 1);
    history[idx][HISTORY_LINE_SIZE - 1] = '\0';
    history_count++;
}

// Get line with history support
void keyboard_getline_history(char* buffer, size_t max_len) {
    size_t pos = 0;
    int hist_idx = history_count;  // Start at current (empty) position
    char saved_line[HISTORY_LINE_SIZE] = {0};  // Save current input when browsing
    bool saved = false;

    buffer[0] = '\0';

    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            screen_putchar('\n');
            break;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                screen_backspace();
            }
        } else if (c == KEY_UP) {
            // Go back in history
            if (hist_idx > 0 && hist_idx > history_count - HISTORY_SIZE) {
                // Save current line first time we press up
                if (!saved && hist_idx == history_count) {
                    strncpy(saved_line, buffer, HISTORY_LINE_SIZE - 1);
                    saved = true;
                }
                hist_idx--;

                // Clear current line on screen
                while (pos > 0) {
                    screen_backspace();
                    pos--;
                }

                // Copy history entry
                int h = hist_idx % HISTORY_SIZE;
                strcpy(buffer, history[h]);
                pos = strlen(buffer);
                screen_print(buffer);
            }
        } else if (c == KEY_DOWN) {
            // Go forward in history
            if (hist_idx < history_count) {
                hist_idx++;

                // Clear current line on screen
                while (pos > 0) {
                    screen_backspace();
                    pos--;
                }

                if (hist_idx == history_count) {
                    // Restore saved line
                    strcpy(buffer, saved_line);
                } else {
                    int h = hist_idx % HISTORY_SIZE;
                    strcpy(buffer, history[h]);
                }
                pos = strlen(buffer);
                screen_print(buffer);
            }
        } else if (c >= ' ' && c <= '~') {
            buffer[pos++] = c;
            buffer[pos] = '\0';
            screen_putchar(c);
        }
    }

    buffer[pos] = '\0';

    // Add to history
    keyboard_history_add(buffer);
}

// Legacy getline without history (for compatibility)
void keyboard_getline(char* buffer, size_t max_len) {
    keyboard_getline_history(buffer, max_len);
}
