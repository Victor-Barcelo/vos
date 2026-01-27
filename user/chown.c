// chown - change file owner and group
// Usage: chown [owner][:group] file...

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s owner[:group] file...\n", prog);
    fprintf(stderr, "       %s :group file...\n", prog);
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        usage(argv[0]);
    }

    char* spec = argv[1];
    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;

    // Parse owner:group or :group
    char* colon = strchr(spec, ':');
    char* owner = spec;
    char* group = NULL;

    if (colon) {
        *colon = '\0';
        group = colon + 1;
    }

    // Parse owner (if present)
    if (owner && *owner) {
        struct passwd* pw = getpwnam(owner);
        if (pw) {
            uid = pw->pw_uid;
        } else {
            // Try numeric
            char* end;
            long n = strtol(owner, &end, 10);
            if (*end != '\0' || n < 0) {
                fprintf(stderr, "chown: invalid user '%s'\n", owner);
                return 1;
            }
            uid = (uid_t)n;
        }
    }

    // Parse group (if present)
    if (group && *group) {
        struct group* gr = getgrnam(group);
        if (gr) {
            gid = gr->gr_gid;
        } else {
            // Try numeric
            char* end;
            long n = strtol(group, &end, 10);
            if (*end != '\0' || n < 0) {
                fprintf(stderr, "chown: invalid group '%s'\n", group);
                return 1;
            }
            gid = (gid_t)n;
        }
    }

    if (uid == (uid_t)-1 && gid == (gid_t)-1) {
        usage(argv[0]);
    }

    int ret = 0;
    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], uid, gid) < 0) {
            fprintf(stderr, "chown: %s: %s\n", argv[i], strerror(errno));
            ret = 1;
        }
    }

    return ret;
}
