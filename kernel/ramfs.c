#include "ramfs.h"
#include "kheap.h"
#include "rtc.h"
#include "string.h"
#include "ctype.h"

#define RAMFS_MAX_FILES 128
#define RAMFS_MAX_DIRS  128

typedef struct ramfs_file {
    char* path;       // canonical, no leading '/'
    uint8_t* data;    // owned
    uint32_t size;
    bool is_symlink;
    uint16_t mode; // POSIX permission bits (07777)
    uint16_t wtime;
    uint16_t wdate;
    uint32_t uid;
    uint32_t gid;
} ramfs_file_t;

typedef struct ramfs_dir {
    char* path;       // canonical, no leading '/'
    uint16_t mode; // POSIX permission bits (07777)
    uint16_t wtime;
    uint16_t wdate;
    uint32_t uid;
    uint32_t gid;
} ramfs_dir_t;

static ramfs_file_t files[RAMFS_MAX_FILES];
static ramfs_dir_t dirs[RAMFS_MAX_DIRS];
static bool ready = false;

static bool ci_eq(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
    }
}

static bool ci_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }
    while (*prefix) {
        char cs = *s++;
        char cp = *prefix++;
        if (tolower((unsigned char)cs) != tolower((unsigned char)cp)) {
            return false;
        }
    }
    return true;
}

static void ramfs_timestamp_now(uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;

    rtc_datetime_t dt;
    if (!rtc_read_datetime(&dt)) {
        return;
    }
    if (dt.year < 1980 || dt.year > 2107 ||
        dt.month < 1 || dt.month > 12 ||
        dt.day < 1 || dt.day > 31 ||
        dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
        return;
    }

    uint16_t wdate = (uint16_t)(((uint16_t)(dt.year - 1980) << 9) |
                               ((uint16_t)dt.month << 5) |
                               (uint16_t)dt.day);
    uint16_t wtime = (uint16_t)(((uint16_t)dt.hour << 11) |
                               ((uint16_t)dt.minute << 5) |
                               (uint16_t)(dt.second / 2u));

    if (out_wtime) *out_wtime = wtime;
    if (out_wdate) *out_wdate = wdate;
}

static char* dup_str(const char* s) {
    if (!s) {
        s = "";
    }
    uint32_t len = (uint32_t)strlen(s);
    char* out = (char*)kmalloc(len + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static bool dir_time_rel(const char* rel, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (!rel || rel[0] == '\0') {
        return false;
    }

    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
            if (out_wtime) *out_wtime = dirs[i].wtime;
            if (out_wdate) *out_wdate = dirs[i].wdate;
            return true;
        }
    }
    return false;
}

static bool build_child_rel(const char* parent_rel, const char* name, char* out, uint32_t out_cap) {
    if (!parent_rel || !name || !out || out_cap == 0) {
        return false;
    }
    uint32_t parent_len = (uint32_t)strlen(parent_rel);
    uint32_t name_len = (uint32_t)strlen(name);
    if (parent_len == 0 || name_len == 0) {
        return false;
    }
    if (parent_len + 1u + name_len + 1u > out_cap) {
        return false;
    }
    memcpy(out, parent_rel, parent_len);
    out[parent_len] = '/';
    memcpy(out + parent_len + 1u, name, name_len);
    out[parent_len + 1u + name_len] = '\0';
    return true;
}

static bool normalize_path(const char* in, char* out, uint32_t out_cap) {
    if (!out || out_cap == 0) {
        return false;
    }
    if (!in) {
        in = "";
    }

    // Build a canonical path without leading '/'.
    uint32_t out_len = 0;
    uint32_t saved[32];
    uint32_t depth = 0;

    const char* p = in;
    while (*p == '/') p++;

    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;

        const char* seg = p;
        uint32_t seg_len = 0;
        while (p[seg_len] && p[seg_len] != '/') seg_len++;
        p += seg_len;

        if (seg_len == 1 && seg[0] == '.') {
            continue;
        }
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (depth) {
                out_len = saved[--depth];
            }
            continue;
        }

        if (depth >= (uint32_t)(sizeof(saved) / sizeof(saved[0]))) {
            return false;
        }
        saved[depth++] = out_len;

        uint32_t need = seg_len + ((out_len > 0u) ? 1u : 0u) + 1u;
        if (out_len + need > out_cap) {
            return false;
        }

        if (out_len > 0u) {
            out[out_len++] = '/';
        }
        for (uint32_t i = 0; i < seg_len; i++) {
            out[out_len++] = seg[i];
        }
    }

    if (out_len >= out_cap) {
        return false;
    }
    out[out_len] = '\0';
    return true;
}

static bool is_ram_path(const char* rel) {
    if (!rel || rel[0] == '\0') {
        return false;
    }
    if (ci_eq(rel, "ram")) {
        return true;
    }
    return ci_starts_with(rel, "ram/");
}

static bool dir_exists_rel(const char* rel) {
    if (!is_ram_path(rel)) {
        return false;
    }
    if (ci_eq(rel, "ram")) {
        return true;
    }

    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
            return true;
        }
    }
    // Implicit directory: any file/dir under it.
    uint32_t rel_len = (uint32_t)strlen(rel);
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_starts_with(dirs[i].path, rel) && dirs[i].path[rel_len] == '/') {
            return true;
        }
    }
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].path && ci_starts_with(files[i].path, rel) && files[i].path[rel_len] == '/') {
            return true;
        }
    }
    return false;
}

static int find_file_rel(const char* rel) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].path && ci_eq(files[i].path, rel)) {
            return i;
        }
    }
    return -1;
}

static int alloc_dir_slot(void) {
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (!dirs[i].path) {
            return i;
        }
    }
    return -1;
}

static int alloc_file_slot(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].path) {
            return i;
        }
    }
    return -1;
}

void ramfs_init(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].path) {
            kfree(files[i].path);
            files[i].path = NULL;
        }
        if (files[i].data) {
            kfree(files[i].data);
            files[i].data = NULL;
        }
        files[i].size = 0;
        files[i].is_symlink = false;
        files[i].mode = 0;
        files[i].wtime = 0;
        files[i].wdate = 0;
    }
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path) {
            kfree(dirs[i].path);
            dirs[i].path = NULL;
        }
        dirs[i].mode = 0;
        dirs[i].wtime = 0;
        dirs[i].wdate = 0;
    }

    // Create a timestamped root directory entry for /ram so it can appear in listings/stat.
    uint16_t wtime = 0;
    uint16_t wdate = 0;
    ramfs_timestamp_now(&wtime, &wdate);
    dirs[0].path = dup_str("ram");
    if (dirs[0].path) {
        dirs[0].mode = 0755u;
        dirs[0].wtime = wtime;
        dirs[0].wdate = wdate;
    }
    ready = true;
}

bool ramfs_is_dir(const char* path) {
    if (!ready) {
        return false;
    }
    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (rel[0] == '\0') {
        return false;
    }
    return dir_exists_rel(rel);
}

bool ramfs_is_file(const char* path) {
    if (!ready) {
        return false;
    }
    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || ci_eq(rel, "ram")) {
        return false;
    }
    return find_file_rel(rel) >= 0;
}

bool ramfs_get_meta(const char* path, bool* out_is_symlink, uint16_t* out_mode) {
    if (out_is_symlink) *out_is_symlink = false;
    if (out_mode) *out_mode = 0;
    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || rel[0] == '\0') {
        return false;
    }

    if (ci_eq(rel, "ram")) {
        if (out_is_symlink) *out_is_symlink = false;
        if (out_mode) *out_mode = 0755u;
        return true;
    }

    // Prefer explicit directory metadata if present.
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
            if (out_is_symlink) *out_is_symlink = false;
            if (out_mode) *out_mode = (uint16_t)(dirs[i].mode & 07777u);
            return true;
        }
    }

    if (dir_exists_rel(rel)) {
        if (out_is_symlink) *out_is_symlink = false;
        if (out_mode) *out_mode = 0755u;
        return true;
    }

    int idx = find_file_rel(rel);
    if (idx < 0) {
        return false;
    }

    if (out_is_symlink) *out_is_symlink = files[idx].is_symlink;
    if (out_mode) *out_mode = (uint16_t)(files[idx].mode & 07777u);
    return true;
}

bool ramfs_set_meta(const char* path, bool is_symlink, uint16_t mode) {
    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || rel[0] == '\0') {
        return false;
    }

    mode &= 07777u;

    if (ci_eq(rel, "ram")) {
        // /ram is always a directory.
        for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
            if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
                dirs[i].mode = mode;
                return true;
            }
        }
        return false;
    }

    int fidx = find_file_rel(rel);
    if (fidx >= 0) {
        files[fidx].is_symlink = is_symlink;
        files[fidx].mode = mode;
        return true;
    }

    if (dir_exists_rel(rel)) {
        if (is_symlink) {
            return false;
        }
        // Ensure an explicit directory entry exists so we can persist mode.
        for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
            if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
                dirs[i].mode = mode;
                return true;
            }
        }

        int slot = alloc_dir_slot();
        if (slot < 0) {
            return false;
        }
        dirs[slot].path = dup_str(rel);
        if (!dirs[slot].path) {
            return false;
        }
        dirs[slot].mode = mode;
        ramfs_timestamp_now(&dirs[slot].wtime, &dirs[slot].wdate);
        return true;
    }

    return false;
}

bool ramfs_get_owner(const char* path, uint32_t* out_uid, uint32_t* out_gid) {
    if (out_uid) *out_uid = 0;
    if (out_gid) *out_gid = 0;

    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || rel[0] == '\0') {
        return false;
    }

    int fidx = find_file_rel(rel);
    if (fidx >= 0) {
        if (out_uid) *out_uid = files[fidx].uid;
        if (out_gid) *out_gid = files[fidx].gid;
        return true;
    }

    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
            if (out_uid) *out_uid = dirs[i].uid;
            if (out_gid) *out_gid = dirs[i].gid;
            return true;
        }
    }

    // Implicit directory (parent of files) - owned by root.
    if (dir_exists_rel(rel)) {
        return true;
    }

    return false;
}

bool ramfs_set_owner(const char* path, uint32_t uid, uint32_t gid) {
    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || rel[0] == '\0') {
        return false;
    }

    int fidx = find_file_rel(rel);
    if (fidx >= 0) {
        files[fidx].uid = uid;
        files[fidx].gid = gid;
        return true;
    }

    // For directories, find or create explicit entry.
    if (dir_exists_rel(rel)) {
        for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
            if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
                dirs[i].uid = uid;
                dirs[i].gid = gid;
                return true;
            }
        }

        // Create explicit entry.
        int slot = alloc_dir_slot();
        if (slot < 0) {
            return false;
        }
        dirs[slot].path = dup_str(rel);
        if (!dirs[slot].path) {
            return false;
        }
        dirs[slot].mode = 0755;
        dirs[slot].uid = uid;
        dirs[slot].gid = gid;
        ramfs_timestamp_now(&dirs[slot].wtime, &dirs[slot].wdate);
        return true;
    }

    return false;
}

bool ramfs_stat_ex(const char* path, bool* out_is_dir, uint32_t* out_size, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_is_dir) *out_is_dir = false;
    if (out_size) *out_size = 0;
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;

    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || rel[0] == '\0') {
        return false;
    }

    if (ci_eq(rel, "ram")) {
        if (out_is_dir) *out_is_dir = true;
        (void)dir_time_rel(rel, out_wtime, out_wdate);
        return true;
    }

    if (dir_exists_rel(rel)) {
        if (out_is_dir) *out_is_dir = true;
        (void)dir_time_rel(rel, out_wtime, out_wdate);
        return true;
    }

    int idx = find_file_rel(rel);
    if (idx < 0) {
        return false;
    }

    if (out_is_dir) *out_is_dir = false;
    if (out_size) *out_size = files[idx].size;
    if (out_wtime) *out_wtime = files[idx].wtime;
    if (out_wdate) *out_wdate = files[idx].wdate;
    return true;
}

bool ramfs_mkdir(const char* path) {
    if (!ready) {
        return false;
    }
    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel)) {
        return false;
    }
    if (ci_eq(rel, "ram")) {
        return true;
    }

    // Create each intermediate directory: ram/<a>/<b>/...
    // Split by '/', building up prefixes.
    char cur[128];
    cur[0] = '\0';

    uint16_t wtime = 0;
    uint16_t wdate = 0;
    ramfs_timestamp_now(&wtime, &wdate);

    const char* p = rel;
    uint32_t cur_len = 0;
    while (*p) {
        const char* seg = p;
        uint32_t seg_len = 0;
        while (p[seg_len] && p[seg_len] != '/') seg_len++;
        p += seg_len;
        if (*p == '/') p++;

        if (seg_len == 0) {
            continue;
        }

        uint32_t need = seg_len + ((cur_len > 0u) ? 1u : 0u) + 1u;
        if (cur_len + need > sizeof(cur)) {
            return false;
        }
        if (cur_len > 0u) {
            cur[cur_len++] = '/';
        }
        memcpy(cur + cur_len, seg, seg_len);
        cur_len += seg_len;
        cur[cur_len] = '\0';

        if (!is_ram_path(cur)) {
            return false;
        }
        if (ci_eq(cur, "ram")) {
            continue;
        }
        if (dir_exists_rel(cur)) {
            continue;
        }
        if (find_file_rel(cur) >= 0) {
            return false;
        }

        int slot = alloc_dir_slot();
        if (slot < 0) {
            return false;
        }
        dirs[slot].path = dup_str(cur);
        if (!dirs[slot].path) {
            return false;
        }
        dirs[slot].mode = 0755u;
        dirs[slot].wtime = wtime;
        dirs[slot].wdate = wdate;
    }

    return true;
}

bool ramfs_write_file(const char* path, const uint8_t* data, uint32_t size, bool overwrite) {
    if (!ready) {
        return false;
    }
    if (!data && size != 0) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || ci_eq(rel, "ram")) {
        return false;
    }

    // Ensure parent directory exists.
    char parent[128];
    strncpy(parent, rel, sizeof(parent) - 1u);
    parent[sizeof(parent) - 1u] = '\0';
    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!ramfs_mkdir(parent)) {
            return false;
        }
    } else {
        // parent is "ram"
    }

    int idx = find_file_rel(rel);
    if (idx >= 0 && !overwrite) {
        return false;
    }
    if (idx < 0) {
        idx = alloc_file_slot();
        if (idx < 0) {
            return false;
        }
        files[idx].path = dup_str(rel);
        if (!files[idx].path) {
            return false;
        }
        files[idx].is_symlink = false;
        files[idx].mode = 0644u;
        files[idx].wtime = 0;
        files[idx].wdate = 0;
    } else {
        if (files[idx].data) {
            kfree(files[idx].data);
            files[idx].data = NULL;
        }
        files[idx].size = 0;
        // Preserve metadata across overwrites.
    }

    uint32_t alloc_size = size ? size : 1u;
    uint8_t* buf = (uint8_t*)kmalloc(alloc_size);
    if (!buf) {
        if (idx >= 0 && files[idx].data == NULL && files[idx].size == 0) {
            // keep path allocated
        }
        return false;
    }
    if (size) {
        memcpy(buf, data, size);
    } else {
        buf[0] = 0;
    }

    files[idx].data = buf;
    files[idx].size = size;
    ramfs_timestamp_now(&files[idx].wtime, &files[idx].wdate);
    return true;
}

bool ramfs_rename(const char* old_path, const char* new_path) {
    if (!ready) {
        return false;
    }

    char old_rel[128];
    char new_rel[128];
    if (!normalize_path(old_path, old_rel, sizeof(old_rel)) || !normalize_path(new_path, new_rel, sizeof(new_rel))) {
        return false;
    }
    if (!is_ram_path(old_rel) || !is_ram_path(new_rel)) {
        return false;
    }
    if (ci_eq(old_rel, "ram") || ci_eq(new_rel, "ram")) {
        return false;
    }

    int idx = find_file_rel(old_rel);
    if (idx < 0) {
        return false;
    }
    if (find_file_rel(new_rel) >= 0) {
        return false;
    }

    // Ensure parent directory exists.
    char parent[128];
    strncpy(parent, new_rel, sizeof(parent) - 1u);
    parent[sizeof(parent) - 1u] = '\0';
    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!ramfs_mkdir(parent)) {
            return false;
        }
    }

    char* dup = dup_str(new_rel);
    if (!dup) {
        return false;
    }
    kfree(files[idx].path);
    files[idx].path = dup;
    return true;
}

bool ramfs_unlink(const char* path) {
    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || ci_eq(rel, "ram")) {
        return false;
    }

    int idx = find_file_rel(rel);
    if (idx < 0) {
        return false;
    }

    if (files[idx].path) {
        kfree(files[idx].path);
        files[idx].path = NULL;
    }
    if (files[idx].data) {
        kfree(files[idx].data);
        files[idx].data = NULL;
    }
    files[idx].size = 0;
    files[idx].is_symlink = false;
    files[idx].mode = 0;
    files[idx].wtime = 0;
    files[idx].wdate = 0;
    return true;
}

bool ramfs_rmdir(const char* path) {
    if (!ready) {
        return false;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || ci_eq(rel, "ram")) {
        return false;
    }
    if (!dir_exists_rel(rel)) {
        return false;
    }

    uint32_t rel_len = (uint32_t)strlen(rel);
    if (rel_len + 2u > sizeof(rel)) {
        return false;
    }

    char prefix[128];
    memcpy(prefix, rel, rel_len);
    prefix[rel_len] = '/';
    prefix[rel_len + 1u] = '\0';

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].path && ci_starts_with(files[i].path, prefix)) {
            return false;
        }
    }
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_starts_with(dirs[i].path, prefix)) {
            return false;
        }
    }

    // Remove explicit directory entry if present.
    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (dirs[i].path && ci_eq(dirs[i].path, rel)) {
            kfree(dirs[i].path);
            dirs[i].path = NULL;
            dirs[i].mode = 0;
            dirs[i].wtime = 0;
            dirs[i].wdate = 0;
            break;
        }
    }

    return true;
}

bool ramfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size) {
    if (!ready) {
        return false;
    }
    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return false;
    }
    if (!is_ram_path(rel) || ci_eq(rel, "ram")) {
        return false;
    }

    int idx = find_file_rel(rel);
    if (idx < 0) {
        return false;
    }
    if (out_data) *out_data = files[idx].data;
    if (out_size) *out_size = files[idx].size;
    return files[idx].data != NULL;
}

static bool add_unique(ramfs_dirent_t* out,
                       uint32_t* count,
                       uint32_t max,
                       const char* name,
                       bool is_dir,
                       bool is_symlink,
                       uint16_t mode,
                       uint32_t size,
                       uint16_t wtime,
                       uint16_t wdate,
                       uint32_t uid,
                       uint32_t gid) {
    if (!out || !count || !name || name[0] == '\0') {
        return false;
    }
    for (uint32_t i = 0; i < *count; i++) {
        if (ci_eq(out[i].name, name)) {
            out[i].is_dir = out[i].is_dir || is_dir;
            if (out[i].is_dir) {
                out[i].is_symlink = false;
            } else {
                out[i].is_symlink = out[i].is_symlink || is_symlink;
            }
            uint32_t old_key = ((uint32_t)out[i].wdate << 16) | (uint32_t)out[i].wtime;
            uint32_t new_key = ((uint32_t)wdate << 16) | (uint32_t)wtime;
            if (new_key > old_key) {
                out[i].wtime = wtime;
                out[i].wdate = wdate;
            }
            return true;
        }
    }
    if (*count >= max) {
        return false;
    }
    strncpy(out[*count].name, name, sizeof(out[*count].name) - 1u);
    out[*count].name[sizeof(out[*count].name) - 1u] = '\0';
    out[*count].is_dir = is_dir;
    out[*count].is_symlink = (!is_dir && is_symlink);
    out[*count].mode = (uint16_t)(mode & 07777u);
    out[*count].size = size;
    out[*count].wtime = wtime;
    out[*count].wdate = wdate;
    out[*count].uid = uid;
    out[*count].gid = gid;
    (*count)++;
    return true;
}

uint32_t ramfs_list_dir(const char* path, ramfs_dirent_t* out, uint32_t max) {
    if (!ready || !out || max == 0) {
        return 0;
    }

    char rel[128];
    if (!normalize_path(path, rel, sizeof(rel))) {
        return 0;
    }
    if (!is_ram_path(rel)) {
        return 0;
    }
    if (!dir_exists_rel(rel)) {
        return 0;
    }

    uint32_t prefix_len = (uint32_t)strlen(rel);
    uint32_t count = 0;

    for (int i = 0; i < RAMFS_MAX_DIRS; i++) {
        if (!dirs[i].path) {
            continue;
        }
        const char* d = dirs[i].path;
        if (!ci_starts_with(d, rel) || d[prefix_len] != '/') {
            continue;
        }
        const char* rem = d + prefix_len + 1u;
        if (rem[0] == '\0') {
            continue;
        }
        char seg[64];
        uint32_t seg_len = 0;
        while (rem[seg_len] && rem[seg_len] != '/' && seg_len + 1u < sizeof(seg)) {
            seg[seg_len] = rem[seg_len];
            seg_len++;
        }
        seg[seg_len] = '\0';
        char child[128];
        child[0] = '\0';
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        if (build_child_rel(rel, seg, child, sizeof(child))) {
            (void)dir_time_rel(child, &wtime, &wdate);
        }
        uint16_t mode = 0755u;
        uint32_t uid = 0, gid = 0;
        // Prefer explicit directory metadata if present.
        for (int di = 0; di < RAMFS_MAX_DIRS; di++) {
            if (dirs[di].path && ci_eq(dirs[di].path, child)) {
                if (dirs[di].mode) {
                    mode = (uint16_t)(dirs[di].mode & 07777u);
                }
                uid = dirs[di].uid;
                gid = dirs[di].gid;
                break;
            }
        }
        add_unique(out, &count, max, seg, true, false, mode, 0, wtime, wdate, uid, gid);
    }

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].path) {
            continue;
        }
        const char* f = files[i].path;
        if (!ci_starts_with(f, rel) || f[prefix_len] != '/') {
            continue;
        }
        const char* rem = f + prefix_len + 1u;
        if (rem[0] == '\0') {
            continue;
        }
        char seg[64];
        uint32_t seg_len = 0;
        while (rem[seg_len] && rem[seg_len] != '/' && seg_len + 1u < sizeof(seg)) {
            seg[seg_len] = rem[seg_len];
            seg_len++;
        }
        seg[seg_len] = '\0';
        bool is_dir = rem[seg_len] == '/';
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        if (is_dir) {
            char child[128];
            child[0] = '\0';
            if (build_child_rel(rel, seg, child, sizeof(child))) {
                (void)dir_time_rel(child, &wtime, &wdate);
            }
            uint16_t mode = 0755u;
            for (int di = 0; di < RAMFS_MAX_DIRS; di++) {
                if (dirs[di].path && ci_eq(dirs[di].path, child)) {
                    if (dirs[di].mode) {
                        mode = (uint16_t)(dirs[di].mode & 07777u);
                    }
                    break;
                }
            }
            add_unique(out, &count, max, seg, true, false, mode, 0u, wtime, wdate, 0u, 0u);
        } else {
            uint16_t mode = files[i].mode ? files[i].mode : 0644u;
            add_unique(out,
                       &count,
                       max,
                       seg,
                       false,
                       files[i].is_symlink,
                       mode,
                       files[i].size,
                       files[i].wtime,
                       files[i].wdate,
                       files[i].uid,
                       files[i].gid);
        }
    }

    return count;
}
