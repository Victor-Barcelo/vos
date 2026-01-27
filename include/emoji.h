/* Auto-generated emoji header - DO NOT EDIT */
#ifndef EMOJI_H
#define EMOJI_H

#include "types.h"

#define EMOJI_SIZE 32

/* Look up emoji pixel data by codepoint. Returns NULL if not found. */
const uint32_t* emoji_lookup(uint32_t codepoint);

/* Check if codepoint is in emoji range. */
int emoji_is_emoji(uint32_t codepoint);

/* Get emoji sprite size (width/height in pixels). */
int emoji_get_size(void);

#endif /* EMOJI_H */
