# VOS - Victor's Operating System

A minimal 32-bit x86 operating system with a POSIX-like userland, written in C.

## Quick Reference for LLM Agents

### Documentation Location

**All detailed documentation is in `/doc/book/`** - Read these files to understand VOS internals:

| File | Topic |
|------|-------|
| `00_index.md` | Table of contents |
| `01_introduction.md` | Overview and goals |
| `14_ata.md` | Disk driver |
| `16_vfs.md` | Virtual filesystem |
| `17_ramfs.md` | RAM filesystem |
| `18_minix.md` | MinixFS (persistent disk) |
| `19_tasking.md` | Process management |
| `21_syscalls.md` | Syscall implementation |
| `25_shell.md` | Shell (dash) |
| `26_newlib.md` | C library |
| `29_build.md` | Build system details |
| `34_syscall_reference.md` | Complete syscall list |
| `35_mcp_server.md` | MCP server usage |
| `39_users.md` | User management, login |

---

## Build System

### Basic Commands

```bash
make              # Build vos.iso
make clean        # Remove build artifacts
make run          # Build and run in QEMU
make format-disk  # Format the persistent disk image (first time only)
```

### Build Output

- `vos.iso` - Bootable ISO image
- `vos-disk.img` - Persistent MinixFS disk (4GB, created on first `make run`)
- `build/` - All compiled objects

### Key Makefile Variables

```makefile
USER_BINS         # List of all userland binaries
INITRAMFS_ROOT    # Where initramfs is assembled (build/initramfs_root)
ISO_DIR           # ISO staging directory (iso/)
```

---

## Adding New Files

### Adding a New Userland Program

1. **Create source file**: `user/myprogram.c`

2. **Add to Makefile** (around line 330):
```makefile
USER_MYPROGRAM = $(USER_BUILD_DIR)/myprogram.elf
# Add to USER_BINS list
USER_BINS = ... $(USER_MYPROGRAM) ...
```

3. **Add build rule** (if not using default pattern):
```makefile
$(USER_MYPROGRAM): $(USER_RUNTIME_OBJECTS) $(USER_BUILD_DIR)/myprogram.o $(USER_RUNTIME_LIBS)
	$(USER_LINK_CMD)
```

4. **Add to initramfs** (around line 720):
```makefile
cp $(USER_MYPROGRAM) $(INITRAMFS_ROOT)/bin/myprogram
```

### Adding Files to Initramfs

Files in `initramfs/` are automatically included. Directory structure:
```
initramfs/
├── bin/          # Binaries (added by Makefile, don't put here)
├── etc/          # Config files (passwd, profile, group)
├── res/          # Resources (images, sounds, fonts, app data)
└── sysroot/      # TCC development files (headers, libs)
```

To add a static file, just put it in the appropriate `initramfs/` subdirectory.

**CRITICAL: Do NOT create these directories at initramfs root level:**
- `/usr/` - Aliased to disk, will break Live Mode if exists in initramfs
- `/home/` - Aliased to disk
- `/var/` - Aliased to disk
- `/root/` - Aliased to disk

**Why?** VOS aliases paths like `/usr` to `/disk/usr`. If `/usr` exists in initramfs, it will be visible in Live Mode (no disk) causing conflicts. Put application resources in `/res/appname/` instead, and copy to `/disk/usr/` in `init.c` during first-boot setup.

### Adding Files to Persistent Disk

Files are installed to `/disk` (now just `/` after pivot_root) during first-boot setup in `user/init.c`:

1. Find `initialize_disk()` function
2. Add your file creation/copy logic there
3. Files persist across reboots

---

## Filesystem Layout (After pivot_root)

```
/                   # MinixFS (persistent disk)
├── bin/            # Executables
├── etc/            # Config (overlay from /ram/etc)
├── home/victor/    # User home
├── root/           # Root home
├── tmp/            # Temp files (/ram/tmp)
├── usr/            # Dev tools, headers, libs
└── var/            # Logs, state

/ram/               # RAMFS (volatile)
/initramfs/         # Boot files (read-only, rarely needed)
```

---

## MCP Server Usage

The VOS MCP server allows testing VOS from Claude Code without manual QEMU interaction.

### Available Commands

| Command | Description |
|---------|-------------|
| `vos_start` | Start QEMU with VOS |
| `vos_stop` | Stop QEMU |
| `vos_exec(command)` | Run command in VOS shell, wait for output |
| `vos_write(text)` | Send raw text to serial console |
| `vos_read(timeout)` | Read available serial output |
| `vos_screenshot()` | Capture screen (returns PNG path) |
| `vos_sendkeys(keys)` | Send keystrokes (e.g., "ret", "ctrl-c") |
| `vos_status()` | Check if VOS is running |
| `vos_upload(local, remote)` | Upload file to VOS |

### Efficient MCP Usage

1. **Prefer `vos_exec` over `vos_write`+`vos_read`** - It handles command/response automatically

2. **Don't take screenshots unless necessary** - They're slow and use tokens. Use `vos_exec` to check output.

3. **Login sequence**:
```
vos_start()
vos_exec("root")      # Login as root
vos_exec("ls /")      # Now you can run commands
```

4. **For commands that produce no output**, `vos_exec` returns empty - this is normal

5. **Alt+1-4 for console switching** - Use `vos_sendkeys("alt-2")` but note MCP may not fully support modifier keys

6. **Serial console** - All output goes to serial, which MCP reads. Kernel debug messages appear here too.

### Common Patterns

```python
# Start and login
vos_start()
vos_exec("root")

# Run commands
result = vos_exec("ls /bin")
result = vos_exec("cat /etc/passwd")

# Check something works
vos_exec("echo test")  # Should return "test"

# When done
vos_stop()
```

---

## Kernel Development

### Adding a New Syscall

1. **kernel/syscall.c** - Add enum value:
```c
SYS_MYSYSCALL = 103,
SYS_MAX = 104,
```

2. **Add name** (same file):
```c
[SYS_MYSYSCALL] = "mysyscall",
```

3. **Add handler** (in switch statement):
```c
case SYS_MYSYSCALL: {
    // Implementation
    frame->eax = result;
    return frame;
}
```

4. **user/syscall.h** - Add enum and wrapper:
```c
SYS_MYSYSCALL = 103,

static inline int sys_mysyscall(int arg) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_MYSYSCALL), "b"(arg) : "memory");
    return ret;
}
```

### Key Kernel Files

| File | Purpose |
|------|---------|
| `kernel/kernel.c` | Main entry, initialization |
| `kernel/task.c` | Process management, scheduling |
| `kernel/syscall.c` | Syscall handlers |
| `kernel/vfs_posix.c` | Filesystem operations |
| `kernel/minixfs.c` | Persistent filesystem |
| `kernel/screen.c` | Framebuffer, console |
| `kernel/keyboard.c` | Input handling |

### Key User Files

| File | Purpose |
|------|---------|
| `user/init.c` | First userspace process, disk setup |
| `user/login.c` | Login process |
| `user/syscall.h` | Syscall wrappers |
| `user/newlib_syscalls.c` | Newlib integration |

---

## Debugging Tips

1. **Serial output** - Add `serial_write_string("[DEBUG] message\n");` in kernel code

2. **Printf in userspace** - `printf()` works normally, output goes to active console

3. **Check build errors**:
```bash
make 2>&1 | grep -E "error:|undefined"
```

4. **Test in VOS**:
```bash
make run   # Opens QEMU window
# Or use MCP: vos_start(), vos_exec("command")
```

5. **Common issues**:
   - Undefined symbol → Check Makefile linking order
   - Syscall not working → Check both kernel enum AND user enum match
   - File not found → Check initramfs copy rules in Makefile

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                    User Space                        │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │  init   │ │  login  │ │  dash   │ │  apps   │   │
│  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘   │
│       │           │           │           │         │
│  ┌────┴───────────┴───────────┴───────────┴────┐   │
│  │              newlib (libc)                   │   │
│  └────────────────────┬────────────────────────┘   │
│                       │ syscall (int 0x80)          │
├───────────────────────┼─────────────────────────────┤
│                       ▼                             │
│  ┌─────────────────────────────────────────────┐   │
│  │               VOS Kernel                     │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐       │   │
│  │  │  VFS    │ │ Tasking │ │ Memory  │       │   │
│  │  │ minixfs │ │scheduler│ │  PMM    │       │   │
│  │  │ ramfs   │ │ signals │ │ paging  │       │   │
│  │  └─────────┘ └─────────┘ └─────────┘       │   │
│  └─────────────────────────────────────────────┘   │
│                    Kernel Space                     │
└─────────────────────────────────────────────────────┘
```

---

## Quick Checklist for New Features

- [ ] Read relevant docs in `/doc/book/`
- [ ] Create source files in appropriate location
- [ ] Update Makefile (build rules, binary list, initramfs copy)
- [ ] If syscall needed: update kernel/syscall.c AND user/syscall.h
- [ ] Test with `make run` or MCP server
- [ ] Check serial output for errors

---

## Contact

This is Victor's personal OS project. For questions about the codebase, check the documentation or examine the source code directly.
