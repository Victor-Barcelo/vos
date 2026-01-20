#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"

// Minimal full-screen editor (nano-like) for /ram files.
// Returns true if the file was saved at least once during the session.
bool editor_nano(const char* abs_path);

#endif
