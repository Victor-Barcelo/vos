#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// Initialize the keyboard
void keyboard_init(void);

// Keyboard interrupt handler (called from assembly)
void keyboard_handler(void);

// Check if a key is available in the buffer
bool keyboard_has_key(void);

// Get the next key from the buffer (blocking)
char keyboard_getchar(void);

// Get a line of input (blocking)
void keyboard_getline(char* buffer, size_t max_len);

#endif
