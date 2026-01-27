// groupadd - create a new group
// Usage: groupadd groupname

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int disk_available(void) {
    struct stat st;
    return (stat("/disk/etc", &st) == 0);
}

// Find the next available GID (scan /etc/group for max GID >= 1000)
static gid_t next_gid(void) {
    gid_t max_gid = 999;

    FILE* f = fopen("/etc/group", "r");
    if (!f) return 1000;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        // Parse gid field (3rd field)
        char* p = line;
        for (int i = 0; i < 2 && p; i++) {
            p = strchr(p, ':');
            if (p) p++;
        }
        if (p) {
            gid_t gid = (gid_t)strtoul(p, NULL, 10);
            if (gid >= 1000 && gid > max_gid) {
                max_gid = gid;
            }
        }
    }
    fclose(f);
    return max_gid + 1;
}

static int append_line(const char* path, const char* line) {
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;
    (void)write(fd, line, strlen(line));
    close(fd);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: groupadd groupname\n");
        return 1;
    }

    const char* groupname = argv[1];

    // Check if group already exists
    if (getgrnam(groupname)) {
        fprintf(stderr, "groupadd: group '%s' already exists\n", groupname);
        return 1;
    }

    // Only root can add groups
    if (getuid() != 0) {
        fprintf(stderr, "groupadd: permission denied (must be root)\n");
        return 1;
    }

    gid_t gid = next_gid();

    char group_line[128];
    snprintf(group_line, sizeof(group_line), "%s::%u:\n", groupname, (unsigned)gid);

    if (append_line("/etc/group", group_line) < 0) {
        fprintf(stderr, "groupadd: failed to update /etc/group: %s\n", strerror(errno));
        return 1;
    }

    if (disk_available()) {
        append_line("/disk/etc/group", group_line);
    } else {
        fprintf(stderr, "groupadd: warning: no persistent storage, group will not survive reboot\n");
    }

    printf("Group '%s' created (gid=%u)\n", groupname, (unsigned)gid);
    return 0;
}
