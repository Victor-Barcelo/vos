# VOS System Capabilities

Technical reference for VOS's current features and limitations.

---

## System Specifications

| Component | Specification |
|-----------|---------------|
| **Architecture** | x86-32 (i686) |
| **Max RAM** | 4 GB (32-bit addressing) |
| **Max Disk** | 128 GB (28-bit LBA), 2-4 GB per partition (FAT16) |
| **Graphics** | Software framebuffer (VESA/VBE) |
| **Input** | PS/2 Keyboard |
| **Serial** | COM1 at 115200 baud |
| **Compiler** | TCC (Tiny C Compiler) |
| **C Library** | Newlib |

---

## Syscalls Available

VOS provides 58 syscalls. Key ones for application development:

### File I/O
| Syscall | Description |
|---------|-------------|
| `sys_open(path, flags)` | Open file, returns fd |
| `sys_close(fd)` | Close file descriptor |
| `sys_read(fd, buf, len)` | Read from fd |
| `sys_write(fd, buf, len)` | Write to fd |
| `sys_lseek(fd, off, whence)` | Seek in file |
| `sys_stat(path, buf)` | Get file info |
| `sys_unlink(path)` | Delete file |
| `sys_mkdir(path)` | Create directory |
| `sys_rmdir(path)` | Remove directory |
| `sys_getcwd(buf, len)` | Get working directory |
| `sys_chdir(path)` | Change directory |
| `sys_opendir(path)` | Open directory for reading |
| `sys_readdir(fd, entry)` | Read directory entry |

### Process Management
| Syscall | Description |
|---------|-------------|
| `sys_spawn(path, args)` | Start new process |
| `sys_exit(code)` | Exit current process |
| `sys_wait(pid, status)` | Wait for child process |
| `sys_getpid()` | Get process ID |
| `sys_kill(pid, sig)` | Send signal to process |

### Memory
| Syscall | Description |
|---------|-------------|
| `sys_sbrk(incr)` | Extend heap |
| `sys_mmap(...)` | Map memory |
| `sys_munmap(addr, len)` | Unmap memory |

### Graphics
| Syscall | Description |
|---------|-------------|
| `sys_gfx_mode(w, h, bpp)` | Set graphics mode |
| `sys_gfx_info(info)` | Get framebuffer info |
| `sys_gfx_pset(x, y, color)` | Set pixel |
| `sys_gfx_blit_rgba(buf, x, y, w, h)` | Blit RGBA buffer |
| `sys_gfx_flip()` | Swap buffers |

### Input
| Syscall | Description |
|---------|-------------|
| `sys_key_read()` | Read keyboard scancode |
| `sys_key_pressed(key)` | Check if key pressed |

### Time
| Syscall | Description |
|---------|-------------|
| `sys_time()` | Get Unix timestamp |
| `sys_gettimeofday(tv)` | Get time with microseconds |
| `sys_nanosleep(req, rem)` | Sleep for duration |

### Signals
| Syscall | Description |
|---------|-------------|
| `sys_signal(sig, handler)` | Set signal handler |
| `sys_sigaction(...)` | Advanced signal handling |

### Terminal
| Syscall | Description |
|---------|-------------|
| `sys_ioctl(fd, req, arg)` | Device control |
| `sys_tcgetattr(fd, termios)` | Get terminal attributes |
| `sys_tcsetattr(fd, act, termios)` | Set terminal attributes |

---

## Graphics Capabilities

### Framebuffer
- VESA/VBE linear framebuffer
- Common resolutions: 640x480, 800x600, 1024x768
- 32-bit color (ARGB8888)

### Available Libraries
| Library | Type | Features |
|---------|------|----------|
| **olive.c** | 2D | Lines, rects, circles, triangles, sprites, text |
| **small3dlib** | 3D | Wireframe, flat shading, integer math only |

### Performance Guidelines
- Software rendering is CPU-bound
- Lower resolution = faster rendering
- Avoid per-pixel operations in inner loops
- Use dirty rectangles when possible
- Pre-calculate lookup tables (sin/cos, etc.)

---

## Memory Layout

```
0x00000000 - 0x000FFFFF  Reserved (BIOS, legacy)
0x00100000 - ...         Kernel code and data
...        - 0xBFFFFFFF  User space (~3 GB)
0xC0000000 - 0xCFFFFFFF  Kernel mapped
0xD0000000 - 0xEFFFFFFF  Kernel heap
0xF0000000 - 0xFFFFFFFF  Kernel stacks
```

### User Process Memory
- Text (code): starts at 0x08048000
- Data/BSS: after text
- Heap: grows up from data end (via sbrk/mmap)
- Stack: grows down from ~0xBFFFFFFF

---

## File System

### Paths
- `/disk/` - FAT16 persistent storage
- `/usr/` - Alias to `/disk/usr/`
- `/home/` - Alias to `/disk/home/`
- `/etc/` - Alias to `/disk/etc/`
- `/tmp/` - RAM disk (volatile)
- `/bin/` - Built-in commands (initramfs)

### FAT16 Limitations
- Max file size: 2 GB (4 GB theoretical)
- Max partition: 2-4 GB
- 8.3 filenames (LFN supported for reading)
- Case insensitive

---

## Input Handling

### Keyboard
```c
#include <syscall.h>

// Blocking read
int scancode = sys_key_read();

// Non-blocking check
if (sys_key_pressed(KEY_LEFT)) {
    // Left arrow is held
}
```

### Key Codes
```c
#define KEY_ESC     0x01
#define KEY_ENTER   0x1C
#define KEY_SPACE   0x39
#define KEY_UP      0x48
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_DOWN    0x50
// See full list in syscall.h
```

---

## Timing

```c
#include <time.h>
#include <unistd.h>

// Get current time
time_t now = time(NULL);

// Sleep (seconds)
sleep(1);

// Sleep (microseconds)
usleep(16000);  // ~60 FPS

// High-resolution timing
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
uint64_t ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
```

---

## Building Programs

### With TCC in VOS
```bash
# Simple compile
tcc -o program program.c

# With libraries
tcc -o game game.c -lolive -lm

# Multiple files
tcc -o app main.c utils.c -I/usr/include
```

### TCC Limitations
- No C++ support
- Limited inline assembly
- Some C99/C11 features missing
- No optimization (but fast compilation)

### Cross-Compilation (from Linux)
```bash
i686-elf-gcc -o program.elf program.c \
    -I/path/to/vos/sysroot/usr/include \
    -L/path/to/vos/sysroot/usr/lib \
    -nostdlib -lc -lvosposix -lgcc
```

---

## POSIX Compliance (~35-40%)

### Supported
- File I/O (open, read, write, close, seek)
- Directory operations
- Process spawning (spawn, not fork)
- Signals (basic)
- Terminal I/O (termios)
- Time functions
- Memory allocation

### Not Supported
- `fork()` / `exec()` (use `spawn()`)
- Threads (pthreads)
- Sockets / Networking
- `select()` / `poll()`
- Shared memory (SysV/POSIX)
- Message queues
- Semaphores

---

## What Works Well

- **2D Games:** Platformers, puzzles, retro-style games
- **Text Applications:** Editors, shells, utilities
- **Retro 3D:** Doom-style, low-poly games
- **Emulators:** CHIP-8, Game Boy, NES
- **Scripting:** Lua, Forth, BASIC interpreters
- **Development:** Edit and compile code natively

## Current Limitations

- **No GPU acceleration:** Software rendering only
- **No networking:** Serial only (SLIP possible)
- **No mouse:** Keyboard input only
- **No audio:** Sound not implemented
- **No USB:** Only emulated IDE/ATA
- **Single-threaded:** No preemptive multitasking

---

## Future Possibilities

These could be added with moderate effort:

| Feature | Complexity | Benefit |
|---------|------------|---------|
| Mouse support | Low | Better UI |
| Audio (PC speaker) | Low | Basic sound |
| Audio (Sound Blaster) | Medium | Full audio |
| SLIP networking | Low | Serial internet |
| virtio-net | Medium | QEMU networking |
| Cooperative threads | Low | Coroutines |
| USB (UHCI) | High | Real hardware |
| FAT32 | Medium | Larger disks |

---

## See Also

- [game_resources.md](game_resources.md) - Game development libraries
- [system_libraries.md](system_libraries.md) - System utilities
- [emulators.md](emulators.md) - Retro emulators
- [README.md](README.md) - Documentation index
