#!/usr/bin/env python3
"""
Download and convert emoji PNGs to C arrays for VOS kernel embedding.
Uses OpenMoji 72x72 PNGs (CC BY-SA 4.0 license).
"""

import os
import sys
import struct
import urllib.request
from pathlib import Path

# Common emoji to include (codepoint, name)
EMOJI_LIST = [
    # Smileys
    (0x1F600, "grinning"),
    (0x1F601, "beaming"),
    (0x1F602, "joy"),
    (0x1F603, "smiley"),
    (0x1F604, "smile"),
    (0x1F605, "sweat_smile"),
    (0x1F606, "laughing"),
    (0x1F609, "wink"),
    (0x1F60A, "blush"),
    (0x1F60D, "heart_eyes"),
    (0x1F60E, "sunglasses"),
    (0x1F60F, "smirk"),
    (0x1F610, "neutral"),
    (0x1F612, "unamused"),
    (0x1F614, "pensive"),
    (0x1F616, "confounded"),
    (0x1F618, "kissing_heart"),
    (0x1F61B, "tongue"),
    (0x1F61C, "wink_tongue"),
    (0x1F61D, "squint_tongue"),
    (0x1F61E, "disappointed"),
    (0x1F620, "angry"),
    (0x1F621, "pouting"),
    (0x1F622, "cry"),
    (0x1F623, "persevere"),
    (0x1F624, "triumph"),
    (0x1F625, "relieved_sweat"),
    (0x1F628, "fearful"),
    (0x1F629, "weary"),
    (0x1F62A, "sleepy"),
    (0x1F62B, "tired"),
    (0x1F62D, "sob"),
    (0x1F62E, "open_mouth"),
    (0x1F62F, "hushed"),
    (0x1F630, "cold_sweat"),
    (0x1F631, "scream"),
    (0x1F632, "astonished"),
    (0x1F633, "flushed"),
    (0x1F634, "sleeping"),
    (0x1F635, "dizzy"),
    (0x1F637, "mask"),
    (0x1F642, "slight_smile"),
    (0x1F643, "upside_down"),
    (0x1F644, "rolling_eyes"),
    # Gestures
    (0x1F44D, "thumbsup"),
    (0x1F44E, "thumbsdown"),
    (0x1F44F, "clap"),
    (0x1F44B, "wave"),
    (0x1F44C, "ok_hand"),
    (0x1F64F, "pray"),
    (0x270C, "victory"),
    (0x2764, "heart"),
    # Objects
    (0x1F525, "fire"),
    (0x2B50, "star"),
    (0x1F31F, "star2"),
    (0x1F4AF, "100"),
    (0x1F389, "tada"),
    (0x1F38A, "confetti"),
    (0x1F381, "gift"),
    (0x1F3C6, "trophy"),
    # Animals
    (0x1F436, "dog"),
    (0x1F431, "cat"),
    (0x1F42D, "mouse"),
    (0x1F430, "rabbit"),
    (0x1F43B, "bear"),
    (0x1F437, "pig"),
    (0x1F438, "frog"),
    (0x1F412, "monkey"),
    (0x1F414, "chicken"),
    (0x1F427, "penguin"),
    # Food
    (0x1F354, "hamburger"),
    (0x1F355, "pizza"),
    (0x1F382, "birthday"),
    (0x2615, "coffee"),
    (0x1F37A, "beer"),
    # Weather/Nature
    (0x2600, "sunny"),
    (0x2601, "cloud"),
    (0x2614, "umbrella"),
    (0x26A1, "zap"),
    (0x1F308, "rainbow"),
    # Symbols
    (0x2705, "check"),
    (0x274C, "x"),
    (0x2753, "question"),
    (0x2757, "exclamation"),
    (0x1F4A1, "bulb"),
    (0x1F4AC, "speech"),
    (0x1F4AD, "thought"),
    (0x1F6A8, "rotating_light"),
    (0x26A0, "warning"),
]

EMOJI_SIZE = 32  # Scale to 32x32 for better visibility in terminal
OPENMOJI_BASE = "https://raw.githubusercontent.com/hfg-gmuend/openmoji/master/color/72x72"

def codepoint_to_filename(cp):
    """Convert codepoint to OpenMoji filename format."""
    return f"{cp:04X}.png"

def download_emoji(cp, output_dir):
    """Download emoji PNG from OpenMoji."""
    filename = codepoint_to_filename(cp)
    url = f"{OPENMOJI_BASE}/{filename}"
    output_path = output_dir / filename

    if output_path.exists():
        return output_path

    try:
        print(f"Downloading U+{cp:04X}...", end=" ", flush=True)
        urllib.request.urlretrieve(url, output_path)
        print("OK")
        return output_path
    except Exception as e:
        print(f"FAILED: {e}")
        return None

def png_to_rgba(png_path, target_size=16):
    """Convert PNG to raw RGBA using simple decoder or PIL."""
    try:
        from PIL import Image
        img = Image.open(png_path).convert('RGBA')
        img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
        return list(img.getdata())
    except ImportError:
        print("PIL not available, using placeholder")
        # Return a placeholder colored square
        return [(255, 200, 0, 255)] * (target_size * target_size)

def generate_c_file(emoji_data, output_path):
    """Generate C source file with embedded emoji data."""
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated emoji sprite data - DO NOT EDIT */\n")
        f.write("/* License: CC BY-SA 4.0 (OpenMoji) */\n\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")

        f.write(f"#define EMOJI_SIZE {EMOJI_SIZE}\n")
        f.write(f"#define EMOJI_COUNT {len(emoji_data)}\n\n")

        # Write each emoji's pixel data
        for cp, name, pixels in emoji_data:
            f.write(f"/* U+{cp:04X} {name} */\n")
            f.write(f"static const uint32_t emoji_{cp:04X}_data[{EMOJI_SIZE}*{EMOJI_SIZE}] = {{\n")
            for y in range(EMOJI_SIZE):
                f.write("    ")
                for x in range(EMOJI_SIZE):
                    r, g, b, a = pixels[y * EMOJI_SIZE + x]
                    # Pack as ARGB (alpha in high byte)
                    pixel = (a << 24) | (r << 16) | (g << 8) | b
                    f.write(f"0x{pixel:08X},")
                f.write("\n")
            f.write("};\n\n")

        # Write lookup table
        f.write("typedef struct {\n")
        f.write("    uint32_t codepoint;\n")
        f.write("    const uint32_t* data;\n")
        f.write("} emoji_entry_t;\n\n")

        f.write("static const emoji_entry_t emoji_table[] = {\n")
        for cp, name, _ in emoji_data:
            f.write(f"    {{ 0x{cp:04X}, emoji_{cp:04X}_data }}, /* {name} */\n")
        f.write("};\n\n")

        # Write lookup function
        f.write("const uint32_t* emoji_lookup(uint32_t codepoint) {\n")
        f.write("    for (size_t i = 0; i < EMOJI_COUNT; i++) {\n")
        f.write("        if (emoji_table[i].codepoint == codepoint) {\n")
        f.write("            return emoji_table[i].data;\n")
        f.write("        }\n")
        f.write("    }\n")
        f.write("    return NULL;\n")
        f.write("}\n\n")

        # Write emoji detection function
        f.write("int emoji_is_emoji(uint32_t codepoint) {\n")
        f.write("    /* Miscellaneous Symbols */\n")
        f.write("    if (codepoint >= 0x2600 && codepoint <= 0x26FF) return 1;\n")
        f.write("    /* Dingbats */\n")
        f.write("    if (codepoint >= 0x2700 && codepoint <= 0x27BF) return 1;\n")
        f.write("    /* Emoticons and beyond */\n")
        f.write("    if (codepoint >= 0x1F300 && codepoint <= 0x1FFFF) return 1;\n")
        f.write("    return 0;\n")
        f.write("}\n\n")

        f.write("int emoji_get_size(void) {\n")
        f.write(f"    return {EMOJI_SIZE};\n")
        f.write("}\n")

    print(f"Generated {output_path}")

def generate_header(output_path):
    """Generate C header file."""
    with open(output_path, 'w') as f:
        f.write("/* Auto-generated emoji header - DO NOT EDIT */\n")
        f.write("#ifndef EMOJI_H\n")
        f.write("#define EMOJI_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define EMOJI_SIZE {EMOJI_SIZE}\n\n")
        f.write("/* Look up emoji pixel data by codepoint. Returns NULL if not found. */\n")
        f.write("const uint32_t* emoji_lookup(uint32_t codepoint);\n\n")
        f.write("/* Check if codepoint is in emoji range. */\n")
        f.write("int emoji_is_emoji(uint32_t codepoint);\n\n")
        f.write("/* Get emoji sprite size (width/height in pixels). */\n")
        f.write("int emoji_get_size(void);\n\n")
        f.write("#endif /* EMOJI_H */\n")

    print(f"Generated {output_path}")

def main():
    script_dir = Path(__file__).parent.parent
    emoji_dir = script_dir / "third_party" / "emoji" / "png"
    emoji_dir.mkdir(parents=True, exist_ok=True)

    kernel_dir = script_dir / "kernel"

    print(f"Downloading {len(EMOJI_LIST)} emoji...")

    emoji_data = []
    for cp, name in EMOJI_LIST:
        png_path = download_emoji(cp, emoji_dir)
        if png_path:
            pixels = png_to_rgba(png_path, EMOJI_SIZE)
            emoji_data.append((cp, name, pixels))

    print(f"\nSuccessfully processed {len(emoji_data)} emoji")

    generate_c_file(emoji_data, kernel_dir / "emoji_data.c")
    generate_header(script_dir / "include" / "emoji.h")

    print("\nDone! Add kernel/emoji_data.c to your Makefile.")

if __name__ == "__main__":
    main()
