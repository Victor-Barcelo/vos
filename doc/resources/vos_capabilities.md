# VOS System Capabilities

Technical reference for VOS's current features and limitations.

---

## System Specifications

| Component | Specification |
|-----------|---------------|
| **Architecture** | x86-32 (i686) |
| **Max RAM** | 4 GB (32-bit addressing) |
| **Max Disk** | 128 GB (28-bit LBA), 2-4 GB per partition (FAT16) |
| **Graphics** | Software framebuffer (Bochs/VBE) |
| **Input** | PS/2 Keyboard and Mouse |
| **Serial** | COM1 at 115200 baud |
| **Compiler** | TCC (Tiny C Compiler) |
| **C Library** | Newlib |
| **Shell** | Custom POSIX-like shell |

---

## Syscalls Available

VOS provides 71+ syscalls. Key ones for application development:

### Process Management
| # | Syscall | Description |
|---|---------|-------------|
| 1 | `exit(status)` | Terminate process |
| 2 | `fork()` | Create child process |
| 3 | `waitpid(pid, status, opts)` | Wait for child |
| 6 | `execve(path, argv, envp)` | Execute program |
| 7 | `getpid()` | Get process ID |
| 8 | `getppid()` | Get parent PID |
| 64 | `setsid()` | Create session |
| 65 | `getsid(pid)` | Get session ID |
| 66 | `setpgid(pid, pgid)` | Set process group |
| 67 | `getpgrp()` | Get process group |

### File I/O
| # | Syscall | Description |
|---|---------|-------------|
| 4 | `read(fd, buf, count)` | Read from fd |
| 5 | `write(fd, buf, count)` | Write to fd |
| 9 | `open(path, flags, mode)` | Open file |
| 10 | `close(fd)` | Close file descriptor |
| 11 | `lseek(fd, offset, whence)` | Seek in file |
| 12 | `stat(path, buf)` | Get file info |
| 13 | `fstat(fd, buf)` | Get file info by fd |
| 14 | `unlink(path)` | Delete file |
| 15 | `mkdir(path, mode)` | Create directory |
| 16 | `rmdir(path)` | Remove directory |
| 17 | `rename(old, new)` | Rename file |
| 18 | `getcwd(buf, size)` | Get working directory |
| 19 | `chdir(path)` | Change directory |
| 23 | `dup(fd)` | Duplicate fd |
| 24 | `dup2(oldfd, newfd)` | Duplicate to specific fd |
| 25 | `pipe(fds[2])` | Create pipe |
| 30 | `truncate(path, length)` | Truncate file |
| 35 | `access(path, mode)` | Check access |
| 38 | `readdir(fd, dirent)` | Read directory entry |
| 51 | `openat(dirfd, path, flags)` | Open relative to dir |

### Memory
| # | Syscall | Description |
|---|---------|-------------|
| 20 | `brk(addr)` | Set program break |
| 21 | `sbrk(incr)` | Extend heap |
| 45 | `mmap(...)` | Map memory |
| 46 | `munmap(addr, len)` | Unmap memory |

### Signals
| # | Syscall | Description |
|---|---------|-------------|
| 26 | `kill(pid, sig)` | Send signal |
| 27 | `signal(sig, handler)` | Set signal handler |
| 28 | `sigaction(...)` | Advanced signal handling |
| 29 | `sigprocmask(...)` | Block signals |
| 44 | `sigsuspend(mask)` | Wait for signal |
| 60 | `sigreturn()` | Return from handler |

### Time
| # | Syscall | Description |
|---|---------|-------------|
| 40 | `time(time_t*)` | Get Unix timestamp |
| 41 | `gettimeofday(tv, tz)` | Get time with microseconds |
| 42 | `nanosleep(req, rem)` | Sleep for duration |
| 43 | `clock_gettime(clk, tp)` | Get clock time |
| 52 | `setitimer(...)` | Set interval timer |
| 53 | `getitimer(...)` | Get interval timer |

### Terminal I/O
| # | Syscall | Description |
|---|---------|-------------|
| 32 | `tcgetattr(fd, termios)` | Get terminal attributes |
| 33 | `tcsetattr(fd, act, termios)` | Set terminal attributes |
| 34 | `ioctl(fd, req, arg)` | Device control |
| 36 | `isatty(fd)` | Check if TTY |

### VOS-Specific
| # | Syscall | Description |
|---|---------|-------------|
| 70 | `uptime_ms()` | System uptime in ms |
| 71 | `sleep(ms)` | Sleep milliseconds |
| 72 | `yield()` | Yield CPU |
| 73 | `proc_list(buf, size)` | List processes |
| 74 | `screen_is_fb()` | Check framebuffer mode |
| 75 | `gfx_blit_rgba(x,y,w,h,pixels)` | Blit RGBA buffer |
| 76 | `font_count()` | Get font count |
| 77 | `font_info(idx, info)` | Get font info |
| 78 | `font_set(idx)` | Set current font |
| 79 | `font_get_current()` | Get current font index |

### Miscellaneous
| # | Syscall | Description |
|---|---------|-------------|
| 22 | `uname(buf)` | Get system info |
| 37 | `umask(mask)` | Set file creation mask |
| 39 | `fcntl(fd, cmd, arg)` | File control |
| 48 | `getenv_vos(name, buf, size)` | Get env var |
| 49 | `setenv_vos(name, value)` | Set env var |
| 50 | `unsetenv_vos(name)` | Unset env var |

---

## Graphics Capabilities

### Framebuffer
- Bochs display or VESA/VBE linear framebuffer
- Supports high resolutions: 1920x1080, 1280x720, etc.
- 32-bit color (ARGB8888 / RGBA8888)
- Direct pixel access via `sys_gfx_blit_rgba()`

### Available Libraries
| Library | Type | Features |
|---------|------|----------|
| **olive.c** | 2D | Lines, rects, circles, triangles, sprites, text |
| **small3dlib** | 3D | Wireframe, flat shading, integer math only |
| **stb_image** | Images | PNG, JPG, BMP loading |

### Graphics Example
```c
#include <stdint.h>

// Direct syscall for graphics
extern int32_t syscall5(int num, int a, int b, int c, int d, int e);
#define SYS_GFX_BLIT_RGBA 75

void blit(int x, int y, int w, int h, uint32_t *pixels) {
    syscall5(SYS_GFX_BLIT_RGBA, x, y, w, h, (int)pixels);
}
```

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
#include <unistd.h>

// Standard input (blocking)
char c;
read(STDIN_FILENO, &c, 1);

// With termios for raw mode
struct termios old, new;
tcgetattr(STDIN_FILENO, &old);
new = old;
new.c_lflag &= ~(ICANON | ECHO);
tcsetattr(STDIN_FILENO, TCSANOW, &new);
// Now read() returns immediately
```

### Mouse (PS/2)
VOS now supports PS/2 mouse input. Mouse events are available through the kernel and can be accessed via ioctl or dedicated syscalls.

### Key Codes
```c
#define KEY_ESC     0x01
#define KEY_ENTER   0x1C
#define KEY_SPACE   0x39
#define KEY_UP      0x48
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_DOWN    0x50
// See full list in keyboard.h
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

// VOS-specific millisecond sleep
extern int32_t syscall1(int num, int arg);
#define SYS_SLEEP 71
syscall1(SYS_SLEEP, 100);  // Sleep 100ms
```

---

## Building Programs

### With TCC in VOS
```bash
# Simple compile
tcc -o program program.c

# With math library
tcc -o game game.c -lm

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
# Build with VOS toolchain
make -C /path/to/vos USER_PROGRAMS="myprogram"

# Or manually with cross-compiler
i686-elf-gcc -o program.elf program.c \
    -I/path/to/vos/sysroot/usr/include \
    -L/path/to/vos/sysroot/usr/lib \
    -nostdlib -lc -lgcc
```

---

## POSIX Compliance (~45%)

### Fully Supported
- File I/O (open, read, write, close, lseek, stat, fstat)
- Directory operations (mkdir, rmdir, chdir, getcwd, opendir, readdir)
- Process management (fork, exec, wait, exit, getpid, getppid)
- Pipes and I/O redirection
- Signals (signal, sigaction, kill, sigprocmask)
- Terminal I/O (termios, ioctl)
- Time functions (time, gettimeofday, clock_gettime, nanosleep)
- Memory allocation (sbrk, mmap anonymous)
- Environment variables (getenv, setenv)

### Partially Supported
- File permissions (mode stored but not enforced)
- User/group IDs (single user system)
- File locking (not implemented)

### Not Supported
- Threads (pthreads)
- Sockets / Networking
- `select()` / `poll()`
- Shared memory (SysV/POSIX)
- Message queues
- Semaphores
- Memory-mapped files

---

## What Works Well

- **Shell Scripting:** Pipes, redirects, background jobs
- **Text Applications:** Editors (ne, ved), shells, utilities
- **2D Games:** Platformers, puzzles, retro-style games
- **Retro 3D:** Doom-style, low-poly games
- **Emulators:** CHIP-8, Game Boy, NES
- **Scripting:** Lua, Forth, BASIC interpreters
- **Development:** Edit and compile code natively with TCC

## Current Limitations

- **No GPU acceleration:** Software rendering only
- **No networking:** Serial only (SLIP possible)
- **No audio:** Sound not implemented
- **No USB:** Only emulated IDE/ATA
- **Single-threaded:** No preemptive multitasking within processes

---

## Available Programs

### System Utilities
| Program | Description |
|---------|-------------|
| `sh` | POSIX-like shell |
| `init` | Init process |
| `login` | Login manager |
| `ps` | Process list |
| `top` | Process monitor |
| `uptime` | System uptime |
| `date` | Current date/time |
| `neofetch` | System info display |

### Editors
| Program | Description |
|---------|-------------|
| `ne` | Nice Editor (full-featured) |
| `ved` | Vi-like editor |

### sbase Utilities
| Program | Description |
|---------|-------------|
| `cat` | Concatenate files |
| `ls` | List directory |
| `cp` | Copy files |
| `mv` | Move files |
| `rm` | Remove files |
| `mkdir` | Create directory |
| `grep` | Search patterns |
| `head` | First lines |
| `tail` | Last lines |
| `wc` | Word count |
| `sort` | Sort lines |
| `uniq` | Unique lines |
| `echo` | Print text |
| `printf` | Formatted print |
| `test` | Condition testing |
| `basename` | Strip path |
| `dirname` | Directory name |

### Development
| Program | Description |
|---------|-------------|
| `tcc` | Tiny C Compiler |
| `font` | Font selector |

### Entertainment
| Program | Description |
|---------|-------------|
| `basic` | BASIC interpreter |
| `eliza` | Chatbot |
| `zork` | Text adventure |
| `olivedemo` | 2D graphics demo |
| `s3lcube` | 3D cube demo |
| `img` | Image viewer |

---

## Future Possibilities

These could be added with varying effort:

| Feature | Complexity | Benefit |
|---------|------------|---------|
| select()/poll() | Medium | I/O multiplexing |
| Cooperative threads | Low | Coroutines |
| Audio (PC speaker) | Low | Basic sound |
| Audio (Sound Blaster) | Medium | Full audio |
| SLIP networking | Low | Serial internet |
| virtio-net | Medium | QEMU networking |
| Full pthreads | High | Multi-threading |
| USB (UHCI) | High | Real hardware |
| FAT32 | Medium | Larger disks |
| SMP support | High | Multi-core |

---

## See Also

- [game_resources.md](game_resources.md) - Game development libraries
- [system_libraries.md](system_libraries.md) - System utilities
- [emulators.md](emulators.md) - Retro emulators
- [networking.md](networking.md) - Network stack options
- [README.md](README.md) - Documentation index
