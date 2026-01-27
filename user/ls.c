/*
 * ls - list directory contents with color support
 *
 * A colorized ls implementation for VOS with vibrant ANSI color output.
 */

#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

/* ANSI escape codes */
#define ESC "\x1b["
#define RESET ESC "0m"

/* Text styles */
#define BOLD ESC "1m"
#define DIM ESC "2m"
#define ITALIC ESC "3m"
#define UNDERLINE ESC "4m"

/* Foreground colors (bright versions for better visibility) */
#define FG_BLACK      ESC "30m"
#define FG_RED        ESC "31m"
#define FG_GREEN      ESC "32m"
#define FG_YELLOW     ESC "33m"
#define FG_BLUE       ESC "34m"
#define FG_MAGENTA    ESC "35m"
#define FG_CYAN       ESC "36m"
#define FG_WHITE      ESC "37m"

/* Bright foreground colors */
#define FG_BRED       ESC "91m"
#define FG_BGREEN     ESC "92m"
#define FG_BYELLOW    ESC "93m"
#define FG_BBLUE      ESC "94m"
#define FG_BMAGENTA   ESC "95m"
#define FG_BCYAN      ESC "96m"
#define FG_BWHITE     ESC "97m"

/* Background colors */
#define BG_RED        ESC "41m"
#define BG_GREEN      ESC "42m"
#define BG_YELLOW     ESC "43m"

struct entry {
    char *name;
    struct stat st;
    char *link_target;
    int link_ok;
};

static int opt_all = 0;
static int opt_almost = 0;
static int opt_long = 0;
static int opt_human = 0;
static int opt_reverse = 0;
static int opt_time = 0;
static int opt_size = 0;
static int opt_classify = 0;
static int opt_recursive = 0;
static int opt_inode = 0;
static int opt_nocolor = 0;
static int opt_oneline = 0;
static int opt_dir = 0;

static int first_output = 1;

static int has_ext(const char *name, const char *ext) {
    size_t nlen = strlen(name);
    size_t elen = strlen(ext);
    if (nlen < elen) return 0;
    return strcasecmp(name + nlen - elen, ext) == 0;
}

static int is_archive(const char *n) {
    return has_ext(n,".tar") || has_ext(n,".gz") || has_ext(n,".tgz") ||
           has_ext(n,".bz2") || has_ext(n,".xz") || has_ext(n,".zip") ||
           has_ext(n,".rar") || has_ext(n,".7z") || has_ext(n,".iso") ||
           has_ext(n,".tar.gz") || has_ext(n,".tar.xz");
}

static int is_image(const char *n) {
    return has_ext(n,".png") || has_ext(n,".jpg") || has_ext(n,".jpeg") ||
           has_ext(n,".gif") || has_ext(n,".bmp") || has_ext(n,".ico") ||
           has_ext(n,".svg") || has_ext(n,".webp") || has_ext(n,".ppm");
}

static int is_media(const char *n) {
    return has_ext(n,".mp3") || has_ext(n,".mp4") || has_ext(n,".mkv") ||
           has_ext(n,".avi") || has_ext(n,".wav") || has_ext(n,".ogg") ||
           has_ext(n,".flac") || has_ext(n,".webm") || has_ext(n,".mov") ||
           has_ext(n,".nes") || has_ext(n,".gb") || has_ext(n,".rom");
}

static int is_source(const char *n) {
    return has_ext(n,".c") || has_ext(n,".cpp") || has_ext(n,".py") ||
           has_ext(n,".js") || has_ext(n,".rs") || has_ext(n,".go") ||
           has_ext(n,".java") || has_ext(n,".sh") || has_ext(n,".asm");
}

static int is_header(const char *n) {
    return has_ext(n,".h") || has_ext(n,".hpp") || has_ext(n,".hh");
}

static int is_config(const char *n) {
    return has_ext(n,".conf") || has_ext(n,".cfg") || has_ext(n,".ini") ||
           has_ext(n,".json") || has_ext(n,".xml") || has_ext(n,".yaml") ||
           has_ext(n,".yml") || has_ext(n,".toml");
}

static int is_doc(const char *n) {
    return has_ext(n,".txt") || has_ext(n,".md") || has_ext(n,".pdf") ||
           has_ext(n,".doc") || has_ext(n,".html") || has_ext(n,".htm");
}

static int is_object(const char *n) {
    return has_ext(n,".o") || has_ext(n,".a") || has_ext(n,".so") ||
           has_ext(n,".elf") || has_ext(n,".obj") || has_ext(n,".bin");
}

/* Get color string for entry - returns start color code */
static void print_colored_name(const struct entry *e) {
    if (opt_nocolor) {
        fputs(e->name, stdout);
        return;
    }

    mode_t m = e->st.st_mode;
    const char *name = e->name;

    /* Directory - bold bright cyan */
    if (S_ISDIR(m)) {
        printf(BOLD FG_BCYAN "%s" RESET, name);
        return;
    }

    /* Symlink - bold bright magenta */
    if (S_ISLNK(m)) {
        if (e->link_ok)
            printf(BOLD FG_BMAGENTA "%s" RESET, name);
        else
            printf(BOLD FG_BRED "%s" RESET, name);  /* broken link */
        return;
    }

    /* Special files */
    if (S_ISFIFO(m)) {
        printf(FG_BYELLOW "%s" RESET, name);
        return;
    }
    if (S_ISSOCK(m)) {
        printf(FG_BMAGENTA "%s" RESET, name);
        return;
    }
    if (S_ISBLK(m) || S_ISCHR(m)) {
        printf(BOLD FG_BYELLOW "%s" RESET, name);
        return;
    }

    /* Setuid/setgid */
    if (S_ISREG(m) && (m & S_ISUID)) {
        printf(BOLD FG_WHITE BG_RED "%s" RESET, name);
        return;
    }
    if (S_ISREG(m) && (m & S_ISGID)) {
        printf(BOLD FG_BLACK BG_YELLOW "%s" RESET, name);
        return;
    }

    /* Executable - bold bright green */
    if (S_ISREG(m) && (m & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        printf(BOLD FG_BGREEN "%s" RESET, name);
        return;
    }

    /* By extension */
    if (is_archive(name)) {
        printf(BOLD FG_BRED "%s" RESET, name);
        return;
    }
    if (is_image(name)) {
        printf(BOLD FG_BMAGENTA "%s" RESET, name);
        return;
    }
    if (is_media(name)) {
        printf(FG_BMAGENTA "%s" RESET, name);
        return;
    }
    if (is_source(name)) {
        printf(FG_BGREEN "%s" RESET, name);
        return;
    }
    if (is_header(name)) {
        printf(FG_BCYAN "%s" RESET, name);
        return;
    }
    if (is_config(name)) {
        printf(FG_BYELLOW "%s" RESET, name);
        return;
    }
    if (is_doc(name)) {
        printf(FG_BWHITE "%s" RESET, name);
        return;
    }
    if (is_object(name)) {
        printf(DIM "%s" RESET, name);
        return;
    }

    /* Regular file - default color */
    fputs(name, stdout);
}

/* Print indicator with color */
static void print_indicator(mode_t m) {
    if (!opt_classify) return;

    if (opt_nocolor) {
        if (S_ISDIR(m)) putchar('/');
        else if (S_ISLNK(m)) putchar('@');
        else if (S_ISFIFO(m)) putchar('|');
        else if (S_ISSOCK(m)) putchar('=');
        else if (S_ISREG(m) && (m & (S_IXUSR|S_IXGRP|S_IXOTH))) putchar('*');
        return;
    }

    /* Colored indicators */
    if (S_ISDIR(m))
        printf(BOLD FG_BCYAN "/" RESET);
    else if (S_ISLNK(m))
        printf(BOLD FG_BMAGENTA "@" RESET);
    else if (S_ISFIFO(m))
        printf(FG_BYELLOW "|" RESET);
    else if (S_ISSOCK(m))
        printf(FG_BMAGENTA "=" RESET);
    else if (S_ISREG(m) && (m & (S_IXUSR|S_IXGRP|S_IXOTH)))
        printf(BOLD FG_BGREEN "*" RESET);
}

static void format_size(off_t size, char *buf, size_t bufsize) {
    if (!opt_human) {
        snprintf(buf, bufsize, "%10ld", (long)size);
        return;
    }
    const char *units = "BKMGTPE";
    double s = (double)size;
    int unit = 0;
    while (s >= 1024.0 && unit < 6) { s /= 1024.0; unit++; }
    if (unit == 0)
        snprintf(buf, bufsize, "%7ld B", (long)size);
    else
        snprintf(buf, bufsize, "%6.1f %c", s, units[unit]);
}

static void format_mode(mode_t m, char *buf) {
    buf[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISBLK(m) ? 'b' :
             S_ISCHR(m) ? 'c' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';
    buf[1] = (m & S_IRUSR) ? 'r' : '-';
    buf[2] = (m & S_IWUSR) ? 'w' : '-';
    buf[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S') : (m & S_IXUSR) ? 'x' : '-';
    buf[4] = (m & S_IRGRP) ? 'r' : '-';
    buf[5] = (m & S_IWGRP) ? 'w' : '-';
    buf[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S') : (m & S_IXGRP) ? 'x' : '-';
    buf[7] = (m & S_IROTH) ? 'r' : '-';
    buf[8] = (m & S_IWOTH) ? 'w' : '-';
    buf[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : (m & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

static void print_entry(const struct entry *e) {
    if (opt_long) {
        char mode_str[12], size_str[16], time_str[32];
        struct tm *tm;

        format_mode(e->st.st_mode, mode_str);
        format_size(e->st.st_size, size_str, sizeof(size_str));

        tm = localtime(&e->st.st_mtime);
        if (tm) {
            time_t now = time(NULL);
            if (now - e->st.st_mtime > 180 * 24 * 60 * 60)
                strftime(time_str, sizeof(time_str), "%b %d  %Y", tm);
            else
                strftime(time_str, sizeof(time_str), "%b %d %H:%M", tm);
        } else {
            snprintf(time_str, sizeof(time_str), "???");
        }

        if (opt_inode)
            printf("%8lu ", (unsigned long)e->st.st_ino);

        /* Color the mode string */
        if (!opt_nocolor) {
            char c = mode_str[0];
            if (c == 'd')
                printf(FG_BCYAN "%s" RESET, mode_str);
            else if (c == 'l')
                printf(FG_BMAGENTA "%s" RESET, mode_str);
            else if (c == 'b' || c == 'c')
                printf(FG_BYELLOW "%s" RESET, mode_str);
            else
                printf("%s", mode_str);
        } else {
            printf("%s", mode_str);
        }

        /* Look up user/group names */
        struct passwd *pw = getpwuid(e->st.st_uid);
        struct group *gr = getgrgid(e->st.st_gid);
        char uid_str[16], gid_str[16];
        if (pw) {
            snprintf(uid_str, sizeof(uid_str), "%-8s", pw->pw_name);
        } else {
            snprintf(uid_str, sizeof(uid_str), "%-8u", (unsigned)e->st.st_uid);
        }
        if (gr) {
            snprintf(gid_str, sizeof(gid_str), "%-8s", gr->gr_name);
        } else {
            snprintf(gid_str, sizeof(gid_str), "%-8u", (unsigned)e->st.st_gid);
        }
        printf(" %3lu %s %s",
               (unsigned long)e->st.st_nlink,
               uid_str, gid_str);

        /* Color the size */
        if (!opt_nocolor)
            printf(FG_BGREEN "%s" RESET, size_str);
        else
            printf("%s", size_str);

        printf(" %s ", time_str);

        /* Print colored name */
        print_colored_name(e);
        print_indicator(e->st.st_mode);

        /* Symlink target */
        if (S_ISLNK(e->st.st_mode) && e->link_target) {
            if (!opt_nocolor) {
                if (e->link_ok)
                    printf(" -> " FG_BMAGENTA "%s" RESET, e->link_target);
                else
                    printf(" -> " FG_BRED "%s" RESET, e->link_target);
            } else {
                printf(" -> %s", e->link_target);
            }
        }
        putchar('\n');
    } else {
        /* Short format */
        if (opt_inode)
            printf("%8lu ", (unsigned long)e->st.st_ino);
        print_colored_name(e);
        print_indicator(e->st.st_mode);
        putchar('\n');
    }
}

static int entry_cmp(const void *a, const void *b) {
    const struct entry *ea = a, *eb = b;
    int cmp = 0;

    if (opt_time) {
        if (ea->st.st_mtime < eb->st.st_mtime) cmp = 1;
        else if (ea->st.st_mtime > eb->st.st_mtime) cmp = -1;
    } else if (opt_size) {
        if (ea->st.st_size < eb->st.st_size) cmp = 1;
        else if (ea->st.st_size > eb->st.st_size) cmp = -1;
    }

    if (cmp == 0)
        cmp = strcasecmp(ea->name, eb->name);

    return opt_reverse ? -cmp : cmp;
}

static void free_entries(struct entry *entries, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(entries[i].name);
        free(entries[i].link_target);
    }
    free(entries);
}

static int list_directory(const char *path, int show_header);

static int process_entry(const char *dir, const char *name, struct entry *e) {
    char fullpath[PATH_MAX];

    e->name = strdup(name);
    if (!e->name) return -1;
    e->link_target = NULL;
    e->link_ok = 1;

    if (dir && dir[0])
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, name);
    else
        snprintf(fullpath, sizeof(fullpath), "%s", name);

    if (lstat(fullpath, &e->st) < 0) {
        memset(&e->st, 0, sizeof(e->st));
        return 0;
    }

    if (S_ISLNK(e->st.st_mode)) {
        char linkbuf[PATH_MAX];
        ssize_t len = readlink(fullpath, linkbuf, sizeof(linkbuf) - 1);
        if (len > 0) {
            linkbuf[len] = '\0';
            e->link_target = strdup(linkbuf);
        }
        struct stat target_st;
        e->link_ok = (stat(fullpath, &target_st) == 0);
    }

    return 0;
}

static int list_directory(const char *path, int show_header) {
    DIR *dir;
    struct dirent *de;
    struct entry *entries = NULL;
    size_t count = 0, capacity = 0;

    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        return 1;
    }

    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;

        if (name[0] == '.') {
            if (!opt_all && !opt_almost) continue;
            if (opt_almost && (!strcmp(name, ".") || !strcmp(name, ".."))) continue;
        }

        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 32;
            struct entry *tmp = realloc(entries, capacity * sizeof(*entries));
            if (!tmp) {
                fprintf(stderr, "ls: out of memory\n");
                free_entries(entries, count);
                closedir(dir);
                return 1;
            }
            entries = tmp;
        }

        if (process_entry(path, name, &entries[count]) == 0)
            count++;
    }
    closedir(dir);

    if (count > 0)
        qsort(entries, count, sizeof(*entries), entry_cmp);

    if (show_header) {
        if (!first_output) putchar('\n');
        first_output = 0;
        if (!opt_nocolor)
            printf(BOLD FG_BWHITE "%s:" RESET "\n", path);
        else
            printf("%s:\n", path);
    }

    for (size_t i = 0; i < count; i++)
        print_entry(&entries[i]);

    if (opt_recursive) {
        for (size_t i = 0; i < count; i++) {
            if (S_ISDIR(entries[i].st.st_mode)) {
                const char *name = entries[i].name;
                if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
                char subpath[PATH_MAX];
                snprintf(subpath, sizeof(subpath), "%s/%s", path, name);
                list_directory(subpath, 1);
            }
        }
    }

    free_entries(entries, count);
    return 0;
}

static int list_single(const char *path) {
    struct entry e;
    memset(&e, 0, sizeof(e));

    if (process_entry(NULL, path, &e) < 0)
        return 1;

    if (S_ISDIR(e.st.st_mode) && !opt_dir) {
        free(e.name);
        free(e.link_target);
        return list_directory(path, 0);
    }

    print_entry(&e);
    free(e.name);
    free(e.link_target);
    return 0;
}

static void usage(void) {
    fprintf(stderr, "Usage: ls [-1AaFdhilRrSt] [file...]\n");
}

int main(int argc, char *argv[]) {
    int ret = 0, i;

    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "--no-color")) { opt_nocolor = 1; continue; }
        for (const char *p = argv[i] + 1; *p; p++) {
            switch (*p) {
                case '1': opt_oneline = 1; break;
                case 'A': opt_almost = 1; break;
                case 'a': opt_all = 1; break;
                case 'd': opt_dir = 1; break;
                case 'F': opt_classify = 1; break;
                case 'h': opt_human = 1; break;
                case 'i': opt_inode = 1; break;
                case 'l': opt_long = 1; break;
                case 'R': opt_recursive = 1; break;
                case 'r': opt_reverse = 1; break;
                case 'S': opt_size = 1; break;
                case 't': opt_time = 1; break;
                default:
                    fprintf(stderr, "ls: unknown option -%c\n", *p);
                    usage();
                    return 1;
            }
        }
    }

    if (!isatty(STDOUT_FILENO))
        opt_oneline = 1;

    if (i >= argc) {
        ret = list_directory(".", 0);
    } else if (i == argc - 1) {
        ret = list_single(argv[i]);
    } else {
        struct entry *files = NULL, *dirs = NULL;
        size_t nfiles = 0, ndirs = 0;

        for (; i < argc; i++) {
            struct entry e;
            memset(&e, 0, sizeof(e));
            if (process_entry(NULL, argv[i], &e) < 0) { ret = 1; continue; }

            if (S_ISDIR(e.st.st_mode) && !opt_dir) {
                dirs = realloc(dirs, (ndirs + 1) * sizeof(*dirs));
                if (dirs) dirs[ndirs++] = e;
            } else {
                files = realloc(files, (nfiles + 1) * sizeof(*files));
                if (files) files[nfiles++] = e;
            }
        }

        if (nfiles > 0) {
            qsort(files, nfiles, sizeof(*files), entry_cmp);
            for (size_t j = 0; j < nfiles; j++)
                print_entry(&files[j]);
            first_output = 0;
        }

        if (ndirs > 0) {
            qsort(dirs, ndirs, sizeof(*dirs), entry_cmp);
            for (size_t j = 0; j < ndirs; j++) {
                if (nfiles > 0 || j > 0) putchar('\n');
                if (!opt_nocolor)
                    printf(BOLD FG_BWHITE "%s:" RESET "\n", dirs[j].name);
                else
                    printf("%s:\n", dirs[j].name);
                if (list_directory(dirs[j].name, 0) != 0) ret = 1;
            }
        }

        free_entries(files, nfiles);
        free_entries(dirs, ndirs);
    }

    return ret;
}
