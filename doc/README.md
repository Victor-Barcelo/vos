# VOS Development Resources

This directory contains documentation for C libraries and resources that can be
used for development within VOS using the TCC compiler.

All libraries listed are:
- Written in pure C (compatible with TCC)
- Single-file or minimal dependencies
- Suitable for embedded/hobby OS use
- Free and open source

---

## Documentation Index

### [game_resources.md](game_resources.md)
Game development libraries including:
- **Math:** linmath.h, HandmadeMath, fixed-point math
- **Physics:** Chipmunk2D (2D), tinyphysicsengine (3D)
- **Collision:** cute_c2.h
- **Pathfinding:** A*, Dijkstra maps, flow fields
- **Random/Noise:** PCG, xorshift, stb_perlin, cellular automata
- **Procedural Generation:** BSP dungeons, terrain generation
- **AI:** Finite state machines, behavior trees, boids flocking
- **Data Structures:** stb_ds.h, entity component systems

### [system_libraries.md](system_libraries.md)
System utilities and general programming:
- **Data Structures:** stb_ds.h, uthash, queues/lists
- **Memory Management:** TLSF allocator, memory pools, arenas
- **Strings:** sds, stb_sprintf, string builders
- **Parsing:** cJSON, minIni, inih, yxml
- **Compression:** miniz, lz4, heatshrink
- **Cryptography:** TweetNaCl, xxHash
- **Command Line:** getopt, optparse, argtable3
- **Logging:** log.c
- **Testing:** utest.h, munit, MinUnit
- **Regular Expressions:** tiny-regex-c
- **Coroutines:** Protothreads
- **File System:** tinydir, cwalk

### [data_formats.md](data_formats.md)
File format parsers and writers:
- **Images:** stb_image (load), stb_image_write (save), QOI, upng
- **Audio:** dr_wav, dr_mp3, dr_flac, stb_vorbis
- **Fonts:** stb_truetype, schrift, BMFont
- **3D Models:** tinyobjloader-c (OBJ), cgltf (glTF)
- **Archives:** miniz (ZIP), microtar (TAR)
- **Configuration:** cJSON, TOML, CSV
- **Maps:** cute_tiled.h (Tiled editor)

### [emulators.md](emulators.md)
Portable emulators for retro systems:
- **Easy:** CHIP-8, Peanut-GB (Game Boy), LiteNES (NES)
- **Moderate:** Walnut-CGB (GBC), 8086tiny (DOS)
- **CPU Cores:** Z80, 6502, 68000
- Integration templates and ROM sources

### [networking.md](networking.md)
Networking stacks and utilities:
- **TCP/IP:** uIP (<10KB), lwIP (full featured)
- **Serial:** SLIP protocol (works with VOS serial)
- **Protocols:** HTTP client, DNS, TFTP
- **Drivers:** NE2000, RTL8139, virtio-net requirements

### [scripting_languages.md](scripting_languages.md)
Embeddable interpreters:
- **Recommended:** Lua, wren, TinyScheme
- **Minimal:** MiniLisp, femtolisp, tinylisp
- **Forth:** zForth, pForth
- **BASIC:** MyBasic
- **Expression Evaluators:** TinyExpr

---

## Quick Reference: Best Libraries by Task

| Task | Library | Size |
|------|---------|------|
| Dynamic arrays | stb_ds.h | Single header |
| Hash maps | stb_ds.h or uthash | Single header |
| JSON | cJSON | 2 files |
| INI files | minIni | 2 files |
| Load PNG/JPG | stb_image.h | Single header |
| Save PNG/BMP | stb_image_write.h | Single header |
| TrueType fonts | stb_truetype.h | Single header |
| 2D collision | cute_c2.h | Single header |
| 2D physics | Chipmunk2D | ~15 files |
| 3D physics | tinyphysicsengine | Single header |
| Noise/terrain | stb_perlin.h | Single header |
| Random numbers | PCG | ~20 lines |
| ZIP files | miniz | Single header |
| Compression | lz4 or miniz | 1-2 files |
| Scripting | Lua | ~15 files |
| Expression eval | TinyExpr | 2 files |
| Unit testing | utest.h | Single header |
| Logging | log.c | 2 files |
| Game Boy emulator | Peanut-GB | Single header |
| NES emulator | LiteNES | ~3000 lines |

---

## Already Integrated in VOS

| Library | Location | Purpose |
|---------|----------|---------|
| olive.c | `/usr/include/olive.h` | 2D software rendering |
| small3dlib | `/usr/include/small3dlib.h` | 3D software rendering |
| newlib | `/usr/include/`, `/usr/lib/libc.a` | Standard C library |
| TCC | `/usr/bin/tcc` | Native C compiler |

---

## Suggested Directory Structure

When adding libraries to VOS sysroot:

```
/usr/include/
├── olive.h              # Already present
├── small3dlib.h         # Already present
├── syscall.h            # Already present
├── stb/
│   ├── stb_ds.h
│   ├── stb_image.h
│   ├── stb_image_write.h
│   ├── stb_perlin.h
│   ├── stb_sprintf.h
│   └── stb_truetype.h
├── cute/
│   ├── cute_c2.h
│   └── cute_tiled.h
├── lua/
│   ├── lua.h
│   ├── lauxlib.h
│   └── lualib.h
├── cjson/
│   └── cJSON.h
├── linmath.h
├── pcg.h
└── tinyexpr.h

/usr/lib/
├── libc.a               # Already present
├── libm.a               # Already present
├── libolive.a           # Already present
├── liblua.a
└── libchipmunk.a
```

---

## Integration Checklist

When adding a library to VOS:

1. **Check compatibility:**
   - [ ] Pure C (no C++)
   - [ ] No inline assembly (or x86 compatible)
   - [ ] No OS-specific code (or easily replaceable)
   - [ ] No floating point (or VOS FPU works)

2. **Build with TCC:**
   ```bash
   tcc -c library.c -o library.o
   tcc -ar rcs liblibrary.a library.o
   ```

3. **Test in VOS:**
   ```bash
   # In VOS shell
   tcc -o test test.c -llibrary
   ./test
   ```

4. **Add to sysroot:**
   - Copy headers to `/usr/include/`
   - Copy library to `/usr/lib/`
   - Update `install_sysroot.sh`

---

## Contributing

To add a new library to this documentation:

1. Verify it works with TCC
2. Test on VOS (or document any issues)
3. Add to appropriate .md file
4. Include:
   - URL and license
   - Size/complexity
   - Basic usage example
   - Any VOS-specific notes
