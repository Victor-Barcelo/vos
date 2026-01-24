# Chapter 13: Mouse Driver

## PS/2 Mouse Protocol

The PS/2 mouse connects through the same 8042 controller as the keyboard, using IRQ12.

### Initialization

```c
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

static void ps2_wait_write(void) {
    while (inb(PS2_STATUS) & 0x02);  // Wait for input buffer clear
}

static void ps2_wait_read(void) {
    while (!(inb(PS2_STATUS) & 0x01));  // Wait for output buffer full
}

static void mouse_write(uint8_t data) {
    ps2_wait_write();
    outb(PS2_COMMAND, 0xD4);  // Send to mouse
    ps2_wait_write();
    outb(PS2_DATA, data);
}

static uint8_t mouse_read(void) {
    ps2_wait_read();
    return inb(PS2_DATA);
}

void mouse_init(void) {
    // Enable auxiliary device (mouse)
    ps2_wait_write();
    outb(PS2_COMMAND, 0xA8);

    // Enable interrupts for auxiliary device
    ps2_wait_write();
    outb(PS2_COMMAND, 0x20);  // Read command byte
    uint8_t status = mouse_read();
    status |= 0x02;            // Enable IRQ12
    ps2_wait_write();
    outb(PS2_COMMAND, 0x60);  // Write command byte
    ps2_wait_write();
    outb(PS2_DATA, status);

    // Set defaults
    mouse_write(0xF6);
    mouse_read();  // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();  // ACK

    // Register IRQ handler
    irq_register_handler(12, mouse_irq_handler);
}
```

## Mouse Packet Format

The standard PS/2 mouse sends 3-byte packets:

```
Byte 0 (Status):
+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
| YOF | XOF | YS  | XS  |  1  | MB  | RB  | LB  |
+-----+-----+-----+-----+-----+-----+-----+-----+

YOF/XOF: Y/X overflow
YS/XS: Y/X sign bits (1 = negative)
MB/RB/LB: Middle/Right/Left button

Byte 1: X movement (9-bit signed, XS is sign)
Byte 2: Y movement (9-bit signed, YS is sign)
```

## Mouse IRQ Handler

```c
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static int16_t mouse_x = 0;
static int16_t mouse_y = 0;
static uint8_t mouse_buttons = 0;

void mouse_irq_handler(interrupt_frame_t *frame) {
    uint8_t status = inb(PS2_STATUS);

    // Check if data is from mouse (bit 5 set)
    if (!(status & 0x20)) return;

    uint8_t data = inb(PS2_DATA);

    switch (mouse_cycle) {
    case 0:
        // First byte - status
        if (data & 0x08) {  // Bit 3 should always be 1
            mouse_packet[0] = data;
            mouse_cycle = 1;
        }
        break;

    case 1:
        // Second byte - X movement
        mouse_packet[1] = data;
        mouse_cycle = 2;
        break;

    case 2:
        // Third byte - Y movement
        mouse_packet[2] = data;
        mouse_cycle = 0;

        // Process complete packet
        process_mouse_packet();
        break;
    }
}
```

## Processing Mouse Data

```c
static void process_mouse_packet(void) {
    uint8_t status = mouse_packet[0];

    // Extract button state
    mouse_buttons = status & 0x07;

    // Calculate X movement (9-bit signed)
    int16_t dx = mouse_packet[1];
    if (status & 0x10) {
        dx |= 0xFF00;  // Sign extend
    }

    // Calculate Y movement (9-bit signed, inverted)
    int16_t dy = mouse_packet[2];
    if (status & 0x20) {
        dy |= 0xFF00;
    }
    dy = -dy;  // Invert Y (screen Y increases downward)

    // Update position with bounds checking
    mouse_x += dx;
    mouse_y += dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen_width()) mouse_x = screen_width() - 1;
    if (mouse_y >= screen_height()) mouse_y = screen_height() - 1;

    // Notify listeners
    if (mouse_callback) {
        mouse_callback(mouse_x, mouse_y, mouse_buttons);
    }
}
```

## Mouse State Access

```c
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t buttons;
} mouse_state_t;

void mouse_get_state(mouse_state_t *state) {
    uint32_t flags = irq_save();
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons;
    irq_restore(flags);
}

bool mouse_button_left(void) {
    return mouse_buttons & 0x01;
}

bool mouse_button_right(void) {
    return mouse_buttons & 0x02;
}

bool mouse_button_middle(void) {
    return mouse_buttons & 0x04;
}
```

## Xterm Mouse Sequences

VOS supports xterm-compatible mouse reporting for terminal applications:

### Enable Mouse Tracking

```c
// Send to terminal to enable mouse reporting:
// \e[?1000h - Enable basic mouse tracking
// \e[?1006h - Enable SGR extended mode

// Disable:
// \e[?1000l - Disable mouse tracking
```

### SGR Mouse Format

When mouse events occur, the terminal sends sequences:

```
Press:   \e[<button;x;yM
Release: \e[<button;x;ym

button: 0=left, 1=middle, 2=right, 32/33/34=motion with button
x, y: 1-based coordinates
```

### Mouse Event Reporting

```c
static bool mouse_tracking_enabled = false;

void terminal_mouse_event(int x, int y, int buttons, bool pressed) {
    if (!mouse_tracking_enabled) return;

    // Convert to 1-based terminal coordinates
    int tx = x / font_width + 1;
    int ty = y / font_height + 1;

    int button = 0;
    if (buttons & 0x01) button = 0;       // Left
    else if (buttons & 0x02) button = 2;  // Right
    else if (buttons & 0x04) button = 1;  // Middle

    char seq[32];
    snprintf(seq, sizeof(seq), "\e[<%d;%d;%d%c",
             button, tx, ty, pressed ? 'M' : 'm');

    // Add to input buffer
    for (int i = 0; seq[i]; i++) {
        keyboard_buffer_push(seq[i]);
    }
}
```

## Mouse Syscalls

```c
// Get current mouse state
int32_t sys_mouse_get(mouse_state_t *state) {
    if (!state) return -EFAULT;

    mouse_get_state(state);
    return 0;
}

// Wait for mouse event
int32_t sys_mouse_wait(mouse_event_t *event) {
    // Block until mouse moves or button changes
    // ...
}
```

## Drawing a Mouse Cursor

For graphical applications:

```c
static uint32_t cursor_backup[16 * 16];
static int cursor_x = -1, cursor_y = -1;

static const uint8_t cursor_data[16][16] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
    // ... arrow pattern ...
};

void draw_cursor(int x, int y) {
    // Restore previous background
    if (cursor_x >= 0) {
        restore_background(cursor_x, cursor_y, cursor_backup);
    }

    // Save new background
    save_background(x, y, cursor_backup);

    // Draw cursor
    for (int dy = 0; dy < 16 && y + dy < screen_height(); dy++) {
        for (int dx = 0; dx < 16 && x + dx < screen_width(); dx++) {
            uint8_t pixel = cursor_data[dy][dx];
            if (pixel == 1) {
                fb_putpixel(x + dx, y + dy, 0x000000);  // Black
            } else if (pixel == 2) {
                fb_putpixel(x + dx, y + dy, 0xFFFFFF);  // White
            }
        }
    }

    cursor_x = x;
    cursor_y = y;
}
```

## Summary

The VOS mouse driver provides:

1. **PS/2 mouse initialization** through 8042 controller
2. **Packet parsing** for position and button state
3. **Coordinate tracking** with screen bounds
4. **Xterm mouse sequences** for terminal compatibility
5. **Syscalls** for user program access
6. **Cursor drawing** for graphical mode

Mouse support enables interactive applications and GUI elements.

---

*Previous: [Chapter 12: Keyboard Driver](12_keyboard.md)*
*Next: [Chapter 14: ATA Disk Driver](14_ata.md)*
