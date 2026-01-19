#ifndef STDIO_H
#define STDIO_H

#include "screen.h"

// Simple printf replacement for uBASIC
// Supports: %s (string), %d (integer), %c (char)
void basic_printf(const char* fmt, ...);

#define printf basic_printf

#endif
