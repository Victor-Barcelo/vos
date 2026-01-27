# Chapter 39: User Management and Login

VOS implements a Linux-like user authentication system with support for multiple users, privilege separation, and persistent configuration.

## Overview

The user management system provides:

1. **Multi-user support** - Multiple user accounts with unique UIDs
2. **Privilege separation** - Root (UID 0) vs regular users
3. **Password authentication** - Optional password protection
4. **Home directories** - Per-user persistent storage
5. **Login shell** - Profile scripts and environment setup

```
+------------------+
|     init (PID 1) |
|   Spawns login   |
+------------------+
        |
        v
+------------------+
|      login       |
| - Read username  |
| - Check passwd   |
| - Set UID/GID    |
| - exec shell     |
+------------------+
        |
        v
+------------------+
|       dash       |
| - Read profile   |
| - Run commands   |
| - Process jobs   |
+------------------+
```

## The passwd File

### Format

VOS uses the standard 7-field `/etc/passwd` format:

```
# name:pass:uid:gid:gecos:home:shell
root::0:0:System Administrator:/root:/bin/dash
victor::1000:1000:Victor:/home/victor:/bin/dash
```

| Field | Description |
|-------|-------------|
| name | Username (max 31 chars) |
| pass | Password (empty = no prompt, `!` = locked) |
| uid | User ID (0 = root) |
| gid | Group ID |
| gecos | Full name / comment (optional) |
| home | Home directory path |
| shell | Login shell |

VOS also accepts 6-field format (without gecos) for compatibility.

### Parsing Implementation

```c
typedef struct user_entry {
    char name[32];
    char pass[64];
    uint32_t uid;
    uint32_t gid;
    char home[128];
    char shell[128];
} user_entry_t;

static int parse_passwd_line(char* line, user_entry_t* out) {
    // Skip comments and empty lines
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#') return -1;

    // Split by colons - supports both formats:
    // 7 fields: name:pass:uid:gid:gecos:home:shell
    // 6 fields: name:pass:uid:gid:home:shell
    char* fields[7] = {0};
    int nf = 0;
    char* p = line;

    for (; nf < 7; nf++) {
        fields[nf] = p;
        char* c = strchr(p, ':');
        if (!c) {
            nf++;  // Count last field
            break;
        }
        *c = '\0';
        p = c + 1;
    }

    // Validate username
    if (!fields[0] || fields[0][0] == '\0') return -1;

    memset(out, 0, sizeof(*out));
    strncpy(out->name, fields[0], sizeof(out->name) - 1);

    if (fields[1]) {
        strncpy(out->pass, fields[1], sizeof(out->pass) - 1);
    }

    if (fields[2]) parse_u32(fields[2], &out->uid);
    if (fields[3]) parse_u32(fields[3], &out->gid);

    // Determine home/shell based on field count
    const char* home_field = NULL;
    const char* shell_field = NULL;

    if (nf >= 7 && fields[6]) {
        // 7-field format with GECOS
        home_field = fields[5];
        shell_field = fields[6];
    } else if (nf >= 6) {
        // 6-field format without GECOS
        home_field = fields[4];
        shell_field = fields[5];
    }

    if (home_field && home_field[0]) {
        strncpy(out->home, home_field, sizeof(out->home) - 1);
    } else {
        snprintf(out->home, sizeof(out->home), "/home/%s", out->name);
    }

    if (shell_field && shell_field[0]) {
        strncpy(out->shell, shell_field, sizeof(out->shell) - 1);
    } else {
        strcpy(out->shell, "/bin/dash");
    }

    return 0;
}
```

## VFS Overlay for /etc

VOS uses an overlay system where `/etc` maps to `/disk/etc` if files exist there, otherwise falls back to the initramfs. The `/disk` mount point uses the Minix filesystem.

### Overlay Semantics

```c
// In kernel/vfs_posix.c
static bool vfs_path_exists_raw(const char* path) {
    if (!path) return false;
    bool is_dir;

    // Check minixfs for /disk/... paths
    if (ci_starts_with(path, "/disk")) {
        return minixfs_stat_ex(path + 5, &is_dir, NULL, NULL, NULL);
    }

    // Check ramfs for /ram/... paths
    while (*path == '/') path++;
    return ramfs_stat_ex(path, &is_dir, NULL, NULL, NULL);
}

static const char* abs_apply_posix_aliases(const char* abs, char tmp[]) {
    // /etc -> /disk/etc (overlay: only if exists on disk)
    if (abs_alias_to(abs, "/etc", "/disk/etc", tmp)) {
        if (vfs_path_exists_raw(tmp)) return tmp;
    }

    // Fall back to original path (initramfs)
    return abs;
}
```

### Effect on passwd

1. **First boot**: `/disk/etc/passwd` doesn't exist → reads from initramfs
2. **After setup**: `/disk/etc/passwd` created → reads from persistent storage
3. **User can modify**: Changes to `/etc/passwd` write to `/disk/etc/passwd`

## The login Program

### Passwd File Search

Login searches multiple locations for the passwd file:

```c
// Check /ram/etc/passwd first (set up by init), then /disk/etc/passwd
static const char* const passwd_paths[] = {
    "/ram/etc/passwd",
    "/disk/etc/passwd",
    "/etc/passwd",
    NULL
};
```

### Main Loop

```c
int main(int argc, char** argv) {
    for (;;) {
        // 1. Prompt for username
        char username[64];
        if (read_line("vos login: ", username, sizeof(username)) != 0) {
            sys_sleep(100);
            continue;
        }

        if (username[0] == '\0') continue;

        // 2. Look up user in passwd (tries multiple paths)
        user_entry_t user;
        if (load_user(username, &user) != 0) {
            printf("Login incorrect\n");
            continue;
        }

        // 3. Check password (if required)
        if (user.pass[0] != '\0' && user.pass[0] != '!') {
            char pass[64];
            if (read_password("Password: ", pass, sizeof(pass)) != 0) {
                printf("Login incorrect\n");
                continue;
            }
            if (strcmp(pass, user.pass) != 0) {
                printf("Login incorrect\n");
                continue;
            }
        }

        // 4. Set up session
        ensure_home_dir(user.home);
        chdir(user.home);

        // 5. Drop privileges
        setgid(user.gid);
        setuid(user.uid);
        setpgid(0, 0);  // New process group

        // 6. Set environment
        setenv("HOME", user.home, 1);
        setenv("USER", user.name, 1);
        setenv("SHELL", user.shell, 1);
        setenv("PATH", "/bin:/usr/bin", 1);

        // 7. Execute login shell
        char login_shell[130];
        const char* base = strrchr(user.shell, '/');
        base = base ? base + 1 : user.shell;
        snprintf(login_shell, sizeof(login_shell), "-%s", base);

        char* const sh_argv[] = {login_shell, NULL};
        execve(user.shell, sh_argv, NULL);

        // If we get here, exec failed
        printf("login: exec %s failed: %s\n", user.shell, strerror(errno));
        return 1;
    }
}
```

### Password Input

Password input disables echo:

```c
static int read_password(const char* prompt, char* buf, size_t cap) {
    struct termios t;
    if (tcgetattr(0, &t) != 0) {
        // Fallback to normal read
        return read_line(prompt, buf, cap);
    }

    // Disable echo
    struct termios noecho = t;
    noecho.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &noecho);

    int rc = read_line(prompt, buf, cap);

    // Restore echo
    tcsetattr(0, TCSANOW, &t);
    fputc('\n', stdout);

    return rc;
}
```

### Home Directory Creation

```c
static void ensure_home_dir(const char* home) {
    if (!home || home[0] == '\0') return;

    // Create /home if needed
    mkdir("/home", 0755);

    // Create user's home
    mkdir(home, 0755);
}
```

## Privilege Levels

### UID 0 (Root)

Root has special privileges:

```c
// In kernel/syscall.c
bool task_is_root(void) {
    task_t* t = get_current_task();
    return t && t->uid == 0;
}

// Example: only root can set system time
case SYS_SET_RTC: {
    if (!task_is_root()) {
        frame->eax = (uint32_t)-EPERM;
        return frame;
    }
    // ... set time ...
}
```

### setuid/setgid Syscalls

```c
// Drop privileges (can't go back!)
case SYS_SETUID: {
    uid_t uid = (uid_t)frame->ebx;
    task_t* t = get_current_task();

    // Root can set any UID
    // Non-root can only set to own UID
    if (t->uid != 0 && uid != t->uid) {
        frame->eax = (uint32_t)-EPERM;
        return frame;
    }

    t->uid = uid;
    t->euid = uid;
    frame->eax = 0;
    return frame;
}
```

## Init and First Boot

### First-Boot Initialization

When VOS detects a blank Minix disk, init prompts the user:

```
  ╔══════════════════════════════════════════════════════════════╗
  ║   ██╗   ██╗ ██████╗ ███████╗    First Boot Setup            ║
  ╚══════════════════════════════════════════════════════════════╝

  A blank disk has been detected.

  Options:
    [Y] Initialize disk for VOS
    [N] Boot in Live Mode

  Initialize disk for VOS? [Y/n]
```

### Disk Detection

```c
// In user/init.c
static int disk_available(void) {
    struct stat st;
    if (stat("/disk", &st) < 0) return 0;

    // Try creating a test file to verify write access
    int fd = open("/disk/.test", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    close(fd);
    unlink("/disk/.test");
    return 1;
}

static int disk_initialized(void) {
    return path_exists("/disk/.vos-initialized");
}
```

### Directory Setup

When user selects [Y], init creates the full directory structure:

```c
// In user/init.c
static void initialize_disk(void) {
    // Create directory structure
    mkdir("/disk/bin", 0755);
    mkdir("/disk/etc", 0755);
    mkdir("/disk/home", 0755);
    mkdir("/disk/root", 0700);
    mkdir("/disk/tmp", 01777);
    mkdir("/disk/var", 0755);
    mkdir("/disk/usr", 0755);

    // Copy all binaries from /bin to /disk/bin
    copy_binaries();

    // Create default users (root, victor)
    create_default_users();

    // Create system config (/etc/profile, /etc/motd, etc.)
    create_system_config();

    // Create home directories with .profile
    create_home_directories();

    // Write initialization marker
    write_file("/disk/.vos-initialized", "VOS initialized\n", 0644);
}
```

### Default Users

Init creates default users with 7-field passwd format:

```c
const char* passwd =
    "root::0:0:System Administrator:/root:/bin/dash\n"
    "victor::1000:1000:Victor:/home/victor:/bin/dash\n";
write_file("/disk/etc/passwd", passwd, 0644);
```

Both users have empty passwords, meaning no password prompt at login.

### Live Mode

If the user selects [N], VOS boots in live mode:
- Uses RAM-based `/ram/etc/passwd` for user database
- No changes written to disk
- All data lost on reboot
- Useful for testing or rescue operations

## Shell Profile

### /etc/profile

System-wide shell configuration. Note that dash doesn't support bash-style prompt escapes (`\u`, `\w`, `\[`, `\]`), so VOS uses direct ANSI escape characters:

```bash
# /etc/profile - System-wide shell configuration for VOS

# Set up PATH (includes /disk/bin for persistent binaries)
export PATH="/bin:/usr/bin:/disk/bin"

# Terminal and editor settings
export TERM=vt100
export EDITOR=edit

# Colored prompt using direct ANSI escapes (ESC = 0x1b)
# Dash doesn't support \e or \033 in PS1, so init writes actual ESC chars
if [ "$USER" = "root" ]; then
    PS1='<ESC>[1;31mroot<ESC>[0m@<ESC>[1;36mvos<ESC>[0m:<ESC>[1;33m$PWD<ESC>[0m# '
else
    PS1='<ESC>[1;32m$USER<ESC>[0m@<ESC>[1;36mvos<ESC>[0m:<ESC>[1;33m$PWD<ESC>[0m$ '
fi

# Useful aliases
alias ll='ls -la'
alias la='ls -A'
alias l='ls -l'
alias ..='cd ..'
alias ...='cd ../..'

# Source user profile if exists
if [ -f "$HOME/.profile" ]; then
    . "$HOME/.profile"
fi
```

The `<ESC>` above represents the actual escape character (0x1b). Init writes these directly into the profile file.

### Login Shell Detection

A login shell has `-` prefixed to argv[0]:

```c
// In login.c
char login_shell[130];
const char* base = strrchr(user.shell, '/');
base = base ? base + 1 : user.shell;
snprintf(login_shell, sizeof(login_shell), "-%s", base);

// Becomes "-dash" for /bin/dash
```

Dash reads `/etc/profile` when argv[0] starts with `-`.

## Path Aliases for Home Directories

### /home Alias

```c
// /home -> /disk/home (persistent storage)
if (abs_alias_to(abs, "/home", "/disk/home", tmp)) {
    if (vfs_path_exists_raw(tmp)) return tmp;
}

// /root -> /disk/root (root's home)
if (abs_alias_to(abs, "/root", "/disk/root", tmp)) {
    if (vfs_path_exists_raw(tmp)) return tmp;
}
```

### Effect

```bash
$ pwd
/home/victor        # Actually /disk/home/victor

$ echo "test" > myfile.txt
$ ls -la /disk/home/victor/
-rw-r--r-- victor myfile.txt   # Persists across reboots!
```

## Security Considerations

### Password Storage

Currently VOS stores passwords in plaintext (for simplicity). A production system should:

1. Hash passwords with bcrypt/scrypt
2. Store in `/etc/shadow` (root-readable only)
3. Use salt to prevent rainbow tables

### Process Isolation

Each user's processes run with their UID:

```c
// Fork inherits parent's UID/GID
case SYS_FORK: {
    task_t* child = task_create(...);
    child->uid = current->uid;
    child->gid = current->gid;
    child->euid = current->euid;
    child->egid = current->egid;
    // ...
}
```

### File Permissions

VOS supports basic Unix permissions:

```c
typedef struct {
    uint16_t mode;  // rwxrwxrwx + type bits
    uid_t uid;      // Owner
    gid_t gid;      // Group
} file_meta_t;

bool check_permission(file_meta_t* f, task_t* t, int access) {
    // Root can do anything
    if (t->uid == 0) return true;

    // Owner permissions
    if (t->uid == f->uid) {
        return (f->mode >> 6) & access;
    }

    // Group permissions
    if (t->gid == f->gid) {
        return (f->mode >> 3) & access;
    }

    // Other permissions
    return f->mode & access;
}
```

## Managing Users

### Adding a User

```bash
# As root, edit /etc/passwd
vi /etc/passwd

# Add line:
newuser::1001:1001:/home/newuser:/bin/dash

# Create home directory
mkdir /home/newuser
```

### Setting a Password

```bash
# Edit /etc/passwd, add password in second field
vi /etc/passwd

# Change:
newuser::1001:1001:/home/newuser:/bin/dash
# To:
newuser:mypassword:1001:1001:/home/newuser:/bin/dash
```

### Locking an Account

```bash
# Prefix password with ! to lock
newuser:!:1001:1001:/home/newuser:/bin/dash
```

## Syscall Reference

| Syscall | Description |
|---------|-------------|
| `SYS_GETUID` | Get current user ID |
| `SYS_GETGID` | Get current group ID |
| `SYS_GETEUID` | Get effective user ID |
| `SYS_GETEGID` | Get effective group ID |
| `SYS_SETUID` | Set user ID (privilege drop) |
| `SYS_SETGID` | Set group ID |

```c
// Get current UID
uid_t uid = getuid();

// Drop to regular user (can't undo!)
setuid(1000);
setgid(1000);
```

## Summary

VOS user management provides:

1. **Multi-user authentication** via `/etc/passwd` (7-field format)
2. **First-boot initialization** with interactive disk setup
3. **Overlay filesystem** for persistent configuration on Minix
4. **Privilege separation** with root vs regular users
5. **Per-user home directories** on persistent storage
6. **Login shell** with dash-compatible profile scripts
7. **Live mode** option for testing without persistence
8. **Basic file permissions** for access control

This creates a familiar Linux-like environment while maintaining simplicity.

---

*Previous: [Chapter 38: Font System and Themes](38_fonts.md)*
*Next: [Chapter 40: Virtual Consoles](40_vconsoles.md)*
