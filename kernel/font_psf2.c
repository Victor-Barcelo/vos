#include "font.h"

#define PSF2_MAGIC 0x864AB572u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t length;
    uint32_t charsize;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

bool font_psf2_parse(const uint8_t* data, uint32_t len, font_t* out) {
    if (!data || !out) {
        return false;
    }
    if (len < (uint32_t)sizeof(psf2_header_t)) {
        return false;
    }

    const psf2_header_t* hdr = (const psf2_header_t*)data;
    if (hdr->magic != PSF2_MAGIC) {
        return false;
    }
    if (hdr->headersize < (uint32_t)sizeof(psf2_header_t)) {
        return false;
    }
    if (hdr->headersize > len) {
        return false;
    }
    if (hdr->width == 0 || hdr->height == 0) {
        return false;
    }
    if (hdr->length == 0 || hdr->charsize == 0) {
        return false;
    }

    uint32_t row_bytes = (hdr->width + 7u) / 8u;
    if (row_bytes == 0) {
        return false;
    }

    uint32_t min_bytes_per_glyph = row_bytes * hdr->height;
    if (hdr->charsize < min_bytes_per_glyph) {
        return false;
    }

    uint32_t available = len - hdr->headersize;
    uint32_t max_glyphs = available / hdr->charsize;
    if (hdr->length > max_glyphs) {
        return false;
    }

    out->width = hdr->width;
    out->height = hdr->height;
    out->row_bytes = row_bytes;
    out->glyph_count = hdr->length;
    out->bytes_per_glyph = hdr->charsize;
    out->glyphs = data + hdr->headersize;
    return true;
}

