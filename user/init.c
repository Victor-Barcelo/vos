#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "syscall.h"

// Simple VT100 color helpers (VOS framebuffer console supports basic SGR).
#define CLR_RESET  "\x1b[0m"
#define CLR_BOLD   "\x1b[1m"
#define CLR_CYAN   "\x1b[36;1m"
#define CLR_GREEN  "\x1b[32;1m"
#define CLR_YELLOW "\x1b[33;1m"
#define CLR_RED    "\x1b[31;1m"
#define CLR_WHITE  "\x1b[37;1m"
#define CLR_BLUE   "\x1b[34;1m"

// Live mode flag - if true, don't try to persist to /disk
static int g_live_mode = 0;

static void tag(const char* t, const char* clr) {
    if (!t) t = "";
    if (!clr) clr = CLR_RESET;
    printf("%s%s%s", clr, t, CLR_RESET);
}

static void cat_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return;
    }
    char buf[128];
    for (;;) {
        int n = (int)read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        (void)write(1, buf, (unsigned int)n);
    }
    close(fd);
}

// Copy a single file from src to dst, optionally set mode
static int copy_file_mode(const char* src, const char* dst, mode_t mode) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -1;

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dfd < 0) {
        close(sfd);
        return -1;
    }

    char buf[1024];
    int n;
    while ((n = (int)read(sfd, buf, sizeof(buf))) > 0) {
        (void)write(dfd, buf, (unsigned int)n);
    }

    close(dfd);
    close(sfd);
    return 0;
}

static int copy_file(const char* src, const char* dst) {
    return copy_file_mode(src, dst, 0644);
}

// Write a string to a file
static int write_file(const char* path, const char* content, mode_t mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) return -1;
    (void)write(fd, content, strlen(content));
    close(fd);
    return 0;
}

// Check if a path exists
static int path_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Check if disk is available (minixfs mounted)
static int disk_available(void) {
    // Try to stat /disk - if it fails with EIO, no disk
    struct stat st;
    if (stat("/disk", &st) < 0) {
        return 0;
    }
    // Try creating a test file to verify write access
    int fd = open("/disk/.test", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        // EIO means disk driver issue, EROFS means read-only
        return 0;
    }
    close(fd);
    unlink("/disk/.test");
    return 1;
}

// Check if disk is initialized (has VOS marker)
static int disk_initialized(void) {
    return path_exists("/disk/.vos-initialized");
}

// Read a single character from stdin (blocking)
static char read_char(void) {
    char c = 0;
    (void)read(0, &c, 1);
    return c;
}

// Show initialization banner
static void show_init_banner(void) {
    printf("\n");
    printf("%s", CLR_CYAN);
    printf("  ╔══════════════════════════════════════════════════════════════╗\n");
    printf("  ║                                                              ║\n");
    printf("  ║   %s██╗   ██╗ ██████╗ ███████╗%s                                ║\n", CLR_WHITE, CLR_CYAN);
    printf("  ║   %s██║   ██║██╔═══██╗██╔════╝%s    %sFirst Boot Setup%s           ║\n", CLR_WHITE, CLR_CYAN, CLR_YELLOW, CLR_CYAN);
    printf("  ║   %s██║   ██║██║   ██║███████╗%s                                ║\n", CLR_WHITE, CLR_CYAN);
    printf("  ║   %s╚██╗ ██╔╝██║   ██║╚════██║%s                                ║\n", CLR_WHITE, CLR_CYAN);
    printf("  ║   %s ╚████╔╝ ╚██████╔╝███████║%s                                ║\n", CLR_WHITE, CLR_CYAN);
    printf("  ║   %s  ╚═══╝   ╚═════╝ ╚══════╝%s                                ║\n", CLR_WHITE, CLR_CYAN);
    printf("  ║                                                              ║\n");
    printf("  ╚══════════════════════════════════════════════════════════════╝\n");
    printf("%s\n", CLR_RESET);
}

// Copy all executables from /bin to /disk/bin
static void copy_binaries(void) {
    tag("[setup] ", CLR_CYAN);
    printf("Copying system binaries to /disk/bin...\n");

    DIR* d = opendir("/bin");
    if (!d) {
        tag("[setup] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("Cannot open /bin\n");
        return;
    }

    int count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char srcpath[256], dstpath[256];
        snprintf(srcpath, sizeof(srcpath), "/bin/%s", ent->d_name);
        snprintf(dstpath, sizeof(dstpath), "/disk/bin/%s", ent->d_name);

        struct stat st;
        if (stat(srcpath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (copy_file_mode(srcpath, dstpath, st.st_mode | 0111) == 0) {
                count++;
            }
        }
    }
    closedir(d);

    tag("[setup] ", CLR_CYAN);
    printf("Copied %d binaries\n", count);
}

// Create default user configuration
static void create_default_users(void) {
    tag("[setup] ", CLR_CYAN);
    printf("Creating default users (root, victor)...\n");

    // passwd file
    const char* passwd =
        "root::0:0:System Administrator:/root:/bin/dash\n"
        "victor::1000:1000:Victor:/home/victor:/bin/dash\n";
    write_file("/disk/etc/passwd", passwd, 0644);

    // group file
    const char* group =
        "root::0:root\n"
        "wheel::10:root,victor\n"
        "users::100:victor\n"
        "victor::1000:victor\n";
    write_file("/disk/etc/group", group, 0644);

    // shadow file (empty passwords for now)
    const char* shadow =
        "root::0:0:99999:7:::\n"
        "victor::0:0:99999:7:::\n";
    write_file("/disk/etc/shadow", shadow, 0600);
}

// Create system configuration files
static void create_system_config(void) {
    tag("[setup] ", CLR_CYAN);
    printf("Creating system configuration...\n");

    // /etc/profile - system-wide shell config
    // Note: Using actual ESC chars (0x1b) since dash doesn't support \e or \033 in PS1
    const char* profile =
        "# /etc/profile - system-wide shell configuration\n"
        "\n"
        "export PATH=/bin:/usr/bin:/disk/bin\n"
        "export TERM=xterm\n"
        "export EDITOR=vi\n"
        "\n"
        "# Color prompt: user@vos:dir$ (red for root)\n"
        "if [ \"$USER\" = \"root\" ]; then\n"
        "    PS1='\x1b[1;31mroot\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m# '\n"
        "else\n"
        "    PS1='\x1b[1;32m$USER\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m$ '\n"
        "fi\n"
        "\n"
        "# Handy aliases\n"
        "alias ll='ls -la'\n"
        "alias la='ls -A'\n"
        "alias l='ls -l'\n"
        "alias ..='cd ..'\n"
        "alias ...='cd ../..'\n"
        "\n"
        "# Source user profile if exists\n"
        "if [ -f \"$HOME/.profile\" ]; then\n"
        "    . \"$HOME/.profile\"\n"
        "fi\n";
    write_file("/disk/etc/profile", profile, 0644);

    // /etc/motd - message of the day
    const char* motd =
        "\n"
        "  Welcome to VOS!\n"
        "\n"
        "  Type 'help' for available commands.\n"
        "  Your files are stored in /home/<username>\n"
        "\n";
    write_file("/disk/etc/motd", motd, 0644);

    // /etc/hostname
    write_file("/disk/etc/hostname", "vos\n", 0644);

    // /etc/issue - pre-login banner
    const char* issue =
        "\\e[1;36mVOS\\e[0m 0.1.0 - \\l\n"
        "\n";
    write_file("/disk/etc/issue", issue, 0644);
}

// Create user home directories with profiles
static void create_home_directories(void) {
    tag("[setup] ", CLR_CYAN);
    printf("Creating home directories...\n");

    // Root home
    mkdir("/disk/root", 0700);
    const char* root_profile =
        "# ~/.profile - root shell configuration\n"
        "export HOME=/root\n"
        "cd $HOME\n";
    write_file("/disk/root/.profile", root_profile, 0644);

    // Victor home
    mkdir("/disk/home/victor", 0755);
    const char* user_profile =
        "# ~/.profile - user shell configuration\n"
        "cd $HOME\n";
    write_file("/disk/home/victor/.profile", user_profile, 0644);

    // Set ownership (victor = uid 1000, gid 1000)
    chown("/disk/home/victor", 1000, 1000);
    chown("/disk/home/victor/.profile", 1000, 1000);
}

// Full disk initialization
static void initialize_disk(void) {
    printf("\n");
    tag("[setup] ", CLR_CYAN);
    printf("Initializing VOS disk...\n\n");

    // Create directory structure
    tag("[setup] ", CLR_CYAN);
    printf("Creating directory structure...\n");

    mkdir("/disk/bin", 0755);
    mkdir("/disk/etc", 0755);
    mkdir("/disk/home", 0755);
    mkdir("/disk/root", 0700);
    mkdir("/disk/tmp", 01777);  // sticky bit
    mkdir("/disk/var", 0755);
    mkdir("/disk/var/log", 0755);
    mkdir("/disk/var/tmp", 01777);
    mkdir("/disk/usr", 0755);
    mkdir("/disk/usr/bin", 0755);
    mkdir("/disk/usr/lib", 0755);
    mkdir("/disk/usr/share", 0755);

    // Copy binaries
    copy_binaries();

    // Create users
    create_default_users();

    // Create system config
    create_system_config();

    // Create home directories
    create_home_directories();

    // Create initialization marker
    tag("[setup] ", CLR_CYAN);
    printf("Finalizing...\n");
    write_file("/disk/.vos-initialized",
        "VOS initialized\n"
        "Version: 0.1.0\n", 0644);

    printf("\n");
    tag("[setup] ", CLR_CYAN);
    tag("Disk initialization complete!\n", CLR_GREEN);
    printf("\n");
}

// Prompt user for disk initialization
static int prompt_disk_init(void) {
    show_init_banner();

    printf("  A blank disk has been detected.\n\n");
    printf("  %sOptions:%s\n", CLR_BOLD, CLR_RESET);
    printf("    %s[Y]%s Initialize disk for VOS\n", CLR_GREEN, CLR_RESET);
    printf("        - Creates /bin, /etc, /home directories\n");
    printf("        - Sets up root and victor users\n");
    printf("        - Copies system binaries\n");
    printf("        - All changes will persist across reboots\n\n");
    printf("    %s[N]%s Boot in Live Mode\n", CLR_YELLOW, CLR_RESET);
    printf("        - No changes written to disk\n");
    printf("        - All data lost on reboot\n");
    printf("        - Good for testing\n\n");

    printf("  Initialize disk for VOS? %s[Y/n]%s ", CLR_CYAN, CLR_RESET);
    fflush(stdout);

    char c = read_char();
    printf("%c\n", c);

    // Default to Yes if just Enter pressed
    if (c == '\n' || c == '\r' || c == 'y' || c == 'Y') {
        return 1;
    }
    return 0;
}

// Set up RAM-based /etc for live mode or as overlay
static void setup_ram_etc(void) {
    mkdir("/ram/etc", 0755);
    mkdir("/ram/etc/skel", 0755);

    if (g_live_mode || !path_exists("/disk/etc/passwd")) {
        // Live mode or first boot without disk init
        tag("[init] ", CLR_CYAN);
        printf("Setting up temporary user database...\n");

        // Standard 7-field format: name:pass:uid:gid:gecos:home:shell
        const char* passwd =
            "root::0:0:root:/root:/bin/dash\n"
            "victor::1000:1000:victor:/home/victor:/bin/dash\n";
        write_file("/ram/etc/passwd", passwd, 0644);

        const char* group =
            "root::0:root\n"
            "victor::1000:victor\n";
        write_file("/ram/etc/group", group, 0644);
    } else {
        // Copy from persistent storage, with fallback if copy fails
        if (copy_file("/disk/etc/passwd", "/ram/etc/passwd") != 0) {
            // Fallback: create default passwd
            const char* default_passwd =
                "root::0:0:System Administrator:/root:/bin/dash\n"
                "victor::1000:1000:Victor:/home/victor:/bin/dash\n";
            write_file("/ram/etc/passwd", default_passwd, 0644);
        }
        if (copy_file("/disk/etc/group", "/ram/etc/group") != 0) {
            // Fallback: create default group
            const char* default_group =
                "root:x:0:\n"
                "victor:x:1000:\n";
            write_file("/ram/etc/group", default_group, 0644);
        }
    }

    // Always create /etc/profile in RAM
    // Note: dash doesn't support bash-style prompt escapes (\u, \w, \[, \])
    // Embed actual ESC character (0x1b) directly for colors
    const char* profile =
        "# /etc/profile\n"
        "export PATH=/bin:/usr/bin:/disk/bin\n"
        "export TERM=xterm\n"
        "if [ \"$USER\" = \"root\" ]; then\n"
        "    PS1='\x1b[1;31mroot\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m# '\n"
        "else\n"
        "    PS1='\x1b[1;32m$USER\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m$ '\n"
        "fi\n"
        "alias ll='ls -la'\n"
        "alias la='ls -A'\n";
    write_file("/ram/etc/profile", profile, 0644);
}

// Setup home directories in RAM for live mode
static void setup_ram_homes(void) {
    mkdir("/ram/home", 0755);
    mkdir("/ram/home/victor", 0755);
    chown("/ram/home/victor", 1000, 1000);

    mkdir("/ram/root", 0700);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initial setup
    mkdir("/tmp", 0777);
    mkdir("/ram/tmp", 0777);

    // Check disk status
    int have_disk = disk_available();
    int need_init = have_disk && !disk_initialized();

    if (need_init) {
        // Blank disk detected - ask user
        if (prompt_disk_init()) {
            initialize_disk();
        } else {
            g_live_mode = 1;
            printf("\n");
            tag("[init] ", CLR_CYAN);
            tag("Booting in Live Mode\n", CLR_YELLOW);
            printf("        (Changes will not be saved)\n\n");
        }
    } else if (!have_disk) {
        g_live_mode = 1;
        tag("[init] ", CLR_CYAN);
        printf("No persistent disk detected, running in Live Mode\n");
    }

    // Set up /etc (RAM overlay)
    setup_ram_etc();

    // Set up home directories
    if (g_live_mode) {
        setup_ram_homes();
    }

    // Clear screen
    printf("\x1b[2J\x1b[H");
    fflush(stdout);

    // Show neofetch
    pid_t neo_pid = fork();
    if (neo_pid == 0) {
        char* const nargv[] = {"/bin/neofetch", NULL};
        execve("/bin/neofetch", nargv, NULL);
        _exit(127);
    } else if (neo_pid > 0) {
        int neo_status = 0;
        (void)waitpid(neo_pid, &neo_status, 0);
    }

    // Show mode indicator
    if (g_live_mode) {
        printf("\n  %s[LIVE MODE]%s Changes will not persist\n", CLR_YELLOW, CLR_RESET);
    }
    printf("\n");
    fflush(stdout);

    // Main loop - spawn login
    for (;;) {
        pid_t pid = fork();
        if (pid < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("fork() for /bin/login failed: %s\n", strerror(errno));
            (void)sys_sleep(1000u);
            continue;
        }

        if (pid == 0) {
            char* const largv[] = {"/bin/login", NULL};
            execve("/bin/login", largv, NULL);
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("execve(/bin/login) failed: %s\n", strerror(errno));
            _exit(127);
        }

        int status = 0;
        pid_t got = waitpid(pid, &status, 0);
        int code = 0;
        if (got >= 0) {
            code = (status >> 8) & 0xFF;
        }
        tag("[init] ", CLR_CYAN);
        printf("Session ended (exit %d), restarting...\n", code);
    }
}
