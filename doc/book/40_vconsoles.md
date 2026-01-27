# Chapter 40: Virtual Consoles

VOS supports multiple virtual consoles (VCs), allowing users to switch between independent terminal sessions using keyboard shortcuts.

## Overview

Virtual consoles provide:

1. **Multiple sessions** - Up to 4 independent terminals
2. **Quick switching** - Alt+1/2/3/4 hotkeys
3. **Isolated state** - Each VC has its own screen buffer
4. **Per-VC processes** - Separate shell on each console

```
+------------------+  +------------------+  +------------------+
|    Console 1     |  |    Console 2     |  |    Console 3     |
|  (login shell)   |  |  (editor)        |  |  (monitoring)    |
+------------------+  +------------------+  +------------------+
        |                    |                    |
        v                    v                    v
+--------------------------------------------------------+
|                    Screen Driver                        |
|  - Per-console cell buffers                            |
|  - Active console rendering                            |
|  - Keyboard routing                                    |
+--------------------------------------------------------+
        |
        v
+------------------+
|   Framebuffer    |
+------------------+
```

## Virtual Console Architecture

### Console Structure

```c
#define MAX_CONSOLES 4

typedef struct virtual_console {
    // Screen state
    cell_t* cell_buffer;       // Character cells
    int cursor_x, cursor_y;    // Cursor position
    uint8_t fg_color, bg_color;// Current colors

    // Scroll state
    int scroll_offset;         // Scrollback position
    cell_t* scrollback_buffer; // History buffer

    // Terminal state
    int parse_state;           // ANSI parser state
    char csi_buffer[16];       // CSI sequence buffer

    // Process association
    pid_t foreground_pid;      // Process receiving input

    // Status
    bool dirty;                // Needs redraw
    bool active;               // Currently visible
} virtual_console_t;

static virtual_console_t consoles[MAX_CONSOLES];
static int active_console = 0;
```

### Per-Console Cell Buffer

Each console maintains its own screen buffer:

```c
void console_init(int idx) {
    virtual_console_t* vc = &consoles[idx];

    // Allocate cell buffer
    int cells = screen_cols_value * screen_rows_value;
    vc->cell_buffer = kcalloc(cells, sizeof(cell_t));

    // Initialize cells
    for (int i = 0; i < cells; i++) {
        vc->cell_buffer[i].codepoint = ' ';
        vc->cell_buffer[i].fg = VGA_WHITE;
        vc->cell_buffer[i].bg = VGA_BLACK;
    }

    // Initialize cursor
    vc->cursor_x = 0;
    vc->cursor_y = 0;
    vc->fg_color = VGA_WHITE;
    vc->bg_color = VGA_BLACK;

    // Initialize parser
    vc->parse_state = STATE_NORMAL;

    vc->active = (idx == 0);
    vc->dirty = true;
}
```

## Console Switching

### Keyboard Handler

```c
// In keyboard.c
void keyboard_irq_handler(interrupt_frame_t* frame) {
    uint8_t scancode = inb(0x60);

    // Check for Alt+1/2/3/4
    if (alt_pressed && scancode >= 0x02 && scancode <= 0x05) {
        int console = scancode - 0x02;  // 0-3
        screen_console_switch(console);
        return;
    }

    // ... normal key handling ...
}
```

### Switch Implementation

```c
void screen_console_switch(int console) {
    if (console < 0 || console >= MAX_CONSOLES) return;
    if (console == active_console) return;

    // Mark old console as inactive
    consoles[active_console].active = false;

    // Switch to new console
    active_console = console;
    consoles[console].active = true;
    consoles[console].dirty = true;

    // Redraw screen with new console's buffer
    screen_refresh_from_console(console);

    // Update hardware cursor
    update_cursor_position(consoles[console].cursor_x,
                          consoles[console].cursor_y);

    serial_write_string("[VC] Switched to console ");
    serial_write_dec(console + 1);
    serial_write_string("\n");
}
```

### Screen Refresh

```c
void screen_refresh_from_console(int idx) {
    virtual_console_t* vc = &consoles[idx];

    // Clear framebuffer
    fb_clear(get_bg_color(VGA_BLACK));

    // Render all cells from console's buffer
    for (int y = 0; y < screen_rows_value; y++) {
        for (int x = 0; x < screen_cols_value; x++) {
            cell_t* cell = &vc->cell_buffer[y * screen_cols_value + x];
            render_cell_at(x, y, cell);
        }
    }

    vc->dirty = false;
}
```

## Process-to-Console Mapping

### Foreground Process

Each console tracks its foreground process:

```c
void console_set_foreground(int console, pid_t pid) {
    if (console < 0 || console >= MAX_CONSOLES) return;
    consoles[console].foreground_pid = pid;
}

pid_t console_get_foreground(int console) {
    if (console < 0 || console >= MAX_CONSOLES) return -1;
    return consoles[console].foreground_pid;
}
```

### Keyboard Input Routing

```c
void keyboard_send_char(char c) {
    // Get foreground process of active console
    pid_t fg = console_get_foreground(active_console);
    if (fg <= 0) return;

    // Find the task
    task_t* t = task_get_by_pid(fg);
    if (!t) return;

    // Add to task's input buffer
    task_input_write(t, c);

    // Wake task if blocked on read
    if (t->state == TASK_BLOCKED_IO) {
        t->state = TASK_RUNNABLE;
    }
}
```

## Console Output

### Writing to Console

```c
void console_putchar(int console, char c) {
    if (console < 0 || console >= MAX_CONSOLES) return;

    virtual_console_t* vc = &consoles[console];

    // Handle control characters
    switch (c) {
    case '\n':
        vc->cursor_x = 0;
        vc->cursor_y++;
        break;
    case '\r':
        vc->cursor_x = 0;
        break;
    case '\t':
        vc->cursor_x = (vc->cursor_x + 8) & ~7;
        break;
    case '\b':
        if (vc->cursor_x > 0) vc->cursor_x--;
        break;
    default:
        if (c >= 32) {
            // Write to cell buffer
            int idx = vc->cursor_y * screen_cols_value + vc->cursor_x;
            vc->cell_buffer[idx].codepoint = c;
            vc->cell_buffer[idx].fg = vc->fg_color;
            vc->cell_buffer[idx].bg = vc->bg_color;
            vc->cursor_x++;
        }
    }

    // Handle line wrap
    if (vc->cursor_x >= screen_cols_value) {
        vc->cursor_x = 0;
        vc->cursor_y++;
    }

    // Handle scroll
    if (vc->cursor_y >= screen_rows_value - 1) {
        console_scroll(console);
        vc->cursor_y = screen_rows_value - 2;
    }

    // If this is the active console, render immediately
    if (vc->active) {
        render_cell_at(vc->cursor_x, vc->cursor_y,
                      &vc->cell_buffer[vc->cursor_y * screen_cols_value + vc->cursor_x]);
    } else {
        vc->dirty = true;
    }
}
```

### Per-Console Scrolling

```c
void console_scroll(int console) {
    virtual_console_t* vc = &consoles[console];

    // Move all lines up by one
    int cols = screen_cols_value;
    int rows = screen_rows_value - 1;  // Exclude status bar

    memmove(vc->cell_buffer,
            vc->cell_buffer + cols,
            cols * (rows - 1) * sizeof(cell_t));

    // Clear last line
    for (int x = 0; x < cols; x++) {
        int idx = (rows - 1) * cols + x;
        vc->cell_buffer[idx].codepoint = ' ';
        vc->cell_buffer[idx].fg = vc->fg_color;
        vc->cell_buffer[idx].bg = vc->bg_color;
    }

    if (vc->active) {
        screen_refresh_from_console(console);
    }
}
```

## ANSI Sequence Handling

Each console has its own ANSI parser state:

```c
void console_write(int console, const char* str, size_t len) {
    virtual_console_t* vc = &consoles[console];

    for (size_t i = 0; i < len; i++) {
        char c = str[i];

        switch (vc->parse_state) {
        case STATE_NORMAL:
            if (c == '\033') {
                vc->parse_state = STATE_ESC;
            } else {
                console_putchar(console, c);
            }
            break;

        case STATE_ESC:
            if (c == '[') {
                vc->parse_state = STATE_CSI;
                vc->csi_len = 0;
            } else {
                vc->parse_state = STATE_NORMAL;
            }
            break;

        case STATE_CSI:
            if (c >= 0x40 && c <= 0x7E) {
                // Final byte
                vc->csi_buffer[vc->csi_len] = '\0';
                console_execute_csi(console, vc->csi_buffer, c);
                vc->parse_state = STATE_NORMAL;
            } else if (vc->csi_len < sizeof(vc->csi_buffer) - 1) {
                vc->csi_buffer[vc->csi_len++] = c;
            }
            break;
        }
    }
}
```

## Status Bar Integration

The status bar shows the active console:

```c
void statusbar_update(void) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             " VC%d | %04d-%02d-%02d %02d:%02d | Up: %us | Mem: %uMB ",
             active_console + 1,
             dt.year, dt.month, dt.day, dt.hour, dt.minute,
             uptime_seconds,
             free_mb);

    // Render to bottom row
    for (int x = 0; x < screen_cols_value && buf[x]; x++) {
        screen_write_char_at(x, screen_rows_value - 1, buf[x],
                            VGA_WHITE | (VGA_BLUE << 4));
    }
}
```

## Spawning Shells on Each Console

### Init Configuration

```c
// In init.c (future enhancement)
void spawn_console_shells(void) {
    for (int i = 0; i < MAX_CONSOLES; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: set up console and run login
            console_set_foreground(i, getpid());

            char* const argv[] = {"/bin/login", NULL};
            execve("/bin/login", argv, NULL);
            _exit(127);
        }
    }
}
```

### Current Implementation

Currently VOS runs login on console 0 only. Other consoles are available but require manual shell spawning.

## Syscall Interface

### Console Syscalls

```c
// Get number of virtual consoles
int sys_console_count(void);

// Get currently active console
int sys_console_active(void);

// Switch to a console (requires appropriate privileges)
int sys_console_switch(int console);
```

### Kernel Implementation

```c
case SYS_CONSOLE_COUNT:
    frame->eax = MAX_CONSOLES;
    return frame;

case SYS_CONSOLE_ACTIVE:
    frame->eax = (uint32_t)screen_console_active();
    return frame;

case SYS_CONSOLE_SWITCH: {
    int console = (int)frame->ebx;
    // Only root or foreground process can switch
    if (!task_is_root() && get_current_task()->pid != console_get_foreground(active_console)) {
        frame->eax = (uint32_t)-EPERM;
        return frame;
    }
    screen_console_switch(console);
    frame->eax = 0;
    return frame;
}
```

## Memory Considerations

### Buffer Sizes

At 1920x1080 with 16x32 font:
- Screen: 120 columns × 33 rows = 3960 cells
- Cell size: ~8 bytes
- Per-console: ~32 KB
- 4 consoles: ~128 KB

### Scrollback Buffer (Optional)

```c
#define SCROLLBACK_LINES 1000

// Per-console scrollback
size_t scrollback_size = screen_cols_value * SCROLLBACK_LINES * sizeof(cell_t);
vc->scrollback_buffer = kmalloc(scrollback_size);
// 120 cols × 1000 lines × 8 bytes = ~960 KB per console
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Alt+1 | Switch to console 1 |
| Alt+2 | Switch to console 2 |
| Alt+3 | Switch to console 3 |
| Alt+4 | Switch to console 4 |
| Shift+PgUp | Scroll up (scrollback) |
| Shift+PgDn | Scroll down |

### Key Detection

```c
// In keyboard.c
static bool alt_pressed = false;
static bool shift_pressed = false;

void keyboard_handle_scancode(uint8_t scancode) {
    // Track modifier state
    if (scancode == 0x38) alt_pressed = true;       // Alt press
    if (scancode == 0xB8) alt_pressed = false;      // Alt release
    if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
    if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;

    // Check Alt+number combinations
    if (alt_pressed) {
        switch (scancode) {
        case 0x02: screen_console_switch(0); return;  // Alt+1
        case 0x03: screen_console_switch(1); return;  // Alt+2
        case 0x04: screen_console_switch(2); return;  // Alt+3
        case 0x05: screen_console_switch(3); return;  // Alt+4
        }
    }

    // ... continue with normal key handling ...
}
```

## Future Enhancements

1. **Auto-spawn shells** - Run login on all consoles at boot
2. **Named consoles** - Label consoles (e.g., "main", "logs")
3. **Copy/paste** - Transfer text between consoles
4. **Split view** - Show multiple consoles simultaneously
5. **Console resize** - Dynamic column/row adjustment

## Summary

Virtual consoles in VOS provide:

1. **Multiple independent terminals** with separate screen buffers
2. **Quick switching** via Alt+1/2/3/4 hotkeys
3. **Per-console state** including cursor, colors, and ANSI parser
4. **Process isolation** with foreground process tracking
5. **Scrollback support** for reviewing history

This enables a productive multi-tasking environment even without a window manager.

---

*Previous: [Chapter 39: User Management](39_users.md)*
*Next: [Chapter 41: Future Directions](32_future.md)*
