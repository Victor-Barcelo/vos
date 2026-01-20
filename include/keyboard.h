#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// Special key codes (negative values to avoid conflict with printable chars)
#define KEY_UP      (-1)
#define KEY_DOWN    (-2)
#define KEY_LEFT    (-3)
#define KEY_RIGHT   (-4)
#define KEY_HOME    (-5)
#define KEY_END     (-6)
#define KEY_PGUP    (-7)
#define KEY_PGDN    (-8)
#define KEY_DELETE  (-9)

// Initialize the keyboard
void keyboard_init(void);

// Keyboard interrupt handler (called from assembly)
void keyboard_handler(void);

// Check if a key is available in the buffer
bool keyboard_has_key(void);

// Get the next key from the buffer (blocking)
char keyboard_getchar(void);

// Try to get the next key from the buffer (non-blocking).
// Returns true and stores the key in *out on success.
bool keyboard_try_getchar(char* out);

// Get a line of input with command history (blocking)
void keyboard_getline(char* buffer, size_t max_len);

// Get a line of input with command history (explicit name)
void keyboard_getline_history(char* buffer, size_t max_len);

// Add a command to history manually
void keyboard_history_add(const char* cmd);

// Optional idle hook called while waiting for input (e.g. status bar refresh).
void keyboard_set_idle_hook(void (*hook)(void));

#endif
