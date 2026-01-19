#include "keyboard.h"
#include "screen.h"
#include "io.h"
#include "idt.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

// Keyboard buffer
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_start = 0;
static volatile size_t buffer_end = 0;

// Shift key state
static bool shift_pressed = false;
static bool caps_lock = false;

// US keyboard scancode to ASCII mapping (lowercase)
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

// US keyboard scancode to ASCII mapping (uppercase/shifted)
static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
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

    // Check for key release (bit 7 set)
    if (scancode & 0x80) {
        scancode &= 0x7F;
        // Left or right shift released
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = false;
        }
    } else {
        // Key press
        if (scancode == 0x2A || scancode == 0x36) {
            // Left or right shift pressed
            shift_pressed = true;
        } else if (scancode == 0x3A) {
            // Caps lock toggled
            caps_lock = !caps_lock;
        } else if (scancode < sizeof(scancode_to_ascii)) {
            char c;
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

            if (c != 0) {
                buffer_push(c);
            }
        }
    }

    // Send End of Interrupt to PIC
    outb(0x20, 0x20);
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

void keyboard_getline(char* buffer, size_t max_len) {
    size_t pos = 0;

    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            screen_putchar('\n');
            break;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                screen_backspace();
            }
        } else if (c >= ' ' && c <= '~') {
            buffer[pos++] = c;
            screen_putchar(c);
        }
    }

    buffer[pos] = '\0';
}
