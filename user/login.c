#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "syscall.h"

// Check /ram/etc/passwd first (set up by init), then /disk/etc/passwd
static const char* const passwd_paths[] = {
    "/ram/etc/passwd",
    "/disk/etc/passwd",
    "/etc/passwd",
    NULL
};

typedef struct user_entry {
    char name[32];
    char pass[64];
    uint32_t uid;
    uint32_t gid;
    char home[128];
    char shell[128];
} user_entry_t;

static void trim_newline(char* s) {
    if (!s) {
        return;
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1u] == '\n' || s[n - 1u] == '\r')) {
        s[n - 1u] = '\0';
        n--;
    }
}

static int parse_u32(const char* s, uint32_t* out) {
    if (!out) {
        return -1;
    }
    *out = 0;
    if (!s || *s == '\0') {
        return -1;
    }
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (!end || end == s) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int parse_passwd_line(char* line, user_entry_t* out) {
    if (!line || !out) {
        return -1;
    }

    // Skip leading whitespace.
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line == '\0' || *line == '#') {
        return -1;
    }

    // Standard passwd format: name:pass:uid:gid:gecos:home:shell (7 fields)
    // Also accept shorter: name:pass:uid:gid:home:shell (6 fields, no gecos)
    char* fields[7] = {0};
    int nf = 0;
    char* p = line;
    for (; nf < 7; nf++) {
        fields[nf] = p;
        char* c = strchr(p, ':');
        if (!c) {
            nf++;  // count the last field
            break;
        }
        *c = '\0';
        p = c + 1;
    }

    if (!fields[0] || fields[0][0] == '\0') {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->name, fields[0], sizeof(out->name) - 1u);
    out->name[sizeof(out->name) - 1u] = '\0';

    if (fields[1]) {
        strncpy(out->pass, fields[1], sizeof(out->pass) - 1u);
        out->pass[sizeof(out->pass) - 1u] = '\0';
    }

    out->uid = 0;
    out->gid = 0;
    if (fields[2]) {
        (void)parse_u32(fields[2], &out->uid);
    }
    if (fields[3]) {
        if (parse_u32(fields[3], &out->gid) != 0) {
            out->gid = out->uid;
        }
    } else {
        out->gid = out->uid;
    }

    // Determine home and shell based on field count
    // 7 fields: name:pass:uid:gid:gecos:home:shell
    // 6 fields: name:pass:uid:gid:home:shell
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
    } else {
        // Not enough fields
        home_field = NULL;
        shell_field = NULL;
    }

    if (home_field && home_field[0] != '\0') {
        strncpy(out->home, home_field, sizeof(out->home) - 1u);
        out->home[sizeof(out->home) - 1u] = '\0';
    } else {
        snprintf(out->home, sizeof(out->home), "/home/%s", out->name);
    }

    if (shell_field && shell_field[0] != '\0') {
        strncpy(out->shell, shell_field, sizeof(out->shell) - 1u);
        out->shell[sizeof(out->shell) - 1u] = '\0';
    } else {
        strncpy(out->shell, "/bin/dash", sizeof(out->shell) - 1u);
        out->shell[sizeof(out->shell) - 1u] = '\0';
    }

    return 0;
}

static int load_user(const char* username, user_entry_t* out) {
    if (!username || !out) {
        return -1;
    }

    // Try each passwd path in order
    for (int i = 0; passwd_paths[i] != NULL; i++) {
        FILE* f = fopen(passwd_paths[i], "r");
        if (!f) {
            continue;
        }

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            trim_newline(line);
            user_entry_t e;
            if (parse_passwd_line(line, &e) != 0) {
                continue;
            }
            if (strcmp(e.name, username) == 0) {
                fclose(f);
                *out = e;
                return 0;
            }
        }

        fclose(f);
    }

    return -1;
}

static int read_line(const char* prompt, char* buf, size_t cap) {
    if (!buf || cap == 0) {
        return -1;
    }
    buf[0] = '\0';

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (!fgets(buf, (int)cap, stdin)) {
        return -1;
    }
    trim_newline(buf);
    return 0;
}

static int read_password(const char* prompt, char* buf, size_t cap) {
    if (!buf || cap == 0) {
        return -1;
    }

    struct termios t;
    if (tcgetattr(0, &t) != 0) {
        return read_line(prompt, buf, cap);
    }

    struct termios noecho = t;
    noecho.c_lflag &= (tcflag_t)~ECHO;
    (void)tcsetattr(0, TCSANOW, &noecho);

    int rc = read_line(prompt, buf, cap);

    (void)tcsetattr(0, TCSANOW, &t);
    fputc('\n', stdout);
    fflush(stdout);
    return rc;
}

static void mkdir_if_missing(const char* path) {
    if (!path || path[0] == '\0') {
        return;
    }
    if (mkdir(path, 0755) == 0) {
        return;
    }
    if (errno == EEXIST) {
        return;
    }
}

static void ensure_home_dir(const char* home) {
    if (!home || home[0] == '\0') {
        return;
    }

    // Best-effort: create /home and /home/<user>.
    mkdir_if_missing("/home");
    mkdir_if_missing(home);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    for (;;) {
        char username[64];
        if (read_line("vos login: ", username, sizeof(username)) != 0) {
            (void)sys_sleep(100u);
            continue;
        }

        if (username[0] == '\0') {
            continue;
        }

        user_entry_t user;
        if (load_user(username, &user) != 0) {
            printf("Login incorrect\n");
            continue;
        }

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

        ensure_home_dir(user.home);
        (void)chdir(user.home);

        // Drop privileges for the session (login is expected to run as uid 0).
        (void)setgid((gid_t)user.gid);
        (void)setuid((uid_t)user.uid);

        // Put the session into its own process group (so job-control style
        // features can work later).
        (void)setpgid(0, 0);

        // Build environment for the shell
        static char env_home[256];
        static char env_user[64];
        static char env_shell[256];
        static char env_term[] = "TERM=xterm-256color";
        static char env_path[] = "PATH=/bin:/usr/bin";

        snprintf(env_home, sizeof(env_home), "HOME=%s", user.home);
        snprintf(env_user, sizeof(env_user), "USER=%s", user.name);
        snprintf(env_shell, sizeof(env_shell), "SHELL=%s", user.shell);

        char* const sh_envp[] = {
            env_home,
            env_user,
            env_shell,
            env_term,
            env_path,
            NULL
        };

        // Create argv[0] with leading '-' to indicate login shell
        // This makes dash read /etc/profile and ~/.profile
        char login_shell[130];
        const char* basename = strrchr(user.shell, '/');
        basename = basename ? basename + 1 : user.shell;
        snprintf(login_shell, sizeof(login_shell), "-%s", basename);

        char* const sh_argv[] = {login_shell, NULL};
        execve(user.shell, sh_argv, sh_envp);
        printf("login: exec %s failed: %s\n", user.shell, strerror(errno));
        return 1;
    }
}
