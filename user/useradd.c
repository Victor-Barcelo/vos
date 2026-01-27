// useradd - create a new user
// Usage: useradd [-m] username

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Check if /disk is available for persistent storage
static int disk_available(void) {
    struct stat st;
    return (stat("/disk/etc", &st) == 0);
}

// Find the next available UID (scan /etc/passwd for max UID >= 1000)
static uid_t next_uid(void) {
    uid_t max_uid = 999;  // Start from 1000 for regular users

    FILE* f = fopen("/etc/passwd", "r");
    if (!f) return 1000;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        // Parse uid field (3rd field)
        char* p = line;
        for (int i = 0; i < 2 && p; i++) {
            p = strchr(p, ':');
            if (p) p++;
        }
        if (p) {
            uid_t uid = (uid_t)strtoul(p, NULL, 10);
            if (uid >= 1000 && uid > max_uid) {
                max_uid = uid;
            }
        }
    }
    fclose(f);
    return max_uid + 1;
}

// Append a line to a file
static int append_line(const char* path, const char* line) {
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;
    (void)write(fd, line, strlen(line));
    close(fd);
    return 0;
}

// Copy a file
static int copy_file(const char* src, const char* dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -1;

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0) {
        close(sfd);
        return -1;
    }

    char buf[512];
    int n;
    while ((n = (int)read(sfd, buf, sizeof(buf))) > 0) {
        (void)write(dfd, buf, (unsigned int)n);
    }

    close(dfd);
    close(sfd);
    return 0;
}

// Copy skeleton files to home directory
static void copy_skel(const char* homedir, uid_t uid, gid_t gid) {
    DIR* d = opendir("/etc/skel");
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
            continue;  // skip . and ..
        }

        char src[256], dst[256];
        snprintf(src, sizeof(src), "/etc/skel/%s", ent->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", homedir, ent->d_name);

        struct stat st;
        if (stat(src, &st) == 0 && S_ISREG(st.st_mode)) {
            copy_file(src, dst);
            chown(dst, uid, gid);
        }
    }
    closedir(d);
}

static void usage(void) {
    fprintf(stderr, "Usage: useradd [-m] username\n");
    fprintf(stderr, "  -m  Create home directory\n");
    exit(1);
}

int main(int argc, char* argv[]) {
    int create_home = 0;
    char* username = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0) {
            create_home = 1;
        } else if (argv[i][0] != '-') {
            username = argv[i];
        } else {
            usage();
        }
    }

    if (!username) {
        usage();
    }

    // Check if user already exists
    if (getpwnam(username)) {
        fprintf(stderr, "useradd: user '%s' already exists\n", username);
        return 1;
    }

    // Only root can add users
    if (getuid() != 0) {
        fprintf(stderr, "useradd: permission denied (must be root)\n");
        return 1;
    }

    // Allocate UID/GID
    uid_t uid = next_uid();
    gid_t gid = uid;  // Primary group same as UID

    // Build home directory path
    char homedir[128];
    snprintf(homedir, sizeof(homedir), "/home/%s", username);

    // Build passwd entry
    char passwd_line[256];
    snprintf(passwd_line, sizeof(passwd_line),
             "%s::%u:%u:%s:/bin/dash\n",
             username, (unsigned)uid, (unsigned)gid, homedir);

    // Build group entry
    char group_line[128];
    snprintf(group_line, sizeof(group_line),
             "%s::%u:%s\n",
             username, (unsigned)gid, username);

    // Append to /etc/passwd (which is /ram/etc/passwd)
    if (append_line("/etc/passwd", passwd_line) < 0) {
        fprintf(stderr, "useradd: failed to update /etc/passwd: %s\n", strerror(errno));
        return 1;
    }

    // Append to /etc/group
    if (append_line("/etc/group", group_line) < 0) {
        fprintf(stderr, "useradd: failed to update /etc/group: %s\n", strerror(errno));
        return 1;
    }

    // Persist to /disk if available
    if (disk_available()) {
        append_line("/disk/etc/passwd", passwd_line);
        append_line("/disk/etc/group", group_line);
    } else {
        fprintf(stderr, "useradd: warning: no persistent storage, user will not survive reboot\n");
    }

    // Create home directory if requested
    if (create_home) {
        // Create /home if it doesn't exist
        (void)mkdir("/home", 0755);

        // Create user's home directory
        if (mkdir(homedir, 0755) < 0 && errno != EEXIST) {
            fprintf(stderr, "useradd: failed to create %s: %s\n", homedir, strerror(errno));
            return 1;
        }

        // Set ownership
        if (chown(homedir, uid, gid) < 0) {
            fprintf(stderr, "useradd: warning: failed to chown %s\n", homedir);
        }

        // Copy skeleton files
        copy_skel(homedir, uid, gid);

        // Also create on /disk for persistence
        if (disk_available()) {
            char disk_home[128];
            snprintf(disk_home, sizeof(disk_home), "/disk/home/%s", username);
            (void)mkdir(disk_home, 0755);
            // Note: chown on /disk won't work (FAT16), but that's okay
        }
    }

    printf("User '%s' created (uid=%u)\n", username, (unsigned)uid);
    return 0;
}
