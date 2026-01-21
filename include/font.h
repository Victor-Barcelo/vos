#ifndef FONT_H
#define FONT_H

#include "types.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t row_bytes;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    const uint8_t* glyphs;
    // Backing PSF2 blob (so higher layers can access the Unicode table).
    const uint8_t* data;
    uint32_t data_len;
    uint32_t headersize;
    uint32_t flags;
} font_t;

bool font_psf2_parse(const uint8_t* data, uint32_t len, font_t* out);

#endif
