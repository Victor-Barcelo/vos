# Chapter 15: Serial Port

When writing a kernel, the screen might not be initialized yet (or might crash). A serial port provides a simple, reliable debug channel that keeps working even when the framebuffer path is broken.

## Serial Port Hardware

VOS configures **COM1** at I/O base `0x3F8`:

### UART Registers

| Offset | DLAB=0 Read | DLAB=0 Write | DLAB=1 |
|--------|-------------|--------------|--------|
| +0 | RBR (Receive) | THR (Transmit) | Divisor Low |
| +1 | IER (Int Enable) | IER | Divisor High |
| +2 | IIR (Int ID) | FCR (FIFO Ctrl) | - |
| +3 | LCR (Line Ctrl) | LCR | - |
| +4 | MCR (Modem Ctrl) | MCR | - |
| +5 | LSR (Line Status) | - | - |
| +6 | MSR (Modem Status) | - | - |
| +7 | Scratch | Scratch | - |

### Line Control Register (LCR)

```
Bit 7: DLAB - Divisor Latch Access Bit
Bit 6: Set Break
Bit 5-3: Parity (000=none)
Bit 2: Stop bits (0=1, 1=2)
Bit 1-0: Data bits (11=8 bits)
```

### Line Status Register (LSR)

```
Bit 6: Transmitter empty
Bit 5: THR empty (can write)
Bit 4: Break interrupt
Bit 3: Framing error
Bit 2: Parity error
Bit 1: Overrun error
Bit 0: Data ready (can read)
```

## Serial Initialization

```c
#define COM1_PORT 0x3F8

void serial_init(void) {
    // Disable interrupts
    outb(COM1_PORT + 1, 0x00);

    // Set DLAB to access divisor
    outb(COM1_PORT + 3, 0x80);

    // Set divisor to 1 (115200 baud)
    // Divisor = 115200 / baud rate
    outb(COM1_PORT + 0, 0x01);  // Low byte
    outb(COM1_PORT + 1, 0x00);  // High byte

    // 8 bits, no parity, 1 stop bit, DLAB off
    outb(COM1_PORT + 3, 0x03);

    // Enable FIFO, clear buffers, 14-byte threshold
    outb(COM1_PORT + 2, 0xC7);

    // Enable RTS, DTR, enable IRQs
    outb(COM1_PORT + 4, 0x0B);

    // Loopback test
    outb(COM1_PORT + 4, 0x1E);  // Set loopback mode
    outb(COM1_PORT + 0, 0xAE);  // Send test byte

    if (inb(COM1_PORT + 0) != 0xAE) {
        // Serial port not working
        return;
    }

    // Normal operation mode
    outb(COM1_PORT + 4, 0x0F);
}
```

### Baud Rate Divisors

| Baud Rate | Divisor |
|-----------|---------|
| 115200 | 1 |
| 57600 | 2 |
| 38400 | 3 |
| 19200 | 6 |
| 9600 | 12 |

## Writing to Serial

```c
static bool serial_transmit_ready(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!serial_transmit_ready());
    outb(COM1_PORT, c);
}

void serial_write_string(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*str++);
    }
}
```

## Formatted Output

```c
void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    serial_write_string(buffer);

    va_end(args);
}

void serial_write_hex(uint32_t value) {
    serial_write_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        serial_write_char(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

void serial_write_dec(int32_t value) {
    if (value < 0) {
        serial_write_char('-');
        value = -value;
    }

    char buf[12];
    int i = 0;
    do {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);

    while (i--) {
        serial_write_char(buf[i]);
    }
}
```

## Console Mirroring

VOS mirrors all screen output to serial:

```c
void screen_putchar(char c) {
    // ... render to screen ...

    // Mirror to serial
    serial_write_char(c);
}
```

This provides:
- Debug output even if screen fails
- Log capture via QEMU
- Consistent logging during development

## Panic Handling

When a fatal error occurs, serial output captures the crash info:

```c
void panic_with_frame(const char *msg, interrupt_frame_t *frame) {
    cli();  // Disable interrupts

    // Print to serial (always works)
    serial_printf("\n=== KERNEL PANIC ===\n");
    serial_printf("Message: %s\n", msg);

    if (frame) {
        serial_printf("\nRegisters:\n");
        serial_printf("  EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
                     frame->eax, frame->ebx, frame->ecx, frame->edx);
        serial_printf("  ESI=%08x EDI=%08x EBP=%08x ESP=%08x\n",
                     frame->esi, frame->edi, frame->ebp, frame->esp_dummy);
        serial_printf("  EIP=%08x CS=%04x EFLAGS=%08x\n",
                     frame->eip, frame->cs, frame->eflags);
        serial_printf("  DS=%04x ES=%04x FS=%04x GS=%04x\n",
                     frame->ds, frame->es, frame->fs, frame->gs);
        serial_printf("  INT=%02x ERR=%08x\n",
                     frame->int_no, frame->err_code);

        // For page faults, show CR2
        if (frame->int_no == 14) {
            uint32_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            serial_printf("  CR2=%08x (faulting address)\n", cr2);
        }
    }

    serial_printf("\nSystem halted.\n");

    // Try to print to screen too
    screen_set_color(VGA_WHITE, VGA_RED);
    screen_print("\n*** KERNEL PANIC ***\n");
    screen_print(msg);
    screen_print("\nCheck serial output for details.\n");

    // Halt forever
    for (;;) {
        hlt();
    }
}
```

## QEMU Serial Configuration

### View in Terminal

```bash
qemu-system-i386 -cdrom vos.iso -serial stdio
```

### Write to File

```bash
qemu-system-i386 -cdrom vos.iso -serial file:serial.log
tail -f serial.log
```

### Multiple Serial Ports

```bash
qemu-system-i386 -cdrom vos.iso \
    -serial stdio \
    -serial file:debug.log
```

## Reading from Serial

For bidirectional communication:

```c
static bool serial_data_ready(void) {
    return inb(COM1_PORT + 5) & 0x01;
}

int serial_read_char(void) {
    if (!serial_data_ready()) {
        return -1;  // No data
    }
    return inb(COM1_PORT);
}

// Blocking read
char serial_getchar(void) {
    while (!serial_data_ready()) {
        hlt();
    }
    return inb(COM1_PORT);
}
```

## Serial Interrupt Handler

For interrupt-driven serial:

```c
void serial_irq_handler(interrupt_frame_t *frame) {
    while (serial_data_ready()) {
        char c = inb(COM1_PORT);
        serial_buffer_push(c);
    }
}

void serial_enable_interrupts(void) {
    // Enable receive interrupt
    outb(COM1_PORT + 1, 0x01);

    // Register handler
    irq_register_handler(4, serial_irq_handler);  // COM1 = IRQ4
}
```

## Debugging Tips

### Log Levels

```c
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

static log_level_t current_level = LOG_INFO;

void log(log_level_t level, const char *fmt, ...) {
    if (level < current_level) return;

    const char *prefix[] = {"[DBG]", "[INF]", "[WRN]", "[ERR]"};
    serial_printf("%s ", prefix[level]);

    va_list args;
    va_start(args, fmt);
    // ... format and print ...
    va_end(args);

    serial_printf("\n");
}
```

### Function Tracing

```c
#define TRACE_ENTER() serial_printf(">>> %s\n", __func__)
#define TRACE_EXIT()  serial_printf("<<< %s\n", __func__)

void some_function(void) {
    TRACE_ENTER();
    // ...
    TRACE_EXIT();
}
```

## Summary

Serial port provides essential debugging:

1. **Early boot output** before screen is ready
2. **Crash dumps** with full register state
3. **Log capture** via QEMU file redirect
4. **Console mirroring** for all screen output
5. **Reliable channel** that rarely fails

Always initialize serial first - it's your lifeline when things go wrong.

---

*Previous: [Chapter 14: ATA Disk Driver](14_ata.md)*
*Next: [Chapter 16: Virtual File System](16_vfs.md)*
