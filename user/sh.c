#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "linenoise.h"
#include "syscall.h"

#define SHELL_MAX_LINE 512
#define SHELL_MAX_ARGS 32
#define SHELL_ARGV_CAP (SHELL_MAX_ARGS + 1)

typedef struct vos_dirent {
    char name[64];
    unsigned char is_dir;
    unsigned char is_symlink;
    unsigned short mode;
    unsigned int size;
    unsigned short wtime;
    unsigned short wdate;
} vos_dirent_t;

static char g_username[32] = "user";
static char g_home[128] = "/";

static void trim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1u] == '\n' || s[n - 1u] == '\r')) {
        s[n - 1u] = '\0';
        n--;
    }
}

static int parse_u32(const char* s, unsigned int* out) {
    if (!out) return -1;
    *out = 0;
    if (!s || *s == '\0') return -1;
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (!end || end == s) return -1;
    *out = (unsigned int)v;
    return 0;
}

static void resolve_user_identity(void) {
    uid_t uid = getuid();
    if (uid == 0) {
        strncpy(g_username, "root", sizeof(g_username) - 1u);
        g_username[sizeof(g_username) - 1u] = '\0';
        strncpy(g_home, "/home/root", sizeof(g_home) - 1u);
        g_home[sizeof(g_home) - 1u] = '\0';
    }

    FILE* f = fopen("/etc/passwd", "r");
    if (!f) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);

        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        // name:pass:uid:gid:home:shell
        char* fields[6] = {0};
        int nf = 0;
        char* q = p;
        for (; nf < 6; nf++) {
            fields[nf] = q;
            char* c = strchr(q, ':');
            if (!c) break;
            *c = '\0';
            q = c + 1;
        }

        if (!fields[0] || fields[0][0] == '\0') continue;
        if (!fields[2] || fields[2][0] == '\0') continue;

        unsigned int file_uid = 0;
        if (parse_u32(fields[2], &file_uid) != 0) continue;
        if ((uid_t)file_uid != uid) continue;

        strncpy(g_username, fields[0], sizeof(g_username) - 1u);
        g_username[sizeof(g_username) - 1u] = '\0';

        if (fields[4] && fields[4][0] != '\0') {
            strncpy(g_home, fields[4], sizeof(g_home) - 1u);
            g_home[sizeof(g_home) - 1u] = '\0';
        } else {
            snprintf(g_home, sizeof(g_home), "/home/%s", g_username);
        }
        break;
    }

    fclose(f);
}

static int sys_readdir(int fd, vos_dirent_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READDIR), "b"(fd), "c"(out)
        : "memory"
    );
    return ret;
}

static void print_errno(const char* what) {
    if (!what) what = "error";
    printf("\x1b[31;1m%s\x1b[0m: %s\n", what, strerror(errno));
}

static int split_args(char* line, char* argv[], bool quoted[], int max) {
    int argc = 0;
    char* p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (argc >= max) break;

        char quote = 0;
        if (*p == '"' || *p == '\'') {
            quote = *p++;
        }

        argv[argc++] = p;
        if (quoted) {
            quoted[argc - 1] = (quote != 0);
        }

        while (*p) {
            if (quote) {
                if (*p == quote) {
                    *p++ = '\0';
                    break;
                }
            } else {
                if (*p == ' ' || *p == '\t') {
                    *p++ = '\0';
                    break;
                }
            }
            p++;
        }
    }

    return argc;
}

static bool sh_is_glob_pattern(const char* s) {
    if (!s) {
        return false;
    }
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == '[') {
            return true;
        }
    }
    return false;
}

static int sh_str_cmp(const void* a, const void* b) {
    const char* const* sa = (const char* const*)a;
    const char* const* sb = (const char* const*)b;
    return strcmp(*sa, *sb);
}

static int sh_expand_glob_token(const char* tok, char** outv, bool* out_alloc, int out_cap) {
    if (!tok || !outv || !out_alloc || out_cap <= 0) {
        return 0;
    }

    const char* pattern = tok;
    const char* dir = ".";
    char dir_buf[256];
    char base_buf[256];
    const char* slash = strrchr(tok, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - tok);
        if (dlen == 0) {
            dir = "/";
        } else if (dlen < sizeof(dir_buf)) {
            memcpy(dir_buf, tok, dlen);
            dir_buf[dlen] = '\0';
            dir = dir_buf;
        } else {
            return 0;
        }
        pattern = slash + 1;
    }

    if (!sh_is_glob_pattern(pattern)) {
        return 0;
    }

    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return 0;
    }

    char* matches[64];
    int mcount = 0;

    vos_dirent_t de;
    while (mcount < (int)(sizeof(matches) / sizeof(matches[0])) && sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

        // POSIX-ish: don't match dotfiles unless pattern starts with '.'.
        if (de.name[0] == '.' && pattern[0] != '.') {
            continue;
        }

        if (fnmatch(pattern, de.name, 0) != 0) {
            continue;
        }

        if (!slash) {
            matches[mcount] = strdup(de.name);
        } else if (dir[0] == '/' && dir[1] == '\0') {
            int n = snprintf(base_buf, sizeof(base_buf), "/%s", de.name);
            if (n <= 0 || (size_t)n >= sizeof(base_buf)) {
                continue;
            }
            matches[mcount] = strdup(base_buf);
        } else {
            int n = snprintf(base_buf, sizeof(base_buf), "%s/%s", dir, de.name);
            if (n <= 0 || (size_t)n >= sizeof(base_buf)) {
                continue;
            }
            matches[mcount] = strdup(base_buf);
        }

        if (!matches[mcount]) {
            continue;
        }
        mcount++;
    }

    close(fd);

    if (mcount == 0) {
        return 0;
    }

    qsort(matches, (size_t)mcount, sizeof(matches[0]), sh_str_cmp);

    int wrote = 0;
    for (int i = 0; i < mcount && wrote < out_cap; i++) {
        outv[wrote] = matches[i];
        out_alloc[wrote] = true;
        wrote++;
    }

    for (int i = wrote; i < mcount; i++) {
        free(matches[i]);
    }

    return wrote;
}

static int sh_expand_globs(int argc, char** argv, const bool* quoted, char** outv, bool* out_alloc, int out_cap) {
    if (!argv || argc <= 0 || !outv || !out_alloc || out_cap <= 0) {
        return 0;
    }

    int outc = 0;
    for (int i = 0; i < argc && outc < out_cap; i++) {
        const char* tok = argv[i];
        if (!tok || tok[0] == '\0') {
            continue;
        }

        bool is_quoted = (quoted && quoted[i]);
        if (i == 0 || is_quoted || !sh_is_glob_pattern(tok)) {
            outv[outc] = argv[i];
            out_alloc[outc] = false;
            outc++;
            continue;
        }

        int n = sh_expand_glob_token(tok, &outv[outc], &out_alloc[outc], out_cap - outc);
        if (n <= 0) {
            outv[outc] = argv[i];
            out_alloc[outc] = false;
            outc++;
            continue;
        }
        outc += n;
    }

    return outc;
}

static void sh_free_globs(int argc, char** argv, const bool* alloc) {
    if (!argv || !alloc) {
        return;
    }
    for (int i = 0; i < argc; i++) {
        if (alloc[i] && argv[i]) {
            free(argv[i]);
        }
    }
}

static void cmd_help(void) {
    puts("\x1b[36;1mBuilt-ins:\x1b[0m");
    puts("  \x1b[33;1mhelp\x1b[0m               Show this help");
    puts("  \x1b[33;1mexit\x1b[0m               Exit the shell");
    puts("  \x1b[33;1mcd\x1b[0m [dir]            Change directory");
    puts("  \x1b[33;1mpwd\x1b[0m                Print current directory");
    puts("  \x1b[33;1mclear\x1b[0m              Clear the screen");

    puts("");
    puts("\x1b[36;1mPrograms in /bin:\x1b[0m");

    int fd = open("/bin", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        print_errno("open /bin");
        return;
    }

    char names[256][64];
    int count = 0;

    vos_dirent_t de;
    while (count < (int)(sizeof(names) / sizeof(names[0])) && sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (de.is_dir) continue;
        strncpy(names[count], de.name, sizeof(names[count]) - 1u);
        names[count][sizeof(names[count]) - 1u] = '\0';
        count++;
    }
    close(fd);

    // Simple insertion sort for a stable, predictable list.
    for (int i = 1; i < count; i++) {
        char tmp[64];
        strncpy(tmp, names[i], sizeof(tmp));
        tmp[sizeof(tmp) - 1u] = '\0';

        int j = i;
        while (j > 0 && strcmp(names[j - 1], tmp) > 0) {
            strncpy(names[j], names[j - 1], sizeof(names[j]));
            names[j][sizeof(names[j]) - 1u] = '\0';
            j--;
        }
        strncpy(names[j], tmp, sizeof(names[j]));
        names[j][sizeof(names[j]) - 1u] = '\0';
    }

    for (int i = 0; i < count; i++) {
        printf("  %s\n", names[i]);
    }

    puts("");
    puts("\x1b[36;1mPrograms in /usr/bin:\x1b[0m");

    fd = open("/usr/bin", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        // /usr is optional (disk-backed).
        return;
    }

    count = 0;
    while (count < (int)(sizeof(names) / sizeof(names[0])) && sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (de.is_dir) continue;
        strncpy(names[count], de.name, sizeof(names[count]) - 1u);
        names[count][sizeof(names[count]) - 1u] = '\0';
        count++;
    }
    close(fd);

    for (int i = 1; i < count; i++) {
        char tmp2[64];
        strncpy(tmp2, names[i], sizeof(tmp2));
        tmp2[sizeof(tmp2) - 1u] = '\0';

        int j = i;
        while (j > 0 && strcmp(names[j - 1], tmp2) > 0) {
            strncpy(names[j], names[j - 1], sizeof(names[j]));
            names[j][sizeof(names[j]) - 1u] = '\0';
            j--;
        }
        strncpy(names[j], tmp2, sizeof(names[j]));
        names[j][sizeof(names[j]) - 1u] = '\0';
    }

    for (int i = 0; i < count; i++) {
        printf("  %s\n", names[i]);
    }
}

static void cmd_pwd(void) {
    char buf[256];
    if (!getcwd(buf, sizeof(buf))) {
        print_errno("getcwd");
        return;
    }
    puts(buf);
}

static void cmd_cd(int argc, char** argv) {
    const char* dir = (argc >= 2) ? argv[1] : g_home;
    char tmp[256];
    if (dir && dir[0] == '~') {
        if (dir[1] == '\0') {
            dir = g_home;
        } else if (dir[1] == '/') {
            snprintf(tmp, sizeof(tmp), "%s%s", g_home, dir + 1);
            dir = tmp;
        }
    }
    if (chdir(dir) < 0) {
        print_errno("cd");
    }
}

static void cmd_clear(void) {
    // VT100 clear screen + home.
    fputs("\x1b[2J\x1b[H", stdout);
}

static void add_prefixed_completion(linenoiseCompletions* lc, const char* before, const char* completion) {
    if (!lc || !before || !completion) {
        return;
    }
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s%s", before, completion);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) {
        return;
    }
    linenoiseAddCompletion(lc, tmp);
}

static void complete_command_token(const char* before, const char* word, linenoiseCompletions* lc) {
    static const char* builtins[] = {"help", "exit", "cd", "pwd", "ls", "cat", "clear"};

    size_t n = strlen(word);
    for (unsigned int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        const char* b = builtins[i];
        if (strncmp(b, word, n) == 0) {
            add_prefixed_completion(lc, before, b);
        }
    }

    // Complete executables from /bin and /usr/bin.
    const char* prefix = word;
    const char* base = "";
    const char* dir = "/bin";
    char tmp[128];
    if (strncmp(word, "/bin/", 5) == 0) {
        prefix = word + 5;
        base = "/bin/";
        dir = "/bin";
    } else if (strncmp(word, "/usr/bin/", 9) == 0) {
        prefix = word + 9;
        base = "/usr/bin/";
        dir = "/usr/bin";
    }

    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        vos_dirent_t de;
        while (sys_readdir(fd, &de) > 0) {
            if (de.is_dir) continue;
            if (de.name[0] == '\0') continue;
            if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
            snprintf(tmp, sizeof(tmp), "%s%s", base, de.name);
            add_prefixed_completion(lc, before, tmp);
        }
        close(fd);
    }

    // If completing a bare word, also look in /usr/bin.
    if (base[0] == '\0') {
        fd = open("/usr/bin", O_RDONLY | O_DIRECTORY);
        if (fd >= 0) {
            vos_dirent_t de;
            while (sys_readdir(fd, &de) > 0) {
                if (de.is_dir) continue;
                if (de.name[0] == '\0') continue;
                if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
                add_prefixed_completion(lc, before, de.name);
            }
            close(fd);
        }
    }
}

static void complete_path_token(const char* before, const char* tok, linenoiseCompletions* lc, int dirs_only) {
    if (!before || !tok || !lc) {
        return;
    }

    const char* prefix = tok;
    char dir_path[256];
    char base_path[256];
    dir_path[0] = '\0';
    base_path[0] = '\0';

    const char* slash = strrchr(tok, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - tok);
        if (dlen == 0) {
            strcpy(dir_path, "/");
            strcpy(base_path, "/");
        } else {
            if (dlen >= sizeof(dir_path)) {
                return;
            }
            memcpy(dir_path, tok, dlen);
            dir_path[dlen] = '\0';
            snprintf(base_path, sizeof(base_path), "%s/", dir_path);
        }
        prefix = slash + 1;
    } else {
        strcpy(dir_path, ".");
        base_path[0] = '\0';
        prefix = tok;
    }

    int fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return;
    }

    vos_dirent_t de;
    while (sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            // still allow, but only when explicitly requested
            if (prefix[0] != '.') continue;
        }
        if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
        if (dirs_only && !de.is_dir) continue;

        char cand[512];
        if (de.is_dir) {
            snprintf(cand, sizeof(cand), "%s%s%s/", before, base_path, de.name);
        } else {
            snprintf(cand, sizeof(cand), "%s%s%s", before, base_path, de.name);
        }
        linenoiseAddCompletion(lc, cand);
    }

    close(fd);
}

static void completion_cb(const char* buf, linenoiseCompletions* lc) {
    if (!buf || !lc) {
        return;
    }

    size_t len = strlen(buf);
    const char* end = buf + len;

    const char* tok = end;
    while (tok > buf && tok[-1] != ' ' && tok[-1] != '\t') {
        tok--;
    }

    char before[512];
    size_t before_len = (size_t)(tok - buf);
    if (before_len >= sizeof(before)) {
        return;
    }
    memcpy(before, buf, before_len);
    before[before_len] = '\0';

    int word_index = 0;
    bool in_word = false;
    for (const char* p = buf; p < tok; p++) {
        if (*p == ' ' || *p == '\t') {
            in_word = false;
            continue;
        }
        if (!in_word) {
            in_word = true;
            word_index++;
        }
    }

    // Extract the first word (command) to specialize path completion.
    char cmd[64];
    cmd[0] = '\0';
    const char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    size_t ci = 0;
    while (*p && *p != ' ' && *p != '\t' && ci + 1 < sizeof(cmd)) {
        cmd[ci++] = *p++;
    }
    cmd[ci] = '\0';

    if (word_index == 0) {
        complete_command_token(before, tok, lc);
        return;
    }

    int dirs_only = (strcmp(cmd, "cd") == 0);
    complete_path_token(before, tok, lc, dirs_only);
}

static int run_external(int argc, char** argv, bool print_exit) {
    if (argc <= 0 || !argv || !argv[0]) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_errno("fork");
        return -1;
    }

    if (pid == 0) {
        // Child: put ourselves in a fresh process group for job control.
        (void)setpgid(0, 0);

        // Resolve command path: try relative first, then /bin/<cmd>, then /usr/bin/<cmd>.
        char pathbuf[256];
        const char* cmd = argv[0];
        const char* exec_path = cmd;

        if (strchr(cmd, '/') == NULL) {
            execve(exec_path, argv, NULL);

            snprintf(pathbuf, sizeof(pathbuf), "/bin/%s", cmd);
            exec_path = pathbuf;
            argv[0] = (char*)exec_path;
            execve(exec_path, argv, NULL);

            snprintf(pathbuf, sizeof(pathbuf), "/usr/bin/%s", cmd);
            exec_path = pathbuf;
            argv[0] = (char*)exec_path;
            execve(exec_path, argv, NULL);
        } else {
            execve(exec_path, argv, NULL);
        }

        // If we got here, exec failed.
        fprintf(stderr, "\x1b[31;1m%s\x1b[0m: %s\n", cmd, strerror(errno));
        _exit(127);
    }

    // Parent: also try to move the child into its own group (racy but safe).
    (void)setpgid(pid, pid);

    // Make the child the terminal foreground process group so Ctrl+C can stop it.
    int fg = (int)pid;
    (void)ioctl(0, TIOCSPGRP, &fg);

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);

    // Restore "no foreground process" while at the prompt.
    int none = 0;
    (void)ioctl(0, TIOCSPGRP, &none);

    int code = 0;
    if (w < 0) {
        print_errno("waitpid");
        code = 127;
    } else {
        code = (status >> 8) & 0xFF;
    }

    if (print_exit) {
        printf("exit %d\n", code);
    }
    return 0;
}

typedef struct sh_redir {
    const char* in_path;
    const char* out_path;
    const char* err_path;
    bool out_append;
    bool err_append;
} sh_redir_t;

static bool sh_parse_redir_token(const char* tok, sh_redir_t* r, const char** out_path, bool* out_append) {
    if (!tok || !r || !out_path || !out_append) {
        return false;
    }

    *out_path = NULL;
    *out_append = false;

    if (tok[0] == '\0') {
        return false;
    }

    if (tok[0] == '<') {
        *out_path = tok[1] ? (tok + 1) : NULL;
        return true;
    }

    if (tok[0] == '>') {
        if (tok[1] == '>') {
            *out_append = true;
            *out_path = tok[2] ? (tok + 2) : NULL;
        } else {
            *out_path = tok[1] ? (tok + 1) : NULL;
        }
        return true;
    }

    if (tok[0] == '2' && tok[1] == '>') {
        if (tok[2] == '>') {
            *out_append = true;
            *out_path = tok[3] ? (tok + 3) : NULL;
        } else {
            *out_path = tok[2] ? (tok + 2) : NULL;
        }
        return true;
    }

    return false;
}

static int sh_apply_redirections(const sh_redir_t* r, int saved[3]) {
    saved[0] = -1;
    saved[1] = -1;
    saved[2] = -1;

    if (!r) {
        return 0;
    }

    if (r->in_path) {
        saved[0] = dup(STDIN_FILENO);
        if (saved[0] < 0) {
            return -1;
        }
        int fd = open(r->in_path, O_RDONLY);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (r->out_path) {
        saved[1] = dup(STDOUT_FILENO);
        if (saved[1] < 0) {
            return -1;
        }
        int flags = O_WRONLY | O_CREAT | (r->out_append ? O_APPEND : O_TRUNC);
        int fd = open(r->out_path, flags, 0666);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (r->err_path) {
        saved[2] = dup(STDERR_FILENO);
        if (saved[2] < 0) {
            return -1;
        }
        int flags = O_WRONLY | O_CREAT | (r->err_append ? O_APPEND : O_TRUNC);
        int fd = open(r->err_path, flags, 0666);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static void sh_restore_redirections(const int saved[3]) {
    if (saved[0] >= 0) {
        (void)dup2(saved[0], STDIN_FILENO);
        close(saved[0]);
    }
    if (saved[1] >= 0) {
        (void)dup2(saved[1], STDOUT_FILENO);
        close(saved[1]);
    }
    if (saved[2] >= 0) {
        (void)dup2(saved[2], STDERR_FILENO);
        close(saved[2]);
    }
}

static int sh_execute_argv(int argc, char** argv, bool print_exit) {
    if (argc <= 0) {
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        return 1;
    }
    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        cmd_cd(argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        cmd_pwd();
        return 0;
    }
    if (strcmp(argv[0], "clear") == 0) {
        cmd_clear();
        return 0;
    }

    (void)run_external(argc, argv, print_exit);
    return 0;
}

static int sh_execute_line(char* line, bool print_exit) {
    if (!line) {
        return 0;
    }

    char* av_raw[SHELL_MAX_ARGS];
    bool q_raw[SHELL_MAX_ARGS];
    int ac_raw = split_args(line, av_raw, q_raw, SHELL_MAX_ARGS);
    if (ac_raw <= 0) {
        return 0;
    }

    sh_redir_t redir = {0};
    char* av[SHELL_MAX_ARGS];
    bool q_av[SHELL_MAX_ARGS];
    int ac = 0;

    for (int i = 0; i < ac_raw; i++) {
        char* tok = av_raw[i];
        if (!tok || tok[0] == '\0') {
            continue;
        }

        if (strcmp(tok, "(") == 0 || strcmp(tok, ")") == 0) {
            continue;
        }

        const char* path = NULL;
        bool append = false;
        if (sh_parse_redir_token(tok, &redir, &path, &append)) {
            if (tok[0] == '<') {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.in_path = av_raw[++i];
                    }
                } else {
                    redir.in_path = path;
                }
            } else if (tok[0] == '2') {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.err_path = av_raw[++i];
                    }
                } else {
                    redir.err_path = path;
                }
                redir.err_append = append;
            } else {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.out_path = av_raw[++i];
                    }
                } else {
                    redir.out_path = path;
                }
                redir.out_append = append;
            }
            continue;
        }

        if (ac < SHELL_MAX_ARGS) {
            av[ac++] = tok;
            q_av[ac - 1] = q_raw[i];
        }
    }

    if (ac <= 0) {
        return 0;
    }

    char* av_exp[SHELL_ARGV_CAP];
    bool av_alloc[SHELL_ARGV_CAP];
    int ac_exp = sh_expand_globs(ac, av, q_av, av_exp, av_alloc, SHELL_MAX_ARGS);
    if (ac_exp <= 0) {
        return 0;
    }
    av_exp[ac_exp] = NULL;

    int saved[3];
    if (sh_apply_redirections(&redir, saved) != 0) {
        print_errno("redirect");
        sh_restore_redirections(saved);
        sh_free_globs(ac_exp, av_exp, av_alloc);
        return 0;
    }

    int rc = sh_execute_argv(ac_exp, av_exp, print_exit);
    sh_restore_redirections(saved);
    sh_free_globs(ac_exp, av_exp, av_alloc);
    return rc;
}

int main(int argc, char** argv) {
    linenoiseSetCompletionCallback(completion_cb);
    linenoiseHistorySetMaxLen(128);

    resolve_user_identity();

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        char buf[SHELL_MAX_LINE];
        strncpy(buf, argv[2], sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';
        return sh_execute_line(buf, false) ? 1 : 0;
    }

    puts("\x1b[36;1mVOS user shell\x1b[0m (linenoise). Type '\x1b[33;1mhelp\x1b[0m' for help.");

    char cwd[256];
    for (;;) {
        if (!getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, "/");
        }

        char display_cwd[256];
        const char* shown = cwd;
        if (g_home[0] == '/' && g_home[1] != '\0') {
            size_t hl = strlen(g_home);
            if (strncmp(cwd, g_home, hl) == 0 && (cwd[hl] == '\0' || cwd[hl] == '/')) {
                snprintf(display_cwd, sizeof(display_cwd), "~%s", cwd + hl);
                shown = display_cwd;
            }
        }

        char prompt[320];
        snprintf(prompt, sizeof(prompt), "%s@vos:%s$ ", g_username, shown);

        // Enable terminal mouse reporting while we are inside linenoise so clicks
        // can move the cursor and the wheel can navigate history.
        fputs("\x1b[?1000h\x1b[?1006h", stdout);
        fflush(stdout);
        char* line = linenoise(prompt);
        fputs("\x1b[?1000l\x1b[?1006l", stdout);
        fflush(stdout);
        if (!line) {
            break;
        }

        // Trim leading whitespace.
        char* s = line;
        while (*s == ' ' || *s == '\t') s++;

        if (*s == '\0') {
            free(line);
            continue;
        }

        linenoiseHistoryAdd(s);

        char buf[SHELL_MAX_LINE];
        strncpy(buf, s, sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';
        if (sh_execute_line(buf, true)) {
            free(line);
            break;
        }

        free(line);
    }

    return 0;
}
