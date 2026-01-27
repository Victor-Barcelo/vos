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

VOS uses a simplified `/etc/passwd` format:

```
# name:pass:uid:gid:home:shell
root::0:0:/root:/bin/dash
victor::1000:1000:/home/victor:/bin/dash
```

| Field | Description |
|-------|-------------|
| name | Username (max 31 chars) |
| pass | Password (empty = no prompt, `!` = locked) |
| uid | User ID (0 = root) |
| gid | Group ID |
| home | Home directory path |
| shell | Login shell |

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

    // Split by colons: name:pass:uid:gid:home:shell
    char* fields[6] = {0};
    int nf = 0;
    char* p = line;

    for (; nf < 6; nf++) {
        fields[nf] = p;
        char* c = strchr(p, ':');
        if (!c) break;
        *c = '\0';
        p = c + 1;
    }

    // Validate and copy fields
    if (!fields[0] || fields[0][0] == '\0') return -1;

    memset(out, 0, sizeof(*out));
    strncpy(out->name, fields[0], sizeof(out->name) - 1);

    if (fields[1]) {
        strncpy(out->pass, fields[1], sizeof(out->pass) - 1);
    }

    if (fields[2]) parse_u32(fields[2], &out->uid);
    if (fields[3]) parse_u32(fields[3], &out->gid);

    if (fields[4] && fields[4][0]) {
        strncpy(out->home, fields[4], sizeof(out->home) - 1);
    } else {
        snprintf(out->home, sizeof(out->home), "/home/%s", out->name);
    }

    if (fields[5] && fields[5][0]) {
        strncpy(out->shell, fields[5], sizeof(out->shell) - 1);
    } else {
        strcpy(out->shell, "/bin/sh");
    }

    return 0;
}
```

## VFS Overlay for /etc

VOS uses an overlay system where `/etc` maps to `/disk/etc` if files exist there, otherwise falls back to the initramfs.

### Overlay Semantics

```c
// In kernel/vfs_posix.c
static bool vfs_path_exists_raw(const char* path) {
    if (!path) return false;
    bool is_dir;

    // Check fatdisk for /disk/... paths
    if (ci_starts_with(path, "/disk")) {
        return fatdisk_stat_ex(path, &is_dir, NULL, NULL, NULL);
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

        // 2. Look up user in passwd
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

### Directory Setup

Init creates required directories on boot:

```c
// In user/init.c
// Set up persistent directories on /disk
mkdir("/disk/etc", 0755);
mkdir("/disk/home", 0755);
mkdir("/disk/root", 0700);   // Root's home
mkdir("/disk/home/victor", 0755);

// Copy passwd to persistent storage if not present
struct stat st;
if (stat("/disk/etc/passwd", &st) < 0) {
    printf("First boot: copying /etc/passwd to /disk/etc/passwd\n");

    int src = open("/etc/passwd", O_RDONLY);
    if (src >= 0) {
        int dst = open("/disk/etc/passwd", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dst >= 0) {
            char buf[256];
            int n;
            while ((n = read(src, buf, sizeof(buf))) > 0) {
                write(dst, buf, n);
            }
            close(dst);
        }
        close(src);
    }
}
```

### Default Users

The initramfs includes default users:

```
# /etc/passwd (in initramfs)
# name:pass:uid:gid:home:shell
root::0:0:/root:/bin/dash
victor::1000:1000:/home/victor:/bin/dash
```

Both users have empty passwords, meaning no password prompt at login.

## Shell Profile

### /etc/profile

System-wide shell configuration:

```bash
# /etc/profile - System-wide shell configuration for VOS

# Set up PATH
export PATH="/bin:/usr/bin"

# Set default editor
export EDITOR="vi"

# History settings
export HISTSIZE=100

# Terminal type
export TERM="xterm"

# Colored prompt
if [ "${USER}" = "root" ]; then
    PS1="$(printf '\033[1;31m')root$(printf '\033[0m')@vos# "
else
    PS1="$(printf '\033[1;32m')${USER}$(printf '\033[0m')@vos$ "
fi
export PS1

# Useful aliases
ll() { ls -la "$@"; }
la() { ls -a "$@"; }

# Load user's default font
font load 2>/dev/null

# Welcome message
echo ""
echo "  Welcome to VOS"
echo "  Use arrow keys for command history"
echo ""
```

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

1. **Multi-user authentication** via `/etc/passwd`
2. **Overlay filesystem** for persistent configuration
3. **Privilege separation** with root vs regular users
4. **Per-user home directories** on persistent storage
5. **Login shell** with profile scripts
6. **Basic file permissions** for access control

This creates a familiar Linux-like environment while maintaining simplicity.

---

*Previous: [Chapter 38: Font System and Themes](38_fonts.md)*
*Next: [Chapter 40: Virtual Consoles](40_vconsoles.md)*
