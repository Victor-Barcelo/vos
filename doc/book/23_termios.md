# Chapter 23: Terminal I/O

The termios interface controls terminal behavior, including line editing, echo, and special character handling. VOS implements POSIX-compatible terminal I/O.

## Termios Structure

```c
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;       // Input modes
    tcflag_t c_oflag;       // Output modes
    tcflag_t c_cflag;       // Control modes
    tcflag_t c_lflag;       // Local modes
    cc_t     c_cc[NCCS];    // Control characters
};
```

## Input Mode Flags (c_iflag)

```c
#define IGNBRK  0x0001  // Ignore break condition
#define BRKINT  0x0002  // Signal on break
#define IGNPAR  0x0004  // Ignore parity errors
#define PARMRK  0x0008  // Mark parity errors
#define INPCK   0x0010  // Enable parity check
#define ISTRIP  0x0020  // Strip 8th bit
#define INLCR   0x0040  // Map NL to CR
#define IGNCR   0x0080  // Ignore CR
#define ICRNL   0x0100  // Map CR to NL
#define IUCLC   0x0200  // Map uppercase to lowercase
#define IXON    0x0400  // Enable start/stop output
#define IXANY   0x0800  // Any char restarts output
#define IXOFF   0x1000  // Enable start/stop input
#define IMAXBEL 0x2000  // Ring bell on full buffer
```

## Output Mode Flags (c_oflag)

```c
#define OPOST   0x0001  // Post-process output
#define OLCUC   0x0002  // Map lowercase to uppercase
#define ONLCR   0x0004  // Map NL to CR-NL
#define OCRNL   0x0008  // Map CR to NL
#define ONOCR   0x0010  // No CR at column 0
#define ONLRET  0x0020  // NL performs CR
#define OFILL   0x0040  // Send fill characters
#define OFDEL   0x0080  // Fill is DEL
```

## Local Mode Flags (c_lflag)

```c
#define ISIG    0x0001  // Enable signals
#define ICANON  0x0002  // Canonical mode (line editing)
#define XCASE   0x0004  // Canonical upper/lower
#define ECHO    0x0008  // Enable echo
#define ECHOE   0x0010  // Echo ERASE as BS-SP-BS
#define ECHOK   0x0020  // Echo NL after KILL
#define ECHONL  0x0040  // Echo NL
#define NOFLSH  0x0080  // Don't flush after signal
#define TOSTOP  0x0100  // Stop background output
#define ECHOCTL 0x0200  // Echo control chars as ^X
#define ECHOPRT 0x0400  // Echo erased chars
#define ECHOKE  0x0800  // Visual erase for KILL
#define IEXTEN  0x8000  // Extended processing
```

## Control Characters (c_cc)

```c
#define VINTR    0   // Interrupt (Ctrl+C)
#define VQUIT    1   // Quit (Ctrl+\)
#define VERASE   2   // Erase (Backspace)
#define VKILL    3   // Kill line (Ctrl+U)
#define VEOF     4   // EOF (Ctrl+D)
#define VTIME    5   // Read timeout
#define VMIN     6   // Minimum chars for read
#define VSWTC    7   // Switch char
#define VSTART   8   // Start output (Ctrl+Q)
#define VSTOP    9   // Stop output (Ctrl+S)
#define VSUSP   10   // Suspend (Ctrl+Z)
#define VEOL    11   // End of line
#define VREPRINT 12  // Reprint line (Ctrl+R)
#define VDISCARD 13  // Discard (Ctrl+O)
#define VWERASE 14   // Word erase (Ctrl+W)
#define VLNEXT  15   // Literal next (Ctrl+V)
#define VEOL2   16   // Second EOL
```

## Default Terminal Settings

```c
static void init_termios(struct termios *t) {
    memset(t, 0, sizeof(*t));

    // Input: Map CR to NL, enable Ctrl+C/Ctrl+Z
    t->c_iflag = ICRNL;

    // Output: Post-process, NL to CR-NL
    t->c_oflag = OPOST | ONLCR;

    // Local: Canonical mode, echo, signals
    t->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL;

    // Control characters
    t->c_cc[VINTR]   = 0x03;  // Ctrl+C
    t->c_cc[VQUIT]   = 0x1C;  // Ctrl+\
    t->c_cc[VERASE]  = 0x7F;  // DEL
    t->c_cc[VKILL]   = 0x15;  // Ctrl+U
    t->c_cc[VEOF]    = 0x04;  // Ctrl+D
    t->c_cc[VSUSP]   = 0x1A;  // Ctrl+Z
    t->c_cc[VSTART]  = 0x11;  // Ctrl+Q
    t->c_cc[VSTOP]   = 0x13;  // Ctrl+S
    t->c_cc[VMIN]    = 1;
    t->c_cc[VTIME]   = 0;
}
```

## Canonical vs Raw Mode

### Canonical Mode (ICANON set)

- Input is line-buffered
- Line editing (backspace, kill line) works
- Read returns complete lines
- Ctrl+D signals EOF

### Raw Mode (ICANON clear)

- Input is character-by-character
- No line editing
- Read returns immediately (based on VMIN/VTIME)
- Used by text editors, games

### Switching to Raw Mode

```c
struct termios raw;
tcgetattr(STDIN_FILENO, &raw);

raw.c_lflag &= ~(ICANON | ECHO | ISIG);
raw.c_iflag &= ~(ICRNL | IXON);
raw.c_oflag &= ~(OPOST);

raw.c_cc[VMIN] = 1;   // Return after 1 char
raw.c_cc[VTIME] = 0;  // No timeout

tcsetattr(STDIN_FILENO, TCSANOW, &raw);
```

## Syscalls

### tcgetattr

```c
int32_t sys_tcgetattr(int fd, struct termios *termios_p) {
    task_t *task = current_task;

    if (fd < 0 || fd >= MAX_FDS || !task->fds[fd]) {
        return -EBADF;
    }

    file_desc_t *desc = task->fds[fd];
    if (!is_tty(desc->node)) {
        return -ENOTTY;
    }

    memcpy(termios_p, &task->termios, sizeof(struct termios));
    return 0;
}
```

### tcsetattr

```c
int32_t sys_tcsetattr(int fd, int actions, const struct termios *termios_p) {
    task_t *task = current_task;

    if (fd < 0 || fd >= MAX_FDS || !task->fds[fd]) {
        return -EBADF;
    }

    file_desc_t *desc = task->fds[fd];
    if (!is_tty(desc->node)) {
        return -ENOTTY;
    }

    switch (actions) {
    case TCSANOW:
        // Apply immediately
        break;
    case TCSADRAIN:
        // Wait for output to drain first
        tty_drain_output();
        break;
    case TCSAFLUSH:
        // Drain output and flush input
        tty_drain_output();
        tty_flush_input();
        break;
    default:
        return -EINVAL;
    }

    memcpy(&task->termios, termios_p, sizeof(struct termios));
    return 0;
}
```

## Terminal Input Processing

```c
void tty_input_char(char c) {
    struct termios *t = &current_task->termios;

    // Input processing (c_iflag)
    if (t->c_iflag & ISTRIP) {
        c &= 0x7F;
    }

    if (c == '\r' && (t->c_iflag & ICRNL)) {
        c = '\n';
    } else if (c == '\n' && (t->c_iflag & INLCR)) {
        c = '\r';
    } else if (c == '\r' && (t->c_iflag & IGNCR)) {
        return;
    }

    // Signal characters (c_lflag & ISIG)
    if (t->c_lflag & ISIG) {
        if (c == t->c_cc[VINTR]) {
            task_signal(current_task, SIGINT);
            return;
        }
        if (c == t->c_cc[VQUIT]) {
            task_signal(current_task, SIGQUIT);
            return;
        }
        if (c == t->c_cc[VSUSP]) {
            task_signal(current_task, SIGTSTP);
            return;
        }
    }

    // Canonical mode processing
    if (t->c_lflag & ICANON) {
        if (c == t->c_cc[VERASE]) {
            // Backspace
            if (line_pos > 0) {
                line_pos--;
                if (t->c_lflag & ECHOE) {
                    tty_output("\b \b");
                }
            }
            return;
        }

        if (c == t->c_cc[VKILL]) {
            // Kill line
            while (line_pos > 0) {
                line_pos--;
                if (t->c_lflag & ECHOK) {
                    tty_output("\b \b");
                }
            }
            return;
        }

        if (c == t->c_cc[VEOF]) {
            // EOF - signal end of input
            eof_pending = true;
            wake_readers();
            return;
        }

        // Buffer character
        if (line_pos < LINE_BUFFER_SIZE - 1) {
            line_buffer[line_pos++] = c;
        }

        // Echo
        if (t->c_lflag & ECHO) {
            tty_output_char(c);
        }

        // Complete line on newline
        if (c == '\n') {
            line_buffer[line_pos] = '\0';
            line_ready = true;
            wake_readers();
        }
    } else {
        // Raw mode - pass directly
        raw_buffer_push(c);

        if (t->c_lflag & ECHO) {
            tty_output_char(c);
        }

        wake_readers();
    }
}
```

## Terminal Read

```c
int32_t tty_read(void *buffer, size_t count) {
    struct termios *t = &current_task->termios;
    char *buf = buffer;
    size_t read = 0;

    if (t->c_lflag & ICANON) {
        // Canonical mode - wait for line
        while (!line_ready && !eof_pending) {
            task_block(current_task);
        }

        if (eof_pending && line_pos == 0) {
            eof_pending = false;
            return 0;  // EOF
        }

        // Copy line
        size_t to_copy = line_pos < count ? line_pos : count;
        memcpy(buf, line_buffer, to_copy);
        read = to_copy;

        // Shift remaining
        memmove(line_buffer, line_buffer + to_copy, line_pos - to_copy);
        line_pos -= to_copy;
        if (line_pos == 0) line_ready = false;
    } else {
        // Raw mode
        int vmin = t->c_cc[VMIN];
        int vtime = t->c_cc[VTIME];

        if (vmin == 0 && vtime == 0) {
            // Non-blocking
            while (read < count && raw_buffer_available()) {
                buf[read++] = raw_buffer_pop();
            }
        } else if (vmin > 0 && vtime == 0) {
            // Block until VMIN chars
            while (read < vmin && read < count) {
                while (!raw_buffer_available()) {
                    task_block(current_task);
                }
                buf[read++] = raw_buffer_pop();
            }
        }
        // ... other VMIN/VTIME combinations
    }

    return read;
}
```

## ioctl Operations

```c
int32_t sys_ioctl(int fd, unsigned long request, unsigned long arg) {
    task_t *task = current_task;

    if (fd < 0 || fd >= MAX_FDS || !task->fds[fd]) {
        return -EBADF;
    }

    file_desc_t *desc = task->fds[fd];

    if (!is_tty(desc->node)) {
        return -ENOTTY;
    }

    switch (request) {
    case TCGETS:
        return sys_tcgetattr(fd, (struct termios *)arg);

    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        return sys_tcsetattr(fd, request - TCSETS, (struct termios *)arg);

    case TIOCGWINSZ: {
        struct winsize *ws = (struct winsize *)arg;
        ws->ws_row = screen_rows();
        ws->ws_col = screen_cols();
        ws->ws_xpixel = screen_width();
        ws->ws_ypixel = screen_height();
        return 0;
    }

    case TIOCSWINSZ: {
        // Set window size (for virtual terminals)
        struct winsize *ws = (struct winsize *)arg;
        // ... store and signal SIGWINCH
        return 0;
    }

    case TIOCGPGRP:
        *(pid_t *)arg = foreground_pgrp;
        return 0;

    case TIOCSPGRP:
        foreground_pgrp = *(pid_t *)arg;
        return 0;

    default:
        return -EINVAL;
    }
}
```

## Window Size

```c
struct winsize {
    unsigned short ws_row;      // Rows (characters)
    unsigned short ws_col;      // Columns (characters)
    unsigned short ws_xpixel;   // Width (pixels)
    unsigned short ws_ypixel;   // Height (pixels)
};
```

Programs like `vi` and `less` use `TIOCGWINSZ` to determine terminal size and `SIGWINCH` to handle resizes.

## Summary

VOS terminal I/O provides:

1. **Termios structure** with input, output, control, and local modes
2. **Canonical mode** for line-buffered input with editing
3. **Raw mode** for character-by-character input
4. **Signal characters** (Ctrl+C, Ctrl+Z)
5. **Echo control** for visibility
6. **ioctl operations** for terminal control
7. **Window size** reporting

This enables editors, shells, and interactive programs to work correctly.

---

*Previous: [Chapter 22: Signals and IPC](22_signals.md)*
*Next: [Chapter 24: POSIX Overview and VOS Compliance](24_posix.md)*
