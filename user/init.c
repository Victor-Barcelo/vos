#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TB_IMPL
#include <termbox2.h>

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

    // Use 32KB buffer for faster copying
    static char buf[32768];
    ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(dfd, buf + written, n - written);
            if (w <= 0) break;
            written += w;
        }
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

// Recursively copy a directory tree
static int copy_tree(const char* src, const char* dst) {
    struct stat st;
    if (stat(src, &st) < 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        mkdir(dst, st.st_mode & 0777);
        DIR* d = opendir(src);
        if (!d) return -1;

        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;

            char srcpath[256], dstpath[256];
            snprintf(srcpath, sizeof(srcpath), "%s/%s", src, ent->d_name);
            snprintf(dstpath, sizeof(dstpath), "%s/%s", dst, ent->d_name);
            copy_tree(srcpath, dstpath);
        }
        closedir(d);
    } else if (S_ISREG(st.st_mode)) {
        copy_file_mode(src, dst, st.st_mode & 0777);
    }
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

// ============================================================================
// Termbox2-based Installer UI
// ============================================================================

// Installer colors
#define INS_TITLE    TB_CYAN | TB_BOLD
#define INS_BOX      TB_BLUE | TB_BOLD
#define INS_TEXT     TB_WHITE
#define INS_DONE     TB_GREEN | TB_BOLD
#define INS_ACTIVE   TB_YELLOW | TB_BOLD
#define INS_PENDING  TB_WHITE
#define INS_BAR_FULL TB_GREEN
#define INS_BAR_EMPTY TB_WHITE
#define INS_STATUS   TB_CYAN

// Installation steps
#define INS_STEP_DIRS     0
#define INS_STEP_BINARIES 1
#define INS_STEP_DEVTOOLS 2
#define INS_STEP_EXTRAS   3
#define INS_STEP_USERS    4
#define INS_STEP_CONFIG   5
#define INS_STEP_HOMES    6
#define INS_STEP_FINALIZE 7
#define INS_STEP_COUNT    8

static const char* ins_step_names[INS_STEP_COUNT] = {
    "Creating directory structure",
    "Copying system binaries",
    "Installing development tools",
    "Installing extras",
    "Creating user accounts",
    "Creating system configuration",
    "Setting up home directories",
    "Finalizing installation"
};

static int ins_current_step = -1;
static int ins_width = 0;
static int ins_height = 0;
static char ins_status_msg[128] = "";

// Draw a horizontal line
static void ins_hline(int x, int y, int len, char ch, uintattr_t fg) {
    for (int i = 0; i < len; i++) {
        tb_set_cell(x + i, y, ch, fg, TB_DEFAULT);
    }
}

// Draw a string at position
static void ins_str(int x, int y, uintattr_t fg, const char* str) {
    tb_print(x, y, fg, TB_DEFAULT, str);
}

// Draw formatted string
static void ins_fmt(int x, int y, uintattr_t fg, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tb_print(x, y, fg, TB_DEFAULT, buf);
}

// Draw the installer frame
static void ins_draw_frame(void) {
    tb_clear();

    // Top border
    tb_set_cell(0, 0, '+', INS_BOX, TB_DEFAULT);
    ins_hline(1, 0, ins_width - 2, '=', INS_BOX);
    tb_set_cell(ins_width - 1, 0, '+', INS_BOX, TB_DEFAULT);

    // Title line
    tb_set_cell(0, 1, '|', INS_BOX, TB_DEFAULT);
    const char* title = "VOS DISK INITIALIZATION";
    int title_x = (ins_width - (int)strlen(title)) / 2;
    ins_str(title_x, 1, INS_TITLE, title);
    tb_set_cell(ins_width - 1, 1, '|', INS_BOX, TB_DEFAULT);

    // Title separator
    tb_set_cell(0, 2, '+', INS_BOX, TB_DEFAULT);
    ins_hline(1, 2, ins_width - 2, '=', INS_BOX);
    tb_set_cell(ins_width - 1, 2, '+', INS_BOX, TB_DEFAULT);

    // Side borders
    for (int y = 3; y < ins_height - 3; y++) {
        tb_set_cell(0, y, '|', INS_BOX, TB_DEFAULT);
        tb_set_cell(ins_width - 1, y, '|', INS_BOX, TB_DEFAULT);
    }

    // Bottom separator
    tb_set_cell(0, ins_height - 3, '+', INS_BOX, TB_DEFAULT);
    ins_hline(1, ins_height - 3, ins_width - 2, '=', INS_BOX);
    tb_set_cell(ins_width - 1, ins_height - 3, '+', INS_BOX, TB_DEFAULT);

    // Footer area
    tb_set_cell(0, ins_height - 2, '|', INS_BOX, TB_DEFAULT);
    tb_set_cell(ins_width - 1, ins_height - 2, '|', INS_BOX, TB_DEFAULT);

    // Bottom border
    tb_set_cell(0, ins_height - 1, '+', INS_BOX, TB_DEFAULT);
    ins_hline(1, ins_height - 1, ins_width - 2, '=', INS_BOX);
    tb_set_cell(ins_width - 1, ins_height - 1, '+', INS_BOX, TB_DEFAULT);
}

// Draw progress bar
static void ins_draw_progress(int step) {
    int y = 5;
    int bar_width = ins_width - 16;  // Leave room for percentage display
    int filled = (step * bar_width) / INS_STEP_COUNT;
    int pct = (step * 100) / INS_STEP_COUNT;

    ins_str(3, y, INS_TEXT, "Progress:");

    tb_set_cell(3, y + 1, '[', INS_TEXT, TB_DEFAULT);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            tb_set_cell(4 + i, y + 1, '#', INS_BAR_FULL, TB_DEFAULT);
        } else {
            tb_set_cell(4 + i, y + 1, '-', INS_BAR_EMPTY, TB_DEFAULT);
        }
    }
    tb_set_cell(4 + bar_width, y + 1, ']', INS_TEXT, TB_DEFAULT);

    ins_fmt(6 + bar_width, y + 1, INS_TEXT, " %3d%%", pct);
}

// Draw step list
static void ins_draw_steps(int current) {
    int y = 9;
    ins_str(3, y, INS_TEXT, "Steps:");
    y += 2;

    for (int i = 0; i < INS_STEP_COUNT; i++) {
        const char* marker;
        uintattr_t color;

        if (i < current) {
            marker = "[DONE]";
            color = INS_DONE;
        } else if (i == current) {
            marker = "[>>>>]";
            color = INS_ACTIVE;
        } else {
            marker = "[    ]";
            color = INS_PENDING;
        }

        ins_str(5, y + i, color, marker);
        ins_str(12, y + i, color, ins_step_names[i]);
    }
}

// Draw status message
static void ins_draw_status(void) {
    int y = ins_height - 5;

    // Clear status area
    for (int x = 2; x < ins_width - 2; x++) {
        tb_set_cell(x, y, ' ', TB_DEFAULT, TB_DEFAULT);
    }

    if (ins_status_msg[0]) {
        ins_fmt(3, y, INS_STATUS, "Current: %s", ins_status_msg);
    }
}

// Draw footer
static void ins_draw_footer(void) {
    const char* footer = "Please wait while VOS is being installed...";
    int x = (ins_width - (int)strlen(footer)) / 2;
    ins_str(x, ins_height - 2, INS_TEXT, footer);
}

// Update the installer display
static void ins_update(int step, const char* status) {
    ins_current_step = step;
    if (status) {
        strncpy(ins_status_msg, status, sizeof(ins_status_msg) - 1);
        ins_status_msg[sizeof(ins_status_msg) - 1] = '\0';
    }

    ins_draw_frame();
    ins_draw_progress(step);
    ins_draw_steps(step);
    ins_draw_status();
    ins_draw_footer();
    tb_present();
}

// Initialize termbox installer UI
static int ins_init(void) {
    if (tb_init() != 0) {
        return -1;
    }
    ins_width = tb_width();
    ins_height = tb_height();
    return 0;
}

// Shutdown termbox installer UI
static void ins_shutdown(void) {
    tb_shutdown();
}

// Show completion screen
static void ins_complete(void) {
    ins_draw_frame();
    ins_draw_progress(INS_STEP_COUNT);
    ins_draw_steps(INS_STEP_COUNT);

    int y = ins_height - 5;
    const char* msg = "Installation complete!";
    int x = (ins_width - (int)strlen(msg)) / 2;
    ins_str(x, y, INS_DONE, msg);

    const char* footer = "Starting VOS...";
    x = (ins_width - (int)strlen(footer)) / 2;
    ins_str(x, ins_height - 2, INS_TEXT, footer);

    tb_present();
    usleep(1500000);  // Show for 1.5 seconds
}

// ============================================================================
// End Installer UI
// ============================================================================

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

// Full disk initialization with termbox2 UI
static void initialize_disk(void) {
    struct stat st;

    // Try to initialize termbox UI
    int use_ui = (ins_init() == 0);

    if (use_ui) {
        // === STEP 0: Creating directory structure ===
        ins_update(INS_STEP_DIRS, "Creating /bin, /etc, /home...");

        mkdir("/disk/bin", 0755);
        mkdir("/disk/etc", 0755);
        mkdir("/disk/home", 0755);
        mkdir("/disk/root", 0700);
        mkdir("/disk/tmp", 01777);
        mkdir("/disk/var", 0755);
        mkdir("/disk/var/log", 0755);
        mkdir("/disk/var/tmp", 01777);
        mkdir("/disk/usr", 0755);
        mkdir("/disk/usr/bin", 0755);
        mkdir("/disk/usr/lib", 0755);
        mkdir("/disk/usr/share", 0755);
        mkdir("/disk/usr/dev", 0755);
        mkdir("/disk/usr/dev/game", 0755);
        mkdir("/disk/usr/dev/game/doc", 0755);
        mkdir("/disk/usr/dev/game/examples", 0755);
        mkdir("/disk/usr/game", 0755);
        mkdir("/disk/usr/game/roms", 0755);

        // === STEP 1: Copying binaries ===
        ins_update(INS_STEP_BINARIES, "Copying system binaries...");

        DIR* d = opendir("/bin");
        if (d) {
            struct dirent* ent;
            int file_count = 0;
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.') continue;

                char srcpath[256], dstpath[256];
                snprintf(srcpath, sizeof(srcpath), "/bin/%s", ent->d_name);
                snprintf(dstpath, sizeof(dstpath), "/disk/bin/%s", ent->d_name);

                // Only update UI every 10 files to reduce overhead
                if (file_count % 10 == 0) {
                    ins_update(INS_STEP_BINARIES, ent->d_name);
                }

                if (stat(srcpath, &st) == 0 && S_ISREG(st.st_mode)) {
                    copy_file_mode(srcpath, dstpath, st.st_mode | 0111);
                    file_count++;
                }
            }
            closedir(d);
        }

        // === STEP 2: Development tools ===
        ins_update(INS_STEP_DEVTOOLS, "Installing TCC, libc, headers...");

        if (stat("/sysroot", &st) == 0) {
            copy_tree("/sysroot/usr", "/disk/usr");
        }

        // === STEP 3: Extras (ROMs + klystrack) ===
        ins_update(INS_STEP_EXTRAS, "Installing game ROMs...");

        if (stat("/res/roms", &st) == 0) {
            copy_tree("/res/roms", "/disk/usr/game/roms");
        }

        ins_update(INS_STEP_EXTRAS, "Installing klystrack resources...");

        if (stat("/res/klystrack", &st) == 0) {
            mkdir("/disk/res", 0755);
            mkdir("/disk/res/klystrack", 0755);
            mkdir("/disk/res/klystrack/res", 0755);
            mkdir("/disk/res/klystrack/key", 0755);
            copy_tree("/res/klystrack/res", "/disk/res/klystrack/res");
            copy_tree("/res/klystrack/key", "/disk/res/klystrack/key");
        }

        // === STEP 4: User accounts ===
        ins_update(INS_STEP_USERS, "Creating root and victor users...");

        const char* passwd =
            "root::0:0:System Administrator:/root:/bin/dash\n"
            "victor::1000:1000:Victor:/home/victor:/bin/dash\n";
        write_file("/disk/etc/passwd", passwd, 0644);

        const char* group =
            "root::0:root\n"
            "wheel::10:root,victor\n"
            "users::100:victor\n"
            "victor::1000:victor\n";
        write_file("/disk/etc/group", group, 0644);

        const char* shadow =
            "root::0:0:99999:7:::\n"
            "victor::0:0:99999:7:::\n";
        write_file("/disk/etc/shadow", shadow, 0600);

        // === STEP 5: System configuration ===
        ins_update(INS_STEP_CONFIG, "Writing /etc/profile, /etc/motd...");

        const char* profile =
            "# /etc/profile - system-wide shell configuration\n"
            "\n"
            "export PATH=/bin:/usr/bin:/disk/bin\n"
            "export TERM=xterm\n"
            "export EDITOR=vi\n"
            "\n"
            "if [ \"$USER\" = \"root\" ]; then\n"
            "    PS1='\x1b[1;31mroot\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m# '\n"
            "else\n"
            "    PS1='\x1b[1;32m$USER\x1b[0m@\x1b[1;36mvos\x1b[0m:\x1b[1;33m$PWD\x1b[0m$ '\n"
            "fi\n"
            "\n"
            "alias ll='ls -la'\n"
            "alias la='ls -A'\n"
            "alias l='ls -l'\n"
            "alias ..='cd ..'\n"
            "alias ...='cd ../..'\n"
            "\n"
            "if [ -f \"$HOME/.profile\" ]; then\n"
            "    . \"$HOME/.profile\"\n"
            "fi\n";
        write_file("/disk/etc/profile", profile, 0644);

        const char* motd =
            "\n"
            "  Welcome to VOS!\n"
            "\n"
            "  Type 'help' for available commands.\n"
            "  Your files are stored in /home/<username>\n"
            "\n";
        write_file("/disk/etc/motd", motd, 0644);

        write_file("/disk/etc/hostname", "vos\n", 0644);

        const char* issue =
            "\\e[1;36mVOS\\e[0m 0.1.0 - \\l\n"
            "\n";
        write_file("/disk/etc/issue", issue, 0644);

        // === STEP 6: Home directories ===
        ins_update(INS_STEP_HOMES, "Setting up /root and /home/victor...");

        mkdir("/disk/root", 0700);
        const char* root_profile =
            "# ~/.profile - root shell configuration\n"
            "export HOME=/root\n"
            "cd $HOME\n";
        write_file("/disk/root/.profile", root_profile, 0644);

        mkdir("/disk/home/victor", 0755);
        const char* user_profile =
            "# ~/.profile - user shell configuration\n"
            "cd $HOME\n";
        write_file("/disk/home/victor/.profile", user_profile, 0644);

        chown("/disk/home/victor", 1000, 1000);
        chown("/disk/home/victor/.profile", 1000, 1000);

        // === STEP 7: Finalize ===
        ins_update(INS_STEP_FINALIZE, "Writing initialization marker...");

        write_file("/disk/.vos-initialized",
            "VOS initialized\n"
            "Version: 0.1.0\n", 0644);

        // Show completion
        ins_complete();
        ins_shutdown();

    } else {
        // Fallback to printf-based output if termbox fails
        printf("\n");
        tag("[setup] ", CLR_CYAN);
        printf("Initializing VOS disk...\n\n");

        tag("[setup] ", CLR_CYAN);
        printf("Creating directory structure...\n");

        mkdir("/disk/bin", 0755);
        mkdir("/disk/etc", 0755);
        mkdir("/disk/home", 0755);
        mkdir("/disk/root", 0700);
        mkdir("/disk/tmp", 01777);
        mkdir("/disk/var", 0755);
        mkdir("/disk/var/log", 0755);
        mkdir("/disk/var/tmp", 01777);
        mkdir("/disk/usr", 0755);
        mkdir("/disk/usr/bin", 0755);
        mkdir("/disk/usr/lib", 0755);
        mkdir("/disk/usr/share", 0755);
        mkdir("/disk/usr/dev", 0755);
        mkdir("/disk/usr/dev/game", 0755);
        mkdir("/disk/usr/dev/game/doc", 0755);
        mkdir("/disk/usr/dev/game/examples", 0755);
        mkdir("/disk/usr/game", 0755);
        mkdir("/disk/usr/game/roms", 0755);

        copy_binaries();

        if (stat("/sysroot", &st) == 0) {
            tag("[setup] ", CLR_CYAN);
            printf("Installing development tools...\n");
            copy_tree("/sysroot/usr", "/disk/usr");
        }

        if (stat("/res/roms", &st) == 0) {
            tag("[setup] ", CLR_CYAN);
            printf("Installing extras...\n");
            copy_tree("/res/roms", "/disk/usr/game/roms");
        }

        if (stat("/res/klystrack", &st) == 0) {
            mkdir("/disk/res", 0755);
            mkdir("/disk/res/klystrack", 0755);
            mkdir("/disk/res/klystrack/res", 0755);
            mkdir("/disk/res/klystrack/key", 0755);
            copy_tree("/res/klystrack/res", "/disk/res/klystrack/res");
            copy_tree("/res/klystrack/key", "/disk/res/klystrack/key");
        }

        create_default_users();
        create_system_config();
        create_home_directories();

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

    // Pivot root: make MinixFS the root filesystem
    // After this, / is the disk and /initramfs has the boot files
    if (!g_live_mode && have_disk) {
        if (sys_pivot_root() == 0) {
            tag("[init] ", CLR_CYAN);
            tag("pivot_root: ", CLR_GREEN);
            printf("MinixFS is now root filesystem\n");
        }
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

    // Spawn login on all 4 virtual consoles
    #define NUM_CONSOLES 4
    pid_t console_pids[NUM_CONSOLES] = {0};

    for (int i = 0; i < NUM_CONSOLES; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("fork() for console %d failed: %s\n", i + 1, strerror(errno));
            continue;
        }

        if (pid == 0) {
            sys_set_console(i);
            char* const largv[] = {"/bin/login", NULL};
            execve("/bin/login", largv, NULL);
            _exit(127);
        }

        console_pids[i] = pid;
    }

    // Main loop - wait for any child to exit and respawn on that console
    for (;;) {
        int status = 0;
        pid_t got = waitpid(-1, &status, 0);

        if (got <= 0) {
            (void)sys_sleep(100u);
            continue;
        }

        int exited_console = -1;
        for (int i = 0; i < NUM_CONSOLES; i++) {
            if (console_pids[i] == got) {
                exited_console = i;
                console_pids[i] = 0;
                break;
            }
        }

        if (exited_console >= 0) {
            (void)sys_sleep(100u);

            pid_t pid = fork();
            if (pid < 0) continue;

            if (pid == 0) {
                sys_set_console(exited_console);
                char* const largv[] = {"/bin/login", NULL};
                execve("/bin/login", largv, NULL);
                _exit(127);
            }

            console_pids[exited_console] = pid;
        }
    }
}
