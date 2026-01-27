# stb_textedit.h

Multi-line text editing widget implementation.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_textedit.h
- **Version**: 1.14
- **Author**: Sean Barrett
- **Sponsor**: RAD Game Tools

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_textedit implements the core functionality of a multi-line text-editing widget. You provide display, word-wrapping, and low-level string insertion/deletion, and stb_textedit maps user inputs into insertions, deletions, cursor position updates, selection state changes, and undo state management.

The library is designed for games and custom applications that need to build their own widgets. It is NOT recommended for editing large texts due to limited performance scaling and undo capacity.

## Features

- **Multi-line Support**: Full multi-line editing with single-line mode option
- **Cursor Navigation**: Arrow keys, home/end, page up/down, word movement
- **Selection**: Click, drag, shift+arrow key selection
- **Undo/Redo**: Configurable undo buffer with limited history
- **Insert/Overwrite Mode**: Per-textfield insert mode toggle
- **No Allocations**: Zero runtime memory allocations
- **IMGUI Compatible**: Designed for immediate-mode GUI systems
- **Windows-like Behavior**: Non-trivial behaviors modeled after Windows text controls

## API Reference

### Main Types

```c
// State for each text editing instance
typedef struct {
    int cursor;              // Cursor position in string
    int select_start;        // Selection start point
    int select_end;          // Selection end point
    unsigned char insert_mode;  // Insert vs overwrite
    int row_count_per_page;  // For page up/down
    // ... private fields ...
    StbUndoState undostate;
} STB_TexteditState;

// Layout information for a row
typedef struct {
    float x0, x1;            // Start/end x coordinates
    float baseline_y_delta;  // Position relative to previous row
    float ymin, ymax;        // Row height above/below baseline
    int num_chars;           // Characters in this row
} StbTexteditRow;
```

### Functions

```c
// Initialize state for a new text field
void stb_textedit_initialize_state(STB_TexteditState *state, int is_single_line);

// Handle mouse click (sets cursor, resets selection)
void stb_textedit_click(STB_TEXTEDIT_STRING *str, STB_TexteditState *state,
                        float x, float y);

// Handle mouse drag (extends selection)
void stb_textedit_drag(STB_TEXTEDIT_STRING *str, STB_TexteditState *state,
                       float x, float y);

// Delete current selection, returns 1 if there was a selection
int stb_textedit_cut(STB_TEXTEDIT_STRING *str, STB_TexteditState *state);

// Paste text at cursor or over selection
int stb_textedit_paste(STB_TEXTEDIT_STRING *str, STB_TexteditState *state,
                       STB_TEXTEDIT_CHARTYPE const *text, int len);

// Process keyboard input
void stb_textedit_key(STB_TEXTEDIT_STRING *str, STB_TexteditState *state,
                      STB_TEXTEDIT_KEYTYPE key);
```

### Required User Definitions

You MUST define these before including with `STB_TEXTEDIT_IMPLEMENTATION`:

```c
// Your string object type
#define STB_TEXTEDIT_STRING        MyStringType

// Get string length
#define STB_TEXTEDIT_STRINGLEN(obj)  my_strlen(obj)

// Get character at position
#define STB_TEXTEDIT_GETCHAR(obj,i)  my_getchar(obj, i)

// Get character width for positioning
#define STB_TEXTEDIT_GETWIDTH(obj,n,i)  my_getwidth(obj, n, i)

// Compute row layout
#define STB_TEXTEDIT_LAYOUTROW(r,obj,n)  my_layoutrow(r, obj, n)

// Delete characters
#define STB_TEXTEDIT_DELETECHARS(obj,i,n)  my_delete(obj, i, n)

// Insert characters
#define STB_TEXTEDIT_INSERTCHARS(obj,i,c,n)  my_insert(obj, i, c, n)

// Newline character
#define STB_TEXTEDIT_NEWLINE  '\n'

// Convert key to insertable character (-1 if not insertable)
#define STB_TEXTEDIT_KEYTOTEXT(k)  my_keytotext(k)

// Keyboard constants
#define STB_TEXTEDIT_K_SHIFT      0x10000
#define STB_TEXTEDIT_K_LEFT       (KEY_LEFT | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_RIGHT      (KEY_RIGHT | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_UP         (KEY_UP | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_DOWN       (KEY_DOWN | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_DELETE     (KEY_DELETE | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_BACKSPACE  (KEY_BACKSPACE | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_UNDO       (KEY_Z | CTRL_BIT | KEYDOWN_BIT)
#define STB_TEXTEDIT_K_REDO       (KEY_Y | CTRL_BIT | KEYDOWN_BIT)
// ... and more
```

### Optional Definitions

```c
// Character type (default: int)
#define STB_TEXTEDIT_CHARTYPE     wchar_t

// Position type (default: int)
#define STB_TEXTEDIT_POSITIONTYPE short

// Undo buffer sizes (defaults shown)
#define STB_TEXTEDIT_UNDOSTATECOUNT  99
#define STB_TEXTEDIT_UNDOCHARCOUNT   999

// Page navigation
#define STB_TEXTEDIT_K_PGUP       ...
#define STB_TEXTEDIT_K_PGDOWN     ...

// Word movement (requires STB_TEXTEDIT_IS_SPACE)
#define STB_TEXTEDIT_K_WORDLEFT   ...
#define STB_TEXTEDIT_K_WORDRIGHT  ...
#define STB_TEXTEDIT_IS_SPACE(ch) isspace(ch)

// Secondary keys (e.g., for macOS)
#define STB_TEXTEDIT_K_LINESTART2 ...
#define STB_TEXTEDIT_K_LINEEND2   ...
```

## Usage Example

```c
// Header mode - just get types
#define STB_TEXTEDIT_CHARTYPE char
#include "stb_textedit.h"

// In ONE implementation file:
#define STB_TEXTEDIT_CHARTYPE       char
#define STB_TEXTEDIT_STRING         MyString
#define STB_TEXTEDIT_STRINGLEN(s)   ((s)->len)
#define STB_TEXTEDIT_GETCHAR(s,i)   ((s)->data[i])
#define STB_TEXTEDIT_GETWIDTH(s,n,i) (8.0f)  // fixed-width font
#define STB_TEXTEDIT_NEWLINE        '\n'
#define STB_TEXTEDIT_KEYTOTEXT(k)   ((k) < 256 ? (k) : -1)

// Layout callback
#define STB_TEXTEDIT_LAYOUTROW(r,s,n) do { \
    (r)->x0 = 0; \
    (r)->x1 = (s)->len * 8.0f; \
    (r)->baseline_y_delta = 16.0f; \
    (r)->ymin = 0; \
    (r)->ymax = 16.0f; \
    (r)->num_chars = (s)->len - (n); \
} while(0)

#define STB_TEXTEDIT_DELETECHARS(s,i,n) memmove((s)->data+i, (s)->data+i+n, (s)->len-i-n), (s)->len-=n
#define STB_TEXTEDIT_INSERTCHARS(s,i,c,n) (memmove((s)->data+i+n,(s)->data+i,(s)->len-i), memcpy((s)->data+i,c,n), (s)->len+=n, 1)

// Key definitions
#define STB_TEXTEDIT_K_SHIFT      0x10000
#define STB_TEXTEDIT_K_LEFT       0x100
#define STB_TEXTEDIT_K_RIGHT      0x101
#define STB_TEXTEDIT_K_UP         0x102
#define STB_TEXTEDIT_K_DOWN       0x103
#define STB_TEXTEDIT_K_LINESTART  0x104
#define STB_TEXTEDIT_K_LINEEND    0x105
#define STB_TEXTEDIT_K_TEXTSTART  0x106
#define STB_TEXTEDIT_K_TEXTEND    0x107
#define STB_TEXTEDIT_K_DELETE     0x108
#define STB_TEXTEDIT_K_BACKSPACE  0x109
#define STB_TEXTEDIT_K_UNDO       0x10A
#define STB_TEXTEDIT_K_REDO       0x10B

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

// Usage
typedef struct { char data[1024]; int len; } MyString;
MyString text = { "Hello", 5 };
STB_TexteditState state;

stb_textedit_initialize_state(&state, 0);  // 0 = multi-line
stb_textedit_click(&text, &state, 0, 0);
stb_textedit_key(&text, &state, 'W');      // Insert 'W'
stb_textedit_key(&text, &state, STB_TEXTEDIT_K_LEFT | STB_TEXTEDIT_K_SHIFT);  // Select left
```

## Memory Usage

Undo system memory (in bytes):
```
[4 + 3 * sizeof(STB_TEXTEDIT_POSITIONTYPE)] * STB_TEXTEDIT_UNDOSTATECOUNT
+ sizeof(STB_TEXTEDIT_CHARTYPE) * STB_TEXTEDIT_UNDOCHARCOUNT
```

With defaults (POSITIONTYPE=int, CHARTYPE=int, UNDOSTATECOUNT=99, UNDOCHARCOUNT=999):
~5.5KB per text field

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<string.h>` - for memmove (can override with STB_TEXTEDIT_memmove)

2. **No Other Dependencies**: Pure C, no platform-specific code

3. **Customizable memmove**:
   ```c
   #define STB_TEXTEDIT_memmove my_memmove
   ```

4. **TCC Compatible**: All features work with TCC

5. **No Floating Point**: Layout uses float for coordinates but no complex math

6. **Static Functions Only**: Implementation is entirely static functions

### Reducing Memory for Embedded Use

```c
#define STB_TEXTEDIT_CHARTYPE      char       // 1 byte instead of 4
#define STB_TEXTEDIT_POSITIONTYPE  short      // 2 bytes instead of 4
#define STB_TEXTEDIT_UNDOSTATECOUNT 16        // Fewer undo levels
#define STB_TEXTEDIT_UNDOCHARCOUNT  256       // Smaller undo buffer
```

## Limitations

- Not designed for large texts (performance doesn't scale)
- Limited undo capacity
- Single-threaded
- No built-in copy/paste (you must implement clipboard access)
- No automatic word-wrap (you provide layout)

## Dependencies

- `<string.h>` - for memmove (optional, can override)
