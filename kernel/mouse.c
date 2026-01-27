#include "mouse.h"
#include "interrupts.h"
#include "io.h"
#include "keyboard.h"
#include "screen.h"
#include "serial.h"

// PS/2 controller ports.
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_CMD_PORT     0x64

// PS/2 controller commands.
#define PS2_CMD_READ_CCB   0x20
#define PS2_CMD_WRITE_CCB  0x60
#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_WRITE_AUX  0xD4

// PS/2 mouse commands.
#define MOUSE_CMD_SET_DEFAULTS      0xF6
#define MOUSE_CMD_ENABLE_STREAMING  0xF4
#define MOUSE_CMD_SET_SAMPLE_RATE   0xF3
#define MOUSE_CMD_GET_DEVICE_ID     0xF2

#define MOUSE_ACK 0xFA

static bool mouse_present = false;
static bool mouse_has_wheel = false;
static uint8_t packet[4];
static uint8_t packet_len = 3;
static uint8_t packet_off = 0;

// Pointer position tracked in "terminal pixel space" (top-left of cell 0,0 is 0,0).
static int32_t mouse_px = 0;
static int32_t mouse_py = 0;
static int32_t mouse_cell_x = 0;
static int32_t mouse_cell_y = 0;
static uint8_t prev_buttons = 0;

static uint32_t u32_to_dec(uint32_t v, char out[11]) {
    if (!out) {
        return 0;
    }
    char tmp[11];
    uint32_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v && n < (uint32_t)sizeof(tmp));
    for (uint32_t i = 0; i < n; i++) {
        out[i] = tmp[n - 1u - i];
    }
    return n;
}

static void mouse_emit_xterm(uint32_t b, uint32_t col_1based, uint32_t row_1based, bool press) {
    if (!screen_vt_mouse_reporting_enabled()) {
        return;
    }

    if (col_1based == 0) col_1based = 1;
    if (row_1based == 0) row_1based = 1;

    if (screen_vt_mouse_reporting_sgr()) {
        // CSI <b;x;yM / CSI <b;x;ym (SGR 1006).
        uint8_t seq[48];
        uint32_t n = 0;
        seq[n++] = 0x1Bu;
        seq[n++] = (uint8_t)'[';
        seq[n++] = (uint8_t)'<';

        char num[11];
        uint32_t len = u32_to_dec(b, num);
        for (uint32_t i = 0; i < len && n < sizeof(seq); i++) seq[n++] = (uint8_t)num[i];
        seq[n++] = (uint8_t)';';

        len = u32_to_dec(col_1based, num);
        for (uint32_t i = 0; i < len && n < sizeof(seq); i++) seq[n++] = (uint8_t)num[i];
        seq[n++] = (uint8_t)';';

        len = u32_to_dec(row_1based, num);
        for (uint32_t i = 0; i < len && n < sizeof(seq); i++) seq[n++] = (uint8_t)num[i];
        seq[n++] = (uint8_t)(press ? 'M' : 'm');

        keyboard_inject_bytes(seq, n);
        return;
    }

    // Legacy X10 mouse mode: CSI M b x y (press + release-as-button3).
    uint8_t seq[6];
    seq[0] = 0x1Bu;
    seq[1] = (uint8_t)'[';
    seq[2] = (uint8_t)'M';

    uint32_t bb = b;
    if (!press) {
        bb = 3;
    }
    uint32_t x = col_1based;
    uint32_t y = row_1based;
    if (x > 223u) x = 223u;
    if (y > 223u) y = 223u;
    seq[3] = (uint8_t)(32u + (bb & 0xFFu));
    seq[4] = (uint8_t)(32u + (x & 0xFFu));
    seq[5] = (uint8_t)(32u + (y & 0xFFu));

    keyboard_inject_bytes(seq, sizeof(seq));
}

static bool ps2_wait_input_empty(void) {
    for (uint32_t i = 0; i < 100000u; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x02u) == 0) {
            return true;
        }
        io_wait();
    }
    return false;
}

static bool ps2_wait_output_full(void) {
    for (uint32_t i = 0; i < 100000u; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x01u) != 0) {
            return true;
        }
        io_wait();
    }
    return false;
}

static void ps2_write_cmd(uint8_t cmd) {
    if (!ps2_wait_input_empty()) {
        return;
    }
    outb(PS2_CMD_PORT, cmd);
}

static void ps2_write_data(uint8_t val) {
    if (!ps2_wait_input_empty()) {
        return;
    }
    outb(PS2_DATA_PORT, val);
}

static bool ps2_read_data(uint8_t* out) {
    if (!out) {
        return false;
    }
    if (!ps2_wait_output_full()) {
        return false;
    }
    *out = inb(PS2_DATA_PORT);
    return true;
}

static void ps2_flush_output(void) {
    for (uint32_t i = 0; i < 32u; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x01u) == 0) {
            break;
        }
        (void)inb(PS2_DATA_PORT);
    }
}

static bool mouse_write(uint8_t val) {
    ps2_write_cmd(PS2_CMD_WRITE_AUX);
    ps2_write_data(val);
    return true;
}

static bool mouse_read_ack(uint8_t* out_ack) {
    uint8_t b = 0;
    if (!ps2_read_data(&b)) {
        return false;
    }
    if (out_ack) {
        *out_ack = b;
    }
    return true;
}

static bool mouse_send_cmd(uint8_t cmd) {
    uint8_t ack = 0;
    if (!mouse_write(cmd)) {
        return false;
    }
    if (!mouse_read_ack(&ack)) {
        return false;
    }
    return ack == MOUSE_ACK;
}

static bool mouse_send_cmd_arg(uint8_t cmd, uint8_t arg) {
    uint8_t ack = 0;
    if (!mouse_write(cmd)) {
        return false;
    }
    if (!mouse_read_ack(&ack) || ack != MOUSE_ACK) {
        return false;
    }
    if (!mouse_write(arg)) {
        return false;
    }
    if (!mouse_read_ack(&ack)) {
        return false;
    }
    return ack == MOUSE_ACK;
}

static void mouse_try_enable_wheel(void) {
    // IntelliMouse wheel enable sequence: sample rates 200, 100, 80 then get device ID.
    (void)mouse_send_cmd_arg(MOUSE_CMD_SET_SAMPLE_RATE, 200);
    (void)mouse_send_cmd_arg(MOUSE_CMD_SET_SAMPLE_RATE, 100);
    (void)mouse_send_cmd_arg(MOUSE_CMD_SET_SAMPLE_RATE, 80);

    // Get device ID.
    uint8_t ack = 0;
    if (!mouse_write(MOUSE_CMD_GET_DEVICE_ID)) {
        return;
    }
    if (!mouse_read_ack(&ack) || ack != MOUSE_ACK) {
        return;
    }

    uint8_t id = 0;
    if (!ps2_read_data(&id)) {
        return;
    }

    if (id == 0x03u || id == 0x04u) {
        mouse_has_wheel = true;
        packet_len = 4;
    }
}

static void mouse_reset_position(void) {
    int cols = screen_cols();
    int rows = screen_usable_rows();
    uint32_t fw = screen_font_width();
    uint32_t fh = screen_font_height();
    if (cols <= 0 || rows <= 0 || fw == 0 || fh == 0) {
        mouse_px = 0;
        mouse_py = 0;
        return;
    }

    int32_t w = (int32_t)((uint32_t)cols * fw);
    int32_t h = (int32_t)((uint32_t)rows * fh);
    mouse_px = w / 2;
    mouse_py = h / 2;
    mouse_cell_x = mouse_px / (int32_t)fw;
    mouse_cell_y = mouse_py / (int32_t)fh;
    screen_mouse_set_pos((int)mouse_cell_x, (int)mouse_cell_y);
}

static void mouse_apply_motion(int32_t dx, int32_t dy) {
    int cols = screen_cols();
    int rows = screen_usable_rows();
    uint32_t fw = screen_font_width();
    uint32_t fh = screen_font_height();
    if (cols <= 0 || rows <= 0 || fw == 0 || fh == 0) {
        return;
    }

    int32_t w = (int32_t)((uint32_t)cols * fw);
    int32_t h = (int32_t)((uint32_t)rows * fh);
    if (w <= 0 || h <= 0) {
        return;
    }

    mouse_px += dx;
    // PS/2 packets report positive Y when moving up.
    mouse_py -= dy;

    if (mouse_px < 0) mouse_px = 0;
    if (mouse_py < 0) mouse_py = 0;
    if (mouse_px >= w) mouse_px = w - 1;
    if (mouse_py >= h) mouse_py = h - 1;

    mouse_cell_x = mouse_px / (int32_t)fw;
    mouse_cell_y = mouse_py / (int32_t)fh;
    screen_mouse_set_pos((int)mouse_cell_x, (int)mouse_cell_y);
}

static void mouse_handle_packet(void) {
    uint8_t b0 = packet[0];

    // Ignore overflow packets to avoid huge jumps.
    if ((b0 & 0xC0u) != 0) {
        return;
    }

    int32_t dx = (int32_t)packet[1];
    int32_t dy = (int32_t)packet[2];
    if (b0 & 0x10u) dx -= 256;
    if (b0 & 0x20u) dy -= 256;

    if (dx != 0 || dy != 0) {
        mouse_apply_motion(dx, dy);
    }

    uint8_t buttons = b0 & 0x07u;
    uint8_t changed = (uint8_t)(buttons ^ prev_buttons);
    if (changed) {
        uint32_t col = (uint32_t)mouse_cell_x + 1u;
        uint32_t row = (uint32_t)mouse_cell_y + 1u;

        if (changed & 0x01u) {
            mouse_emit_xterm(0, col, row, (buttons & 0x01u) != 0);
        }
        if (changed & 0x02u) {
            mouse_emit_xterm(2, col, row, (buttons & 0x02u) != 0);
        }
        if (changed & 0x04u) {
            mouse_emit_xterm(1, col, row, (buttons & 0x04u) != 0);
        }
    }
    prev_buttons = buttons;

    if (mouse_has_wheel && packet_len == 4) {
        uint8_t zraw = packet[3] & 0x0Fu;
        int8_t z = (int8_t)zraw;
        if (z & 0x08) {
            z |= (int8_t)0xF0;
        }

        if (z != 0) {
            uint32_t col = (uint32_t)mouse_cell_x + 1u;
            uint32_t row = (uint32_t)mouse_cell_y + 1u;
            if (screen_vt_mouse_reporting_enabled() && screen_vt_mouse_reporting_wheel()) {
                // Wheel maps to buttons 4/5 in xterm encodings.
                if (z > 0) {
                    mouse_emit_xterm(64, col, row, true);
                } else {
                    mouse_emit_xterm(65, col, row, true);
                }
            } else {
                // No reporting: treat wheel as console scrollback.
                if (z > 0) {
                    screen_scrollback_lines(-3);
                } else {
                    screen_scrollback_lines(3);
                }
            }
        }
    }
}

static void mouse_irq_handler(interrupt_frame_t* frame) {
    (void)frame;

    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & 0x01u) == 0) {
        return;
    }
    // Bit 5 indicates this byte came from the auxiliary device (mouse).
    if ((status & 0x20u) == 0) {
        (void)inb(PS2_DATA_PORT);
        return;
    }

    uint8_t b = inb(PS2_DATA_PORT);

    // Sync to the first byte of a packet (bit 3 is always set).
    if (packet_off == 0 && (b & 0x08u) == 0) {
        return;
    }

    // Bounds check before writing to packet buffer
    if (packet_off >= sizeof(packet)) {
        packet_off = 0;  // Reset on overflow (shouldn't happen)
        return;
    }

    packet[packet_off++] = b;
    if (packet_off < packet_len) {
        return;
    }

    packet_off = 0;
    mouse_handle_packet();
}

void mouse_init(void) {
    // Enable IRQ12 on the PIC (slave PIC, bit 4).
    uint8_t slave_mask = inb(0xA1);
    slave_mask &= (uint8_t)~(1u << 4);
    outb(0xA1, slave_mask);

    // Enable the auxiliary device.
    ps2_write_cmd(PS2_CMD_ENABLE_AUX);

    // Enable mouse IRQs in the controller command byte.
    ps2_write_cmd(PS2_CMD_READ_CCB);
    uint8_t ccb = 0;
    if (!ps2_read_data(&ccb)) {
        serial_write_string("[MOUSE] no controller response; mouse disabled\n");
        return;
    }
    ccb |= 0x02u; // enable IRQ12
    ccb |= 0x01u; // keep IRQ1 enabled too
    ps2_write_cmd(PS2_CMD_WRITE_CCB);
    ps2_write_data(ccb);

    ps2_flush_output();

    // Reset to defaults and enable streaming.
    if (!mouse_send_cmd(MOUSE_CMD_SET_DEFAULTS)) {
        serial_write_string("[MOUSE] no ACK on defaults; mouse disabled\n");
        return;
    }

    mouse_try_enable_wheel();

    if (!mouse_send_cmd(MOUSE_CMD_ENABLE_STREAMING)) {
        serial_write_string("[MOUSE] no ACK on enable; mouse disabled\n");
        return;
    }

    mouse_present = true;
    prev_buttons = 0;
    packet_off = 0;

    // Don't show visual mouse cursor in shell (no GUI use case yet)
    // Mouse driver still works for VT mouse reporting in TUI apps
    // screen_mouse_set_enabled(true);
    mouse_reset_position();

    irq_register_handler(12, mouse_irq_handler);

    serial_write_string("[MOUSE] PS/2 initialized");
    if (mouse_has_wheel) {
        serial_write_string(" (wheel)");
    }
    serial_write_char('\n');
}
