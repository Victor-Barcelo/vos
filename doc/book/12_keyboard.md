# Chapter 12: Keyboard Driver

## PS/2 Controller

The PS/2 keyboard connects through the 8042 controller:

| Port | Read | Write |
|------|------|-------|
| 0x60 | Read data | Write data |
| 0x64 | Read status | Write command |

### Status Register Bits

| Bit | Meaning |
|-----|---------|
| 0 | Output buffer full (data ready to read) |
| 1 | Input buffer full (don't write yet) |
| 2 | System flag |
| 3 | Command/data (0=data, 1=command) |
| 5 | Auxiliary output buffer full (mouse) |
| 6 | Timeout error |
| 7 | Parity error |

## Scancodes

When a key is pressed, the keyboard sends a **scancode**. When released, it sends the same scancode with bit 7 set (OR 0x80).

### Scancode Set 1

VOS uses Scancode Set 1 (the default):

```
+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
| Esc |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |
| 01  | 02  | 03  | 04  | 05  | 06  | 07  | 08  | 09  | 0A  |
+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
|  0  |  -  |  =  | BS  | Tab |  Q  |  W  |  E  |  R  |  T  |
| 0B  | 0C  | 0D  | 0E  | 0F  | 10  | 11  | 12  | 13  | 14  |
+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
```

### Extended Scancodes

Some keys send a prefix byte (0xE0) followed by the scancode:

| Key | Scancodes |
|-----|-----------|
| Up Arrow | E0 48 |
| Down Arrow | E0 50 |
| Left Arrow | E0 4B |
| Right Arrow | E0 4D |
| Home | E0 47 |
| End | E0 4F |
| Page Up | E0 49 |
| Page Down | E0 51 |
| Insert | E0 52 |
| Delete | E0 53 |
| Right Alt (AltGr) | E0 38 |
| Right Ctrl | E0 1D |

## Keyboard Handler

```c
#define KEYBOARD_DATA_PORT 0x60

static bool extended_key = false;
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool altgr_pressed = false;
static bool caps_lock = false;

void keyboard_irq_handler(interrupt_frame_t *frame) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Check for extended key prefix
    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }

    bool released = scancode & 0x80;
    scancode &= 0x7F;

    if (extended_key) {
        extended_key = false;
        handle_extended_key(scancode, released);
        return;
    }

    // Handle modifier keys
    switch (scancode) {
        case 0x2A: case 0x36:  // Left/Right Shift
            shift_pressed = !released;
            return;
        case 0x1D:              // Left Ctrl
            ctrl_pressed = !released;
            return;
        case 0x38:              // Left Alt
            alt_pressed = !released;
            return;
        case 0x3A:              // Caps Lock
            if (!released) caps_lock = !caps_lock;
            return;
    }

    if (!released) {
        char c = translate_scancode(scancode);
        if (c) {
            buffer_push(c);
        }
    }
}
```

### Extended Key Handler

```c
static void handle_extended_key(uint8_t scancode, bool released) {
    if (released) {
        // Handle release of extended keys
        if (scancode == 0x38) {
            altgr_pressed = false;
        }
        return;
    }

    // Handle press of extended keys
    switch (scancode) {
        case 0x48: buffer_push(KEY_UP);    break;
        case 0x50: buffer_push(KEY_DOWN);  break;
        case 0x4B: buffer_push(KEY_LEFT);  break;
        case 0x4D: buffer_push(KEY_RIGHT); break;
        case 0x47: buffer_push(KEY_HOME);  break;
        case 0x4F: buffer_push(KEY_END);   break;
        case 0x49: buffer_push(KEY_PGUP);  break;
        case 0x51: buffer_push(KEY_PGDN);  break;
        case 0x52: buffer_push(KEY_INSERT);break;
        case 0x53: buffer_push(KEY_DELETE);break;
        case 0x38: altgr_pressed = true;   break;
    }
}
```

## Keyboard Layouts

### US Layout (Default)

```c
static const char us_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',','.','/', 0, '*',
    0, ' ', 0, ...
};

static const char us_shifted[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0, '*',
    0, ' ', 0, ...
};
```

### Spanish Layout

VOS includes Spanish keyboard support with AltGr:

```c
static const char es_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','\'','\xa1','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','`','+','\n',
    0, 'a','s','d','f','g','h','j','k','l','\xf1','\'','\xba',
    0, '\xe7','z','x','c','v','b','n','m',',','.','-', 0, ...
};

// AltGr combinations
static const char es_altgr[128] = {
    ['2'-'0'+2] = '@',   // AltGr+2 = @
    ['3'-'0'+2] = '#',   // AltGr+3 = #
    ['7'-'0'+2] = '{',   // AltGr+7 = {
    ['8'-'0'+2] = '[',   // AltGr+8 = [
    ['9'-'0'+2] = ']',   // AltGr+9 = ]
    ['0'-'0'+2] = '}',   // AltGr+0 = }
};
```

### Scancode Translation

```c
static char translate_scancode(uint8_t scancode) {
    char c = 0;

    // Check AltGr first
    if (altgr_pressed && es_altgr[scancode]) {
        return es_altgr[scancode];
    }

    // Get base character
    if (shift_pressed) {
        c = current_layout_shifted[scancode];
    } else {
        c = current_layout_normal[scancode];
    }

    // Apply caps lock to letters
    if (caps_lock && c >= 'a' && c <= 'z') {
        c = shift_pressed ? c : (c - 32);
    } else if (caps_lock && c >= 'A' && c <= 'Z') {
        c = shift_pressed ? (c + 32) : c;
    }

    return c;
}
```

## Input Buffer

VOS uses a circular buffer for keyboard input:

```c
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_start = 0;
static volatile size_t buffer_end = 0;

static void buffer_push(char c) {
    size_t next = (buffer_end + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_start) {  // Not full
        keyboard_buffer[buffer_end] = c;
        buffer_end = next;
    }
}

char keyboard_getchar(void) {
    while (buffer_start == buffer_end) {
        hlt();  // Wait for interrupt
    }

    char c = keyboard_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

bool keyboard_has_char(void) {
    return buffer_start != buffer_end;
}
```

## Line Input

For shell input, VOS provides line editing:

```c
void keyboard_getline(char *buffer, size_t max_len) {
    size_t pos = 0;
    size_t len = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            buffer[len] = '\0';
            screen_putchar('\n');
            keyboard_history_add(buffer);
            return;
        }

        if (c == '\b') {
            if (pos > 0) {
                // Delete character at pos-1
                memmove(&buffer[pos-1], &buffer[pos], len - pos);
                pos--;
                len--;
                redraw_line(buffer, len, pos);
            }
        }

        else if (c == KEY_LEFT && pos > 0) {
            pos--;
            update_cursor(pos);
        }

        else if (c == KEY_RIGHT && pos < len) {
            pos++;
            update_cursor(pos);
        }

        else if (c == KEY_UP) {
            // History previous
            const char *hist = history_prev();
            if (hist) {
                strcpy(buffer, hist);
                len = pos = strlen(buffer);
                redraw_line(buffer, len, pos);
            }
        }

        else if (c == KEY_DOWN) {
            // History next
            const char *hist = history_next();
            if (hist) {
                strcpy(buffer, hist);
                len = pos = strlen(buffer);
                redraw_line(buffer, len, pos);
            }
        }

        else if (c >= 32 && c < 127 && len < max_len - 1) {
            // Insert character at pos
            memmove(&buffer[pos+1], &buffer[pos], len - pos);
            buffer[pos] = c;
            pos++;
            len++;
            redraw_line(buffer, len, pos);
        }
    }
}
```

## Command History

```c
#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][256];
static int history_count = 0;
static int history_index = 0;

void keyboard_history_add(const char *cmd) {
    if (cmd[0] == '\0') return;

    // Don't add duplicates
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_SIZE;
        if (strcmp(history[last], cmd) == 0) return;
    }

    int idx = history_count % HISTORY_SIZE;
    strncpy(history[idx], cmd, 255);
    history[idx][255] = '\0';
    history_count++;
    history_index = history_count;
}

const char* history_prev(void) {
    if (history_index > 0 &&
        history_index > history_count - HISTORY_SIZE) {
        history_index--;
        return history[history_index % HISTORY_SIZE];
    }
    return NULL;
}

const char* history_next(void) {
    if (history_index < history_count - 1) {
        history_index++;
        return history[history_index % HISTORY_SIZE];
    }
    history_index = history_count;
    return "";
}
```

## Keyboard Syscalls

```c
// Read a character (blocking)
int32_t sys_key_read(void) {
    return keyboard_getchar();
}

// Check if key is currently pressed
int32_t sys_key_pressed(int key) {
    return key_state[key] ? 1 : 0;
}
```

## Summary

The VOS keyboard driver provides:

1. **PS/2 interrupt handling** for key events
2. **Scancode translation** with multiple layouts
3. **Modifier key tracking** (Shift, Ctrl, Alt, AltGr)
4. **Circular buffer** for input queuing
5. **Line editing** with cursor movement
6. **Command history** with up/down navigation
7. **Syscalls** for user programs

---

*Previous: [Chapter 11: Console Output](11_console.md)*
*Next: [Chapter 13: Mouse Driver](13_mouse.md)*
