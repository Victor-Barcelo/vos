# VOS Development Resources

This directory contains documentation for C libraries and resources that can be
used for development within VOS using the TCC compiler.

All libraries listed are:
- Written in pure C (compatible with TCC)
- Single-file or minimal dependencies
- Suitable for embedded/hobby OS use
- Free and open source

For detailed VOS documentation, see the [book/](../book/) directory.

---

## VOS System Reference

### [vos_capabilities.md](vos_capabilities.md)
Complete system reference including:
- **System specs:** Architecture, memory, disk limits
- **Syscalls:** All 71+ system calls with numbers and descriptions
- **Graphics:** Framebuffer API, olive.c, small3dlib
- **POSIX compliance:** ~45% coverage details
- **Available programs:** System utilities, editors, entertainment
- **Building:** TCC usage and cross-compilation

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
Networking stacks and utilities (for future VOS development):
- **TCP/IP:** uIP (<10KB), lwIP (full featured)
- **Serial:** SLIP protocol (works with VOS serial)
- **Protocols:** HTTP client, DNS, TFTP
- **Drivers:** NE2000, RTL8139, virtio-net requirements

### [scripting_languages.md](scripting_languages.md)
Embeddable interpreters:
- **Recommended:** Lua, wren, TinyScheme
- **Minimal:** MiniLisp, femtolisp, tinylisp
- **Forth:** zForth, pForth
- **BASIC:** MyBasic (VOS has built-in uBASIC)
- **Expression Evaluators:** TinyExpr

---

## Implementation Roadmaps

### [threading_roadmap.md](threading_roadmap.md)
Complete guide to adding threading support:
- **Phase 1:** User-space fibers (cooperative)
- **Phase 2:** Kernel thread support
- **Phase 3:** Synchronization primitives (mutex, condvar)
- **Phase 4:** Full pthreads API
- Reference implementations and complexity estimates

### [audio_implementation.md](audio_implementation.md)
Audio system implementation options:
- **PC Speaker:** Simple beeps and tones
- **Sound Blaster 16:** PCM audio with DMA
- **AC'97:** Modern codec support
- QEMU audio configuration
- Audio file format libraries

### [io_multiplexing.md](io_multiplexing.md)
I/O multiplexing implementation guide:
- Non-blocking I/O with O_NONBLOCK
- select() implementation
- poll() implementation
- VFS polling operations
- Wait queues for efficiency

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
| stb_image | `/usr/include/stb_image.h` | Image loading |
| jsmn | `/usr/include/jsmn.h` | JSON parsing |
| linenoise | (shell) | Line editing |
| newlib | `/usr/include/`, `/usr/lib/libc.a` | Standard C library |
| TCC | `/usr/bin/tcc` | Native C compiler |
| sbase | `/bin/*` | Unix utilities |
| ne | `/bin/ne` | Nice Editor |

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

## VOS Development Tips

### Using fork/exec
VOS now supports full POSIX fork/exec:
```c
pid_t pid = fork();
if (pid == 0) {
    // Child process
    execve("/bin/program", argv, envp);
    _exit(1);
} else {
    // Parent process
    waitpid(pid, &status, 0);
}
```

### Graphics Programming
Use the `sys_gfx_blit_rgba()` syscall for direct framebuffer access:
```c
#include <stdint.h>
extern int32_t syscall5(int, int, int, int, int, int);
#define SYS_GFX_BLIT_RGBA 75

uint32_t pixels[WIDTH * HEIGHT];
// ... fill pixels ...
syscall5(SYS_GFX_BLIT_RGBA, x, y, WIDTH, HEIGHT, (int)pixels);
```

### Cross-Compilation
Build programs from Linux:
```bash
make USER_PROGRAMS="myprogram"
```

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

---

## See Also

- [VOS Book](../book/00_index.md) - Complete VOS development guide
- [Chapter 34: Syscall Reference](../book/34_syscall_reference.md) - Full syscall documentation
- [Chapter 32: Future Enhancements](../book/32_future.md) - Roadmap and planned features

---

## File List

| File | Description |
|------|-------------|
| [README.md](README.md) | This index |
| [vos_capabilities.md](vos_capabilities.md) | System reference |
| [game_resources.md](game_resources.md) | Game development libraries |
| [system_libraries.md](system_libraries.md) | System utilities |
| [data_formats.md](data_formats.md) | File format libraries |
| [emulators.md](emulators.md) | Retro emulators |
| [networking.md](networking.md) | Network stacks |
| [scripting_languages.md](scripting_languages.md) | Embeddable interpreters |
| [threading_roadmap.md](threading_roadmap.md) | Threading implementation guide |
| [audio_implementation.md](audio_implementation.md) | Audio system guide |
| [io_multiplexing.md](io_multiplexing.md) | I/O multiplexing guide |
