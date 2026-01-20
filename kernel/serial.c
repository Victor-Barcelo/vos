#include "serial.h"
#include "io.h"

#define COM1_BASE 0x3F8

static bool serial_initialized = false;

static bool serial_transmit_empty(void) {
    return (inb(COM1_BASE + 5) & 0x20) != 0;
}

static bool serial_received(void) {
    return (inb(COM1_BASE + 5) & 0x01) != 0;
}

void serial_init(void) {
    outb(COM1_BASE + 1, 0x00);  // Disable interrupts
    outb(COM1_BASE + 3, 0x80);  // Enable DLAB
    outb(COM1_BASE + 0, 0x01);  // Divisor low (115200 baud)
    outb(COM1_BASE + 1, 0x00);  // Divisor high
    outb(COM1_BASE + 3, 0x03);  // 8 bits, no parity, one stop bit
    outb(COM1_BASE + 2, 0xC7);  // Enable FIFO, clear, 14-byte threshold
    outb(COM1_BASE + 4, 0x0B);  // IRQs enabled, RTS/DSR set

    // Loopback test
    outb(COM1_BASE + 4, 0x1E);
    outb(COM1_BASE + 0, 0xAE);
    (void)inb(COM1_BASE + 0);   // Some emulators don't reflect loopback reads reliably.

    outb(COM1_BASE + 4, 0x0F);  // Normal operation mode
    serial_initialized = true;
}

bool serial_is_initialized(void) {
    return serial_initialized;
}

void serial_write_char(char c) {
    if (!serial_initialized) {
        return;
    }
    if (c == '\n') {
        serial_write_char('\r');
    }
    while (!serial_transmit_empty()) {
        __asm__ volatile ("pause");
    }
    outb(COM1_BASE + 0, (uint8_t)c);
}

void serial_write_string(const char* str) {
    if (!serial_initialized || !str) {
        return;
    }
    while (*str) {
        serial_write_char(*str++);
    }
}

void serial_write_hex(uint32_t value) {
    serial_write_string("0x");
    const char hex_chars[] = "0123456789ABCDEF";
    bool leading = true;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        if (nibble != 0 || !leading || i == 0) {
            serial_write_char(hex_chars[nibble]);
            leading = false;
        }
    }
}

void serial_write_dec(int32_t value) {
    if (value < 0) {
        serial_write_char('-');
        value = -value;
    }
    if (value == 0) {
        serial_write_char('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) {
        serial_write_char(buf[--i]);
    }
}

bool serial_try_read_char(char* out) {
    if (!serial_initialized || !out) {
        return false;
    }
    if (!serial_received()) {
        return false;
    }

    char c = (char)inb(COM1_BASE + 0);
    if (c == '\r') {
        c = '\n';
    } else if ((uint8_t)c == 0x7Fu) {
        c = '\b';
    }

    *out = c;
    return true;
}
