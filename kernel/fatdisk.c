#include "fatdisk.h"
#include "ata.h"
#include "ctype.h"
#include "io.h"
#include "kheap.h"
#include "rtc.h"
#include "serial.h"
#include "string.h"

#define FATDISK_MOUNT "/disk"
#define SECTOR_SIZE 512u

enum {
    FAT_ATTR_READONLY = 0x01,
    FAT_ATTR_HIDDEN   = 0x02,
    FAT_ATTR_SYSTEM   = 0x04,
    FAT_ATTR_VOLUME   = 0x08,
    FAT_ATTR_DIR      = 0x10,
    FAT_ATTR_ARCHIVE  = 0x20,
    FAT_ATTR_LFN      = 0x0F,
};

// Store minimal POSIX-ish metadata in otherwise-unused SFN bytes on FAT16.
// - NT reserved byte (offset 12): use high bits as a meta marker.
// - FAT32 hi-cluster (offset 20..21): unused on FAT16, store mode bits.
enum {
    FAT_META_PRESENT = 0x80,
    FAT_META_SYMLINK = 0x40,
};

typedef struct fatdisk_fs {
    bool ready;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint32_t total_sectors;
    uint16_t fat_sectors;
    uint32_t root_dir_sectors;
    uint32_t fat_start_lba;
    uint32_t first_root_lba;
    uint32_t first_data_lba;
    uint32_t cluster_count;
    uint32_t cluster_size_bytes;
    uint16_t alloc_cursor;
    char label[12];
} fatdisk_fs_t;

static fatdisk_fs_t g_fs;

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void fat_dirent_best_ts(const uint8_t e[32], uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (!e) {
        return;
    }

    uint16_t wtime = read_le16(e + 22);
    uint16_t wdate = read_le16(e + 24);

    if (wdate == 0) {
        // Try create time/date.
        wtime = read_le16(e + 14);
        wdate = read_le16(e + 16);
    }

    if (wdate == 0) {
        // Try last access date (no time field).
        wtime = 0;
        wdate = read_le16(e + 18);
    }

    if (out_wtime) *out_wtime = wtime;
    if (out_wdate) *out_wdate = wdate;
}

static bool disk_read(uint32_t lba, uint8_t* out) {
    return ata_read_sector(lba, out);
}

static bool disk_write(uint32_t lba, const uint8_t* in) {
    return ata_write_sector(lba, in);
}

static const char* skip_slashes(const char* p) {
    while (p && *p == '/') {
        p++;
    }
    return p ? p : "";
}

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

static bool name11_eq(const uint8_t* a, const uint8_t* b) {
    if (!a || !b) {
        return false;
    }
    for (int i = 0; i < 11; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void fat_timestamp_now(uint16_t* out_wtime, uint16_t* out_wdate) {
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

static void fat_stamp_dirent(uint8_t e[32], uint16_t wtime, uint16_t wdate, bool set_create_fields) {
    if (!e) {
        return;
    }

    // Last access date (date only).
    write_le16(e + 18, wdate);

    // Last write time/date.
    write_le16(e + 22, wtime);
    write_le16(e + 24, wdate);

    if (set_create_fields) {
        e[13] = 0; // create time (tenths)
        write_le16(e + 14, wtime);
        write_le16(e + 16, wdate);
    }
}

static void fat_root_best_ts(uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (!g_fs.ready) {
        return;
    }

    uint16_t best_time = 0;
    uint16_t best_date = 0;

    for (uint32_t s = 0; s < g_fs.root_dir_sectors; s++) {
        uint32_t lba = g_fs.first_root_lba + s;
        uint8_t sec[SECTOR_SIZE];
        if (!disk_read(lba, sec)) {
            break;
        }

        bool changed = false;

        for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
            uint8_t* e = sec + off;
            if (e[0] == 0x00) {
                // End-of-directory marker.
                s = g_fs.root_dir_sectors;
                break;
            }
            if (e[0] == 0xE5) {
                continue;
            }
            uint8_t attr = e[11];
            if (attr == FAT_ATTR_LFN) {
                continue;
            }
            if (attr & FAT_ATTR_VOLUME) {
                continue;
            }

            uint16_t wtime = 0;
            uint16_t wdate = 0;
            fat_dirent_best_ts(e, &wtime, &wdate);

            if (wdate == 0) {
                // Backfill missing timestamps for legacy images/files that had zeros.
                fat_timestamp_now(&wtime, &wdate);
                if (wdate != 0) {
                    fat_stamp_dirent(e, wtime, wdate, true);
                    changed = true;
                }
            }

            uint32_t best_key = ((uint32_t)best_date << 16) | (uint32_t)best_time;
            uint32_t key = ((uint32_t)wdate << 16) | (uint32_t)wtime;
            if (key > best_key) {
                best_time = wtime;
                best_date = wdate;
            }
        }

        if (changed) {
            (void)disk_write(lba, sec);
            (void)ata_flush();
        }
    }

    if (out_wtime) *out_wtime = best_time;
    if (out_wdate) *out_wdate = best_date;
}

static const char* fatdisk_strip_mount(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return NULL;
    }

    const char* rel = skip_slashes(abs_path);
    if (rel[0] == '\0') {
        return NULL;
    }

    if (ci_eq(rel, "disk")) {
        return "/";
    }
    if (ci_starts_with(rel, "disk/")) {
        return rel + 4; // points at "/..."
    }
    return NULL;
}

static bool fat_make_83(const char* in, uint8_t out11[11]) {
    if (!in || !out11) {
        return false;
    }

    for (int i = 0; i < 11; i++) {
        out11[i] = ' ';
    }

    if (in[0] == '.' && in[1] == '\0') {
        out11[0] = '.';
        return true;
    }
    if (in[0] == '.' && in[1] == '.' && in[2] == '\0') {
        out11[0] = '.';
        out11[1] = '.';
        return true;
    }

    uint32_t base_len = 0;
    uint32_t ext_len = 0;
    bool in_ext = false;

    for (const char* p = in; *p; p++) {
        char c = *p;
        if (c == '/') {
            break;
        }
        if (c == '.') {
            if (in_ext) {
                continue; // ignore extra dots
            }
            in_ext = true;
            continue;
        }
        if (c == ' ') {
            continue;
        }

        char up = (char)toupper((unsigned char)c);
        bool ok = isalnum((unsigned char)up) || up == '_' || up == '-' || up == '$' || up == '~';
        if (!ok) {
            up = '_';
        }

        if (!in_ext) {
            if (base_len >= 8) {
                continue;
            }
            out11[base_len++] = (uint8_t)up;
        } else {
            if (ext_len >= 3) {
                continue;
            }
            out11[8 + ext_len++] = (uint8_t)up;
        }
    }

    return base_len != 0;
}

static void fat_name_from_entry(const uint8_t* e, char* out, uint32_t out_len) {
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (!e) {
        return;
    }

    char name[9];
    char ext[4];
    memcpy(name, e + 0, 8);
    memcpy(ext, e + 8, 3);
    name[8] = '\0';
    ext[3] = '\0';

    int n_end = 7;
    while (n_end >= 0 && name[n_end] == ' ') n_end--;
    int e_end = 2;
    while (e_end >= 0 && ext[e_end] == ' ') e_end--;

    uint32_t pos = 0;
    for (int i = 0; i <= n_end && pos + 1u < out_len; i++) {
        out[pos++] = (char)tolower((unsigned char)name[i]);
    }
    if (e_end >= 0 && pos + 1u < out_len) {
        out[pos++] = '.';
        for (int i = 0; i <= e_end && pos + 1u < out_len; i++) {
            out[pos++] = (char)tolower((unsigned char)ext[i]);
        }
    }
    out[pos] = '\0';
}

// -----------------------------
// FAT long filename (LFN) support (read-only)
// -----------------------------

#define FAT_LFN_MAX_PARTS 20u
#define FAT_LFN_MAX_CHARS (FAT_LFN_MAX_PARTS * 13u)

typedef struct lfn_state {
    bool active;
    uint8_t checksum;
    uint8_t total_parts;
    uint32_t seen_mask;
    uint16_t chars[FAT_LFN_MAX_CHARS];
} lfn_state_t;

static void lfn_reset(lfn_state_t* s) {
    if (!s) {
        return;
    }
    s->active = false;
    s->checksum = 0;
    s->total_parts = 0;
    s->seen_mask = 0;
    for (uint32_t i = 0; i < FAT_LFN_MAX_CHARS; i++) {
        s->chars[i] = 0xFFFFu;
    }
}

static uint8_t fat_lfn_checksum(const uint8_t name11[11]) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < 11u; i++) {
        sum = (uint8_t)(((sum & 1u) ? 0x80u : 0u) + (sum >> 1) + name11[i]);
    }
    return sum;
}

static void lfn_feed(lfn_state_t* s, const uint8_t e[32]) {
    if (!s || !e) {
        return;
    }
    uint8_t ord_raw = e[0];
    uint8_t ord = ord_raw & 0x1Fu;
    bool last = (ord_raw & 0x40u) != 0;
    if (ord == 0 || ord > FAT_LFN_MAX_PARTS) {
        lfn_reset(s);
        return;
    }

    if (last) {
        lfn_reset(s);
        s->active = true;
        s->total_parts = ord;
        s->checksum = e[13];
    }
    if (!s->active) {
        return;
    }
    if (e[13] != s->checksum || ord > s->total_parts) {
        lfn_reset(s);
        return;
    }

    uint32_t base = (uint32_t)(ord - 1u) * 13u;
    if (base + 12u >= FAT_LFN_MAX_CHARS) {
        lfn_reset(s);
        return;
    }

    // UCS-2 name fragments in 3 groups: 5 + 6 + 2 = 13 code units.
    for (uint32_t i = 0; i < 5u; i++) {
        s->chars[base + i] = read_le16(e + 1u + i * 2u);
    }
    for (uint32_t i = 0; i < 6u; i++) {
        s->chars[base + 5u + i] = read_le16(e + 14u + i * 2u);
    }
    for (uint32_t i = 0; i < 2u; i++) {
        s->chars[base + 11u + i] = read_le16(e + 28u + i * 2u);
    }

    s->seen_mask |= (1u << (ord - 1u));
}

static bool lfn_name_for_sfn(const lfn_state_t* s, const uint8_t sfn_entry[32], char* out, uint32_t out_len) {
    if (!s || !sfn_entry || !out || out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!s->active || s->total_parts == 0) {
        return false;
    }
    uint32_t parts = s->total_parts;
    if (parts > FAT_LFN_MAX_PARTS) {
        return false;
    }
    uint32_t full_mask = (parts == 32u) ? 0xFFFFFFFFu : ((1u << parts) - 1u);
    if ((s->seen_mask & full_mask) != full_mask) {
        return false;
    }
    uint8_t sum = fat_lfn_checksum(sfn_entry);
    if (sum != s->checksum) {
        return false;
    }

    uint32_t max_chars = parts * 13u;
    uint32_t pos = 0;
    for (uint32_t i = 0; i < max_chars && pos + 1u < out_len; i++) {
        uint16_t ch = s->chars[i];
        if (ch == 0x0000u || ch == 0xFFFFu) {
            break;
        }
        if (ch <= 0x7Fu) {
            out[pos++] = (char)ch;
        } else {
            out[pos++] = '?';
        }
    }
    out[pos] = '\0';
    return out[0] != '\0';
}

static uint32_t cluster_to_lba(uint16_t cluster) {
    return g_fs.first_data_lba + (uint32_t)(cluster - 2u) * (uint32_t)g_fs.sectors_per_cluster;
}

static bool fat_is_eoc(uint16_t v) {
    return v >= 0xFFF8u;
}

static bool fat_get(uint16_t cluster, uint16_t* out_next) {
    if (!out_next || !g_fs.ready) {
        return false;
    }
    if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
        return false;
    }

    uint32_t offset = (uint32_t)cluster * 2u;
    uint32_t sector_index = offset / SECTOR_SIZE;
    uint32_t in_off = offset % SECTOR_SIZE;
    uint32_t lba = g_fs.fat_start_lba + sector_index;

    uint8_t sec[SECTOR_SIZE];
    if (!disk_read(lba, sec)) {
        return false;
    }

    *out_next = (uint16_t)sec[in_off] | (uint16_t)((uint16_t)sec[in_off + 1u] << 8);
    return true;
}

static bool fat_set_one(uint32_t fat_base_lba, uint16_t cluster, uint16_t value) {
    uint32_t offset = (uint32_t)cluster * 2u;
    uint32_t sector_index = offset / SECTOR_SIZE;
    uint32_t in_off = offset % SECTOR_SIZE;
    uint32_t lba = fat_base_lba + sector_index;

    uint8_t sec[SECTOR_SIZE];
    if (!disk_read(lba, sec)) {
        return false;
    }

    sec[in_off] = (uint8_t)(value & 0xFFu);
    sec[in_off + 1u] = (uint8_t)((value >> 8) & 0xFFu);

    return disk_write(lba, sec);
}

static bool fat_set(uint16_t cluster, uint16_t value) {
    if (!g_fs.ready) {
        return false;
    }
    if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
        return false;
    }
    for (uint8_t i = 0; i < g_fs.num_fats; i++) {
        uint32_t fat_base = g_fs.fat_start_lba + (uint32_t)i * (uint32_t)g_fs.fat_sectors;
        if (!fat_set_one(fat_base, cluster, value)) {
            return false;
        }
    }
    return true;
}

static bool fat_find_free_cluster(uint16_t* out_cluster) {
    if (!out_cluster || !g_fs.ready) {
        return false;
    }

    uint16_t start = g_fs.alloc_cursor;
    if (start < 2u) start = 2u;
    uint16_t max_cluster = (uint16_t)(g_fs.cluster_count + 1u);
    if (max_cluster < 2u) {
        return false;
    }

    uint8_t sec[SECTOR_SIZE];
    uint32_t cached_lba = 0xFFFFFFFFu;

    uint32_t steps = g_fs.cluster_count;
    for (uint32_t step = 0; step < steps; step++) {
        uint16_t c = (uint16_t)(start + step);
        if (c > max_cluster) {
            c = (uint16_t)(2u + (c - 2u) % (max_cluster - 1u));
        }

        uint32_t off = (uint32_t)c * 2u;
        uint32_t lba = g_fs.fat_start_lba + (off / SECTOR_SIZE);
        uint32_t in_off = off % SECTOR_SIZE;

        if (lba != cached_lba) {
            if (!disk_read(lba, sec)) {
                return false;
            }
            cached_lba = lba;
        }

        uint16_t v = (uint16_t)sec[in_off] | (uint16_t)((uint16_t)sec[in_off + 1u] << 8);
        if (v == 0u) {
            *out_cluster = c;
            g_fs.alloc_cursor = (uint16_t)(c + 1u);
            return true;
        }
    }

    return false;
}

typedef struct dir_loc {
    uint32_t lba;
    uint16_t off;
} dir_loc_t;

typedef struct fat_dir {
    bool is_root;
    uint16_t cluster;
} fat_dir_t;

static bool dir_entry_is_free(const uint8_t* e) {
    return e[0] == 0x00 || e[0] == 0xE5;
}

static bool dir_entry_is_valid(const uint8_t* e) {
    if (!e) {
        return false;
    }
    if (e[0] == 0x00 || e[0] == 0xE5) {
        return false;
    }
    uint8_t attr = e[11];
    if (attr == FAT_ATTR_LFN) {
        return false;
    }
    if (attr & FAT_ATTR_VOLUME) {
        return false;
    }
    return true;
}

static bool dir_iter_find_by_name(fat_dir_t dir, const uint8_t name11[11], dir_loc_t* out_loc, uint8_t out_entry[32]) {
    if (!g_fs.ready || !name11) {
        return false;
    }

    uint8_t sec[SECTOR_SIZE];

    if (dir.is_root) {
        uint32_t total = g_fs.root_dir_sectors;
        for (uint32_t s = 0; s < total; s++) {
            uint32_t lba = g_fs.first_root_lba + s;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                const uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    return false;
                }
                if (!dir_entry_is_valid(e)) {
                    continue;
                }
                if (name11_eq(e + 0, name11)) {
                    if (out_loc) {
                        out_loc->lba = lba;
                        out_loc->off = (uint16_t)off;
                    }
                    if (out_entry) {
                        memcpy(out_entry, e, 32);
                    }
                    return true;
                }
            }
        }
        return false;
    }

    uint16_t cluster = dir.cluster;
    if (cluster < 2u) {
        return false;
    }

    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            return false;
        }

        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
            uint32_t lba = base + si;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                const uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    return false;
                }
                if (!dir_entry_is_valid(e)) {
                    continue;
                }
                if (name11_eq(e + 0, name11)) {
                    if (out_loc) {
                        out_loc->lba = lba;
                        out_loc->off = (uint16_t)off;
                    }
                    if (out_entry) {
                        memcpy(out_entry, e, 32);
                    }
                    return true;
                }
            }
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            return false;
        }
        cluster = next;
    }
    return false;
}

static bool dir_iter_find_by_name_str(fat_dir_t dir, const char* name, dir_loc_t* out_loc, uint8_t out_entry[32]) {
    if (!g_fs.ready || !name) {
        return false;
    }

    uint8_t sec[SECTOR_SIZE];
    lfn_state_t lfn;
    lfn_reset(&lfn);

    char cur_name[256];
    uint8_t want11[11];
    bool have_want11 = fat_make_83(name, want11);

    if (dir.is_root) {
        uint32_t total = g_fs.root_dir_sectors;
        for (uint32_t s = 0; s < total; s++) {
            uint32_t lba = g_fs.first_root_lba + s;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                const uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    return false;
                }
                if (e[0] == 0xE5) {
                    lfn_reset(&lfn);
                    continue;
                }

                uint8_t attr = e[11];
                if (attr == FAT_ATTR_LFN) {
                    lfn_feed(&lfn, e);
                    continue;
                }
                if (attr & FAT_ATTR_VOLUME) {
                    lfn_reset(&lfn);
                    continue;
                }

                bool have_lfn = lfn_name_for_sfn(&lfn, e, cur_name, sizeof(cur_name));
                if (!have_lfn) {
                    fat_name_from_entry(e, cur_name, sizeof(cur_name));
                }
                lfn_reset(&lfn);

                if (ci_eq(cur_name, name) || (have_want11 && name11_eq(e + 0, want11))) {
                    if (out_loc) {
                        out_loc->lba = lba;
                        out_loc->off = (uint16_t)off;
                    }
                    if (out_entry) {
                        memcpy(out_entry, e, 32);
                    }
                    return true;
                }
            }
        }
        return false;
    }

    uint16_t cluster = dir.cluster;
    if (cluster < 2u) {
        return false;
    }

    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            return false;
        }

        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
            uint32_t lba = base + si;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                const uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    return false;
                }
                if (e[0] == 0xE5) {
                    lfn_reset(&lfn);
                    continue;
                }

                uint8_t attr = e[11];
                if (attr == FAT_ATTR_LFN) {
                    lfn_feed(&lfn, e);
                    continue;
                }
                if (attr & FAT_ATTR_VOLUME) {
                    lfn_reset(&lfn);
                    continue;
                }

                bool have_lfn = lfn_name_for_sfn(&lfn, e, cur_name, sizeof(cur_name));
                if (!have_lfn) {
                    fat_name_from_entry(e, cur_name, sizeof(cur_name));
                }
                lfn_reset(&lfn);

                if (ci_eq(cur_name, name) || (have_want11 && name11_eq(e + 0, want11))) {
                    if (out_loc) {
                        out_loc->lba = lba;
                        out_loc->off = (uint16_t)off;
                    }
                    if (out_entry) {
                        memcpy(out_entry, e, 32);
                    }
                    return true;
                }
            }
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            return false;
        }
        cluster = next;
    }
    return false;
}

static bool dir_find_free_slot(fat_dir_t dir, dir_loc_t* out_loc) {
    if (!g_fs.ready || !out_loc) {
        return false;
    }

    uint8_t sec[SECTOR_SIZE];

    if (dir.is_root) {
        uint32_t total = g_fs.root_dir_sectors;
        for (uint32_t s = 0; s < total; s++) {
            uint32_t lba = g_fs.first_root_lba + s;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                if (dir_entry_is_free(sec + off)) {
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    return true;
                }
            }
        }
        return false;
    }

    uint16_t cluster = dir.cluster;
    if (cluster < 2u) {
        return false;
    }

    uint16_t last = cluster;
    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            return false;
        }

        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
            uint32_t lba = base + si;
            if (!disk_read(lba, sec)) {
                return false;
            }
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                if (dir_entry_is_free(sec + off)) {
                    out_loc->lba = lba;
                    out_loc->off = (uint16_t)off;
                    return true;
                }
            }
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            break;
        }
        last = next;
        cluster = next;
    }

    // No free slots: grow directory by 1 cluster.
    uint16_t new_cluster = 0;
    if (!fat_find_free_cluster(&new_cluster)) {
        return false;
    }
    if (!fat_set(new_cluster, 0xFFFFu)) {
        return false;
    }

    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));
    uint32_t base = cluster_to_lba(new_cluster);
    for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
        if (!disk_write(base + si, zero)) {
            (void)fat_set(new_cluster, 0u);
            return false;
        }
    }

    if (!fat_set(last, new_cluster)) {
        (void)fat_set(new_cluster, 0u);
        return false;
    }

    out_loc->lba = base;
    out_loc->off = 0;
    return true;
}

static bool resolve_parent_dir(const char* abs_path, fat_dir_t* out_dir, uint8_t out_name11[11]) {
    if (!out_dir || !out_name11) {
        return false;
    }
    out_dir->is_root = true;
    out_dir->cluster = 0;

    const char* rel = fatdisk_strip_mount(abs_path);
    if (!rel) {
        return false;
    }
    if (rel[0] != '/') {
        return false;
    }

    const char* p = rel;
    while (*p == '/') p++;
    if (*p == '\0') {
        return false; // mount root has no parent/name
    }

    char seg[64];
    uint8_t seg11[11];

    for (;;) {
        while (*p == '/') p++;
        if (*p == '\0') {
            return false;
        }

        uint32_t len = 0;
        while (p[len] && p[len] != '/') {
            if (len + 1u >= sizeof(seg)) {
                return false;
            }
            seg[len] = p[len];
            len++;
        }
        seg[len] = '\0';
        p += len;

        while (*p == '/') p++;
        bool last = (*p == '\0');

        if (!fat_make_83(seg, seg11)) {
            return false;
        }

        if (last) {
            memcpy(out_name11, seg11, 11);
            return true;
        }

        // Traverse into directory.
        dir_loc_t loc;
        uint8_t ent[32];
        if (!dir_iter_find_by_name(*out_dir, seg11, &loc, ent)) {
            return false;
        }
        uint8_t attr = ent[11];
        if ((attr & FAT_ATTR_DIR) == 0) {
            return false;
        }

        uint16_t cl = read_le16(ent + 26);
        out_dir->is_root = false;
        out_dir->cluster = cl;
    }
}

static bool lookup_path_entry(const char* abs_path, dir_loc_t* out_loc, uint8_t out_entry[32]) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }

    const char* rel = fatdisk_strip_mount(abs_path);
    if (!rel || rel[0] != '/') {
        return false;
    }

    const char* p = rel;
    while (*p == '/') p++;
    if (*p == '\0') {
        return false;
    }

    fat_dir_t dir;
    dir.is_root = true;
    dir.cluster = 0;

    char seg[128];

    for (;;) {
        while (*p == '/') p++;
        if (*p == '\0') {
            return false;
        }

        uint32_t len = 0;
        while (p[len] && p[len] != '/') {
            if (len + 1u >= sizeof(seg)) {
                return false;
            }
            seg[len] = p[len];
            len++;
        }
        seg[len] = '\0';
        p += len;

        while (*p == '/') p++;
        bool last = (*p == '\0');

        dir_loc_t loc;
        uint8_t ent[32];
        if (!dir_iter_find_by_name_str(dir, seg, &loc, ent)) {
            return false;
        }

        if (last) {
            if (out_loc) {
                *out_loc = loc;
            }
            if (out_entry) {
                memcpy(out_entry, ent, 32);
            }
            return true;
        }

        if ((ent[11] & FAT_ATTR_DIR) == 0) {
            return false;
        }
        uint16_t cl = read_le16(ent + 26);
        dir.is_root = false;
        dir.cluster = cl;
        if (dir.cluster < 2u) {
            return false;
        }
    }
}

static bool is_root_path(const char* abs_path) {
    const char* rel = fatdisk_strip_mount(abs_path);
    return rel && strcmp(rel, "/") == 0;
}

static bool write_dir_entry_at(dir_loc_t loc, const uint8_t entry[32]) {
    uint8_t sec[SECTOR_SIZE];
    if (!disk_read(loc.lba, sec)) {
        return false;
    }
    if ((uint32_t)loc.off + 32u > SECTOR_SIZE) {
        return false;
    }
    memcpy(sec + loc.off, entry, 32);
    return disk_write(loc.lba, sec);
}

static bool free_cluster_chain(uint16_t start) {
    if (!g_fs.ready) {
        return false;
    }
    uint16_t cluster = start;
    uint32_t max_steps = g_fs.cluster_count + 4u;

    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            return true;
        }
        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (!fat_set(cluster, 0u)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            return true;
        }
        cluster = next;
    }
    return false;
}

static bool write_file_data(uint16_t start_cluster, const uint8_t* data, uint32_t size) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    if (start_cluster < 2u) {
        return false;
    }

    uint32_t remaining = size;
    uint16_t cluster = start_cluster;
    uint8_t sec[SECTOR_SIZE];

    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps && remaining; step++) {
        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster && remaining; si++) {
            memset(sec, 0, sizeof(sec));
            uint32_t to_copy = remaining;
            if (to_copy > SECTOR_SIZE) {
                to_copy = SECTOR_SIZE;
            }
            memcpy(sec, data, to_copy);
            if (!disk_write(base + si, sec)) {
                return false;
            }
            data += to_copy;
            remaining -= to_copy;
        }

        if (!remaining) {
            break;
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            return false;
        }
        cluster = next;
    }

    return remaining == 0;
}

static bool read_file_data(uint16_t start_cluster, uint8_t* dst, uint32_t size) {
    if (size == 0) {
        return true;
    }
    if (!dst) {
        return false;
    }
    if (start_cluster < 2u) {
        return false;
    }

    uint32_t remaining = size;
    uint16_t cluster = start_cluster;
    uint8_t sec[SECTOR_SIZE];

    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps && remaining; step++) {
        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster && remaining; si++) {
            if (!disk_read(base + si, sec)) {
                return false;
            }
            uint32_t to_copy = remaining;
            if (to_copy > SECTOR_SIZE) {
                to_copy = SECTOR_SIZE;
            }
            memcpy(dst, sec, to_copy);
            dst += to_copy;
            remaining -= to_copy;
        }

        if (!remaining) {
            break;
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next)) {
            return false;
        }
        if (fat_is_eoc(next)) {
            return false;
        }
        cluster = next;
    }

    return remaining == 0;
}

static bool alloc_chain(uint32_t clusters_needed, uint16_t* out_start) {
    *out_start = 0;
    if (clusters_needed == 0) {
        return true;
    }

    uint16_t first = 0;
    uint16_t prev = 0;
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t c = 0;
        if (!fat_find_free_cluster(&c)) {
            if (first) {
                (void)free_cluster_chain(first);
            }
            return false;
        }
        if (!fat_set(c, 0xFFFFu)) {
            if (first) {
                (void)free_cluster_chain(first);
            }
            return false;
        }
        if (!first) {
            first = c;
        }
        if (prev) {
            if (!fat_set(prev, c)) {
                (void)free_cluster_chain(first);
                return false;
            }
        }
        prev = c;
    }
    *out_start = first;
    return true;
}

bool fatdisk_init(void) {
    memset(&g_fs, 0, sizeof(g_fs));
    strncpy(g_fs.label, "disk", sizeof(g_fs.label) - 1u);
    g_fs.label[sizeof(g_fs.label) - 1u] = '\0';

    if (!ata_is_present()) {
        (void)ata_init();
    }
    if (!ata_is_present()) {
        serial_write_string("[FATDISK] no ATA disk\n");
        return false;
    }

    uint8_t bpb[SECTOR_SIZE];
    if (!disk_read(0, bpb)) {
        serial_write_string("[FATDISK] failed to read BPB\n");
        return false;
    }

    uint16_t bytes_per_sector = read_le16(bpb + 11);
    uint8_t sectors_per_cluster = bpb[13];
    uint16_t reserved_sectors = read_le16(bpb + 14);
    uint8_t num_fats = bpb[16];
    uint16_t root_entries = read_le16(bpb + 17);
    uint16_t total16 = read_le16(bpb + 19);
    uint16_t fat_sectors = read_le16(bpb + 22);
    uint32_t total32 = read_le32(bpb + 32);

    if (bytes_per_sector != SECTOR_SIZE) {
        serial_write_string("[FATDISK] unsupported bytes/sector\n");
        return false;
    }
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1u)) != 0) {
        serial_write_string("[FATDISK] invalid sectors/cluster\n");
        return false;
    }
    if (reserved_sectors == 0 || num_fats == 0 || fat_sectors == 0) {
        serial_write_string("[FATDISK] invalid FAT layout\n");
        return false;
    }

    uint32_t total_sectors = total16 ? (uint32_t)total16 : total32;
    if (total_sectors == 0) {
        serial_write_string("[FATDISK] invalid total sectors\n");
        return false;
    }

    uint32_t root_dir_sectors = ((uint32_t)root_entries * 32u + (bytes_per_sector - 1u)) / bytes_per_sector;
    uint32_t fat_start_lba = (uint32_t)reserved_sectors;
    uint32_t first_root_lba = fat_start_lba + (uint32_t)num_fats * (uint32_t)fat_sectors;
    uint32_t first_data_lba = first_root_lba + root_dir_sectors;
    if (first_data_lba >= total_sectors) {
        serial_write_string("[FATDISK] invalid data start\n");
        return false;
    }

    uint32_t data_sectors = total_sectors - first_data_lba;
    uint32_t cluster_count = data_sectors / (uint32_t)sectors_per_cluster;
    if (cluster_count < 4085u || cluster_count >= 65525u) {
        serial_write_string("[FATDISK] only FAT16 supported\n");
        return false;
    }

    // FAT12/16 volume label at offset 43, len 11.
    char label[12];
    memcpy(label, bpb + 43, 11);
    label[11] = '\0';
    int end = 10;
    while (end >= 0 && (label[end] == ' ' || label[end] == '\t')) {
        label[end--] = '\0';
    }
    if (label[0] != '\0') {
        strncpy(g_fs.label, label, sizeof(g_fs.label) - 1u);
        g_fs.label[sizeof(g_fs.label) - 1u] = '\0';
    }

    g_fs.bytes_per_sector = bytes_per_sector;
    g_fs.sectors_per_cluster = sectors_per_cluster;
    g_fs.reserved_sectors = reserved_sectors;
    g_fs.num_fats = num_fats;
    g_fs.root_entries = root_entries;
    g_fs.total_sectors = total_sectors;
    g_fs.fat_sectors = fat_sectors;
    g_fs.root_dir_sectors = root_dir_sectors;
    g_fs.fat_start_lba = fat_start_lba;
    g_fs.first_root_lba = first_root_lba;
    g_fs.first_data_lba = first_data_lba;
    g_fs.cluster_count = cluster_count;
    g_fs.cluster_size_bytes = (uint32_t)bytes_per_sector * (uint32_t)sectors_per_cluster;
    g_fs.alloc_cursor = 2u;
    g_fs.ready = true;

    serial_write_string("[FATDISK] mounted label=");
    serial_write_string(g_fs.label);
    serial_write_string(" clusters=");
    serial_write_dec((int32_t)g_fs.cluster_count);
    serial_write_char('\n');

    return true;
}

bool fatdisk_is_ready(void) {
    return g_fs.ready;
}

const char* fatdisk_label(void) {
    return g_fs.label;
}

bool fatdisk_statfs(uint32_t* out_bsize, uint32_t* out_blocks, uint32_t* out_bfree) {
    if (out_bsize) {
        *out_bsize = 0;
    }
    if (out_blocks) {
        *out_blocks = 0;
    }
    if (out_bfree) {
        *out_bfree = 0;
    }
    if (!g_fs.ready) {
        return false;
    }

    uint32_t bsize = (uint32_t)g_fs.bytes_per_sector;
    uint32_t blocks = g_fs.cluster_count * (uint32_t)g_fs.sectors_per_cluster; // usable data blocks (sectors)

    uint32_t free_clusters = 0;
    uint16_t max_cluster = (uint16_t)(g_fs.cluster_count + 1u);

    uint8_t sec[SECTOR_SIZE];
    uint32_t cached_lba = 0xFFFFFFFFu;

    for (uint16_t c = 2u; c <= max_cluster; c++) {
        uint32_t off = (uint32_t)c * 2u;
        uint32_t lba = g_fs.fat_start_lba + (off / SECTOR_SIZE);
        uint32_t in_off = off % SECTOR_SIZE;

        if (lba != cached_lba) {
            if (!disk_read(lba, sec)) {
                return false;
            }
            cached_lba = lba;
        }

        uint16_t v = (uint16_t)sec[in_off] | (uint16_t)((uint16_t)sec[in_off + 1u] << 8);
        if (v == 0u) {
            free_clusters++;
        }
    }

    uint32_t bfree = free_clusters * (uint32_t)g_fs.sectors_per_cluster;

    if (out_bsize) {
        *out_bsize = bsize;
    }
    if (out_blocks) {
        *out_blocks = blocks;
    }
    if (out_bfree) {
        *out_bfree = bfree;
    }
    return true;
}

bool fatdisk_is_dir(const char* abs_path) {
    if (!g_fs.ready) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return true;
    }
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, NULL, ent)) {
        return false;
    }
    return (ent[11] & FAT_ATTR_DIR) != 0;
}

bool fatdisk_is_file(const char* abs_path) {
    if (!g_fs.ready) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return false;
    }
    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }
    return (ent[11] & FAT_ATTR_DIR) == 0;
}

bool fatdisk_stat(const char* abs_path, bool* out_is_dir, uint32_t* out_size) {
    if (out_is_dir) {
        *out_is_dir = false;
    }
    if (out_size) {
        *out_size = 0;
    }
    if (!g_fs.ready || !abs_path) {
        return false;
    }

    if (is_root_path(abs_path)) {
        if (out_is_dir) *out_is_dir = true;
        if (out_size) *out_size = 0;
        return true;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }

    bool is_dir = (ent[11] & FAT_ATTR_DIR) != 0;
    if (out_is_dir) {
        *out_is_dir = is_dir;
    }
    if (out_size) {
        *out_size = is_dir ? 0u : read_le32(ent + 28);
    }
    return true;
}

bool fatdisk_stat_ex(const char* abs_path, bool* out_is_dir, uint32_t* out_size, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_is_dir) {
        *out_is_dir = false;
    }
    if (out_size) {
        *out_size = 0;
    }
    if (out_wtime) {
        *out_wtime = 0;
    }
    if (out_wdate) {
        *out_wdate = 0;
    }
    if (!g_fs.ready || !abs_path) {
        return false;
    }

    if (is_root_path(abs_path)) {
        if (out_is_dir) *out_is_dir = true;
        if (out_size) *out_size = 0;
        fat_root_best_ts(out_wtime, out_wdate);
        return true;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }

    bool is_dir = (ent[11] & FAT_ATTR_DIR) != 0;
    if (out_is_dir) {
        *out_is_dir = is_dir;
    }
    if (out_size) {
        *out_size = is_dir ? 0u : read_le32(ent + 28);
    }
    uint16_t wtime = 0;
    uint16_t wdate = 0;
    fat_dirent_best_ts(ent, &wtime, &wdate);

    // Backfill missing timestamps for legacy images/files that had zeros.
    if (wdate == 0) {
        fat_timestamp_now(&wtime, &wdate);
        if (wdate != 0) {
            fat_stamp_dirent(ent, wtime, wdate, true);
            (void)write_dir_entry_at(loc, ent);
            (void)ata_flush();
        }
    }

    if (out_wtime) *out_wtime = wtime;
    if (out_wdate) *out_wdate = wdate;
    return true;
}

bool fatdisk_get_meta(const char* abs_path, bool* out_is_symlink, uint16_t* out_mode) {
    if (out_is_symlink) {
        *out_is_symlink = false;
    }
    if (out_mode) {
        *out_mode = 0;
    }
    if (!g_fs.ready || !abs_path) {
        return false;
    }

    if (is_root_path(abs_path)) {
        if (out_is_symlink) *out_is_symlink = false;
        if (out_mode) *out_mode = 0755u;
        return true;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }

    bool is_dir = (ent[11] & FAT_ATTR_DIR) != 0;
    bool is_symlink = false;
    uint16_t mode = is_dir ? 0755u : 0644u;

    if ((ent[12] & FAT_META_PRESENT) != 0) {
        is_symlink = (ent[12] & FAT_META_SYMLINK) != 0;
        mode = (uint16_t)(read_le16(ent + 20) & 07777u);
    }

    if (out_is_symlink) *out_is_symlink = is_symlink;
    if (out_mode) *out_mode = mode;
    return true;
}

bool fatdisk_set_meta(const char* abs_path, bool is_symlink, uint16_t mode) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return false;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }

    uint8_t nt = ent[12];
    nt |= FAT_META_PRESENT;
    if (is_symlink) {
        nt |= FAT_META_SYMLINK;
    } else {
        nt &= (uint8_t)~FAT_META_SYMLINK;
    }
    ent[12] = nt;
    write_le16(ent + 20, (uint16_t)(mode & 07777u));

    if (!write_dir_entry_at(loc, ent)) {
        return false;
    }
    (void)ata_flush();
    return true;
}

uint32_t fatdisk_list_dir(const char* abs_path, fatdisk_dirent_t* out, uint32_t max) {
    if (!g_fs.ready || !out || max == 0) {
        return 0;
    }

    fat_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    if (is_root_path(abs_path)) {
        dir.is_root = true;
        dir.cluster = 0;
    } else {
        dir_loc_t loc;
        uint8_t ent[32];
        if (!lookup_path_entry(abs_path, &loc, ent)) {
            return 0;
        }
        if ((ent[11] & FAT_ATTR_DIR) == 0) {
            return 0;
        }
        dir.is_root = false;
        dir.cluster = read_le16(ent + 26);
        if (dir.cluster < 2u) {
            return 0;
        }
    }

    uint32_t count = 0;
    uint8_t sec[SECTOR_SIZE];

    bool wrote_any = false;
    lfn_state_t lfn;
    lfn_reset(&lfn);
    char long_name[256];

    if (dir.is_root) {
        uint32_t total = g_fs.root_dir_sectors;
        for (uint32_t s = 0; s < total && count < max; s++) {
            uint32_t lba = g_fs.first_root_lba + s;
            if (!disk_read(lba, sec)) {
                break;
            }
            bool dirty_sector = false;
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE && count < max; off += 32u) {
                uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    if (dirty_sector) {
                        wrote_any |= disk_write(lba, sec);
                    }
                    if (wrote_any) {
                        (void)ata_flush();
                    }
                    return count;
                }
                if (e[0] == 0xE5) {
                    lfn_reset(&lfn);
                    continue;
                }

                uint8_t attr = e[11];
                if (attr == FAT_ATTR_LFN) {
                    lfn_feed(&lfn, e);
                    continue;
                }
                if (attr & FAT_ATTR_VOLUME) {
                    lfn_reset(&lfn);
                    continue;
                }

                bool have_lfn = lfn_name_for_sfn(&lfn, e, long_name, sizeof(long_name));
                if (!have_lfn) {
                    fat_name_from_entry(e, long_name, sizeof(long_name));
                }
                lfn_reset(&lfn);

                strncpy(out[count].name, long_name, sizeof(out[count].name) - 1u);
                out[count].name[sizeof(out[count].name) - 1u] = '\0';
                out[count].is_dir = (e[11] & FAT_ATTR_DIR) != 0;
                out[count].is_symlink = false;
                out[count].mode = out[count].is_dir ? 0755u : 0644u;
                if ((e[12] & FAT_META_PRESENT) != 0) {
                    out[count].is_symlink = (e[12] & FAT_META_SYMLINK) != 0;
                    out[count].mode = (uint16_t)(read_le16(e + 20) & 07777u);
                }
                out[count].size = out[count].is_dir ? 0u : read_le32(e + 28);
                uint16_t wtime = 0;
                uint16_t wdate = 0;
                fat_dirent_best_ts(e, &wtime, &wdate);
                if (wdate == 0) {
                    fat_timestamp_now(&wtime, &wdate);
                    if (wdate != 0) {
                        fat_stamp_dirent(e, wtime, wdate, true);
                        dirty_sector = true;
                    }
                }
                out[count].wtime = wtime;
                out[count].wdate = wdate;
                count++;
            }
            if (dirty_sector) {
                wrote_any |= disk_write(lba, sec);
            }
        }
        if (wrote_any) {
            (void)ata_flush();
        }
        return count;
    }

    uint16_t cluster = dir.cluster;
    uint32_t max_steps = g_fs.cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps && count < max; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            break;
        }
        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster && count < max; si++) {
            uint32_t lba = base + si;
            if (!disk_read(lba, sec)) {
                if (wrote_any) {
                    (void)ata_flush();
                }
                return count;
            }
            bool dirty_sector = false;
            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE && count < max; off += 32u) {
                uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    if (dirty_sector) {
                        wrote_any |= disk_write(lba, sec);
                    }
                    if (wrote_any) {
                        (void)ata_flush();
                    }
                    return count;
                }
                if (e[0] == 0xE5) {
                    lfn_reset(&lfn);
                    continue;
                }

                uint8_t attr = e[11];
                if (attr == FAT_ATTR_LFN) {
                    lfn_feed(&lfn, e);
                    continue;
                }
                if (attr & FAT_ATTR_VOLUME) {
                    lfn_reset(&lfn);
                    continue;
                }

                bool have_lfn = lfn_name_for_sfn(&lfn, e, long_name, sizeof(long_name));
                if (!have_lfn) {
                    fat_name_from_entry(e, long_name, sizeof(long_name));
                }
                lfn_reset(&lfn);

                strncpy(out[count].name, long_name, sizeof(out[count].name) - 1u);
                out[count].name[sizeof(out[count].name) - 1u] = '\0';
                out[count].is_dir = (e[11] & FAT_ATTR_DIR) != 0;
                out[count].is_symlink = false;
                out[count].mode = out[count].is_dir ? 0755u : 0644u;
                if ((e[12] & FAT_META_PRESENT) != 0) {
                    out[count].is_symlink = (e[12] & FAT_META_SYMLINK) != 0;
                    out[count].mode = (uint16_t)(read_le16(e + 20) & 07777u);
                }
                out[count].size = out[count].is_dir ? 0u : read_le32(e + 28);
                uint16_t wtime = 0;
                uint16_t wdate = 0;
                fat_dirent_best_ts(e, &wtime, &wdate);
                if (wdate == 0) {
                    fat_timestamp_now(&wtime, &wdate);
                    if (wdate != 0) {
                        fat_stamp_dirent(e, wtime, wdate, true);
                        dirty_sector = true;
                    }
                }
                out[count].wtime = wtime;
                out[count].wdate = wdate;
                count++;
            }
            if (dirty_sector) {
                wrote_any |= disk_write(lba, sec);
            }
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next) || fat_is_eoc(next)) {
            break;
        }
        cluster = next;
    }

    if (wrote_any) {
        (void)ata_flush();
    }
    return count;
}

bool fatdisk_read_file_alloc(const char* abs_path, uint8_t** out_data, uint32_t* out_size) {
    if (!out_data) {
        return false;
    }
    *out_data = NULL;
    if (out_size) {
        *out_size = 0;
    }
    if (!g_fs.ready || !abs_path) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return false;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }
    if (ent[11] & FAT_ATTR_DIR) {
        return false;
    }

    uint32_t size = read_le32(ent + 28);
    uint16_t start = read_le16(ent + 26);

    if (size == 0) {
        uint8_t* buf = (uint8_t*)kmalloc(1u);
        if (!buf) {
            return false;
        }
        buf[0] = 0;
        *out_data = buf;
        if (out_size) *out_size = 0;
        return true;
    }

    uint8_t* buf = (uint8_t*)kmalloc(size);
    if (!buf) {
        return false;
    }

    if (!read_file_data(start, buf, size)) {
        kfree(buf);
        return false;
    }

    *out_data = buf;
    if (out_size) *out_size = size;
    return true;
}

bool fatdisk_write_file(const char* abs_path, const uint8_t* data, uint32_t size, bool overwrite) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }

    fat_dir_t parent;
    uint8_t name11[11];
    if (!resolve_parent_dir(abs_path, &parent, name11)) {
        return false;
    }

    dir_loc_t ent_loc;
    uint8_t ent[32];
    bool exists = dir_iter_find_by_name(parent, name11, &ent_loc, ent);
    if (exists) {
        if (ent[11] & FAT_ATTR_DIR) {
            return false;
        }
        if (!overwrite) {
            return false;
        }

        uint16_t old = read_le16(ent + 26);
        if (old >= 2u) {
            if (!free_cluster_chain(old)) {
                return false;
            }
        }
    }

    uint16_t start_cluster = 0;
    uint32_t clusters_needed = (size + g_fs.cluster_size_bytes - 1u) / g_fs.cluster_size_bytes;
    if (!alloc_chain(clusters_needed, &start_cluster)) {
        return false;
    }

    if (clusters_needed && !write_file_data(start_cluster, data, size)) {
        (void)free_cluster_chain(start_cluster);
        return false;
    }

    uint16_t wtime = 0;
    uint16_t wdate = 0;
    fat_timestamp_now(&wtime, &wdate);

    if (!exists) {
        dir_loc_t slot;
        if (!dir_find_free_slot(parent, &slot)) {
            if (start_cluster) {
                (void)free_cluster_chain(start_cluster);
            }
            return false;
        }

        uint8_t newe[32];
        memset(newe, 0, sizeof(newe));
        memcpy(newe + 0, name11, 11);
        newe[11] = FAT_ATTR_ARCHIVE;
        fat_stamp_dirent(newe, wtime, wdate, true);
        newe[26] = (uint8_t)(start_cluster & 0xFFu);
        newe[27] = (uint8_t)((start_cluster >> 8) & 0xFFu);
        newe[28] = (uint8_t)(size & 0xFFu);
        newe[29] = (uint8_t)((size >> 8) & 0xFFu);
        newe[30] = (uint8_t)((size >> 16) & 0xFFu);
        newe[31] = (uint8_t)((size >> 24) & 0xFFu);

        if (!write_dir_entry_at(slot, newe)) {
            if (start_cluster) {
                (void)free_cluster_chain(start_cluster);
            }
            return false;
        }
    } else {
        ent[11] = FAT_ATTR_ARCHIVE;
        fat_stamp_dirent(ent, wtime, wdate, false);
        ent[26] = (uint8_t)(start_cluster & 0xFFu);
        ent[27] = (uint8_t)((start_cluster >> 8) & 0xFFu);
        ent[28] = (uint8_t)(size & 0xFFu);
        ent[29] = (uint8_t)((size >> 8) & 0xFFu);
        ent[30] = (uint8_t)((size >> 16) & 0xFFu);
        ent[31] = (uint8_t)((size >> 24) & 0xFFu);
        if (!write_dir_entry_at(ent_loc, ent)) {
            if (start_cluster) {
                (void)free_cluster_chain(start_cluster);
            }
            return false;
        }
    }

    (void)ata_flush();
    return true;
}

bool fatdisk_mkdir(const char* abs_path) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return true;
    }

    fat_dir_t parent;
    uint8_t name11[11];
    if (!resolve_parent_dir(abs_path, &parent, name11)) {
        return false;
    }

    dir_loc_t existing_loc;
    uint8_t existing[32];
    if (dir_iter_find_by_name(parent, name11, &existing_loc, existing)) {
        return false; // already exists
    }

    uint16_t new_cluster = 0;
    if (!alloc_chain(1, &new_cluster) || new_cluster < 2u) {
        return false;
    }

    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));
    uint32_t base = cluster_to_lba(new_cluster);
    for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
        if (!disk_write(base + si, zero)) {
            (void)free_cluster_chain(new_cluster);
            return false;
        }
    }

    // Write '.' and '..' entries into first sector.
    uint8_t sec[SECTOR_SIZE];
    memset(sec, 0, sizeof(sec));
    uint8_t dot[32];
    uint8_t dotdot[32];
    memset(dot, 0, sizeof(dot));
    memset(dotdot, 0, sizeof(dotdot));

    for (int i = 0; i < 11; i++) {
        dot[i] = ' ';
        dotdot[i] = ' ';
    }
    dot[0] = '.';
    dotdot[0] = '.';
    dotdot[1] = '.';
    dot[11] = FAT_ATTR_DIR;
    dotdot[11] = FAT_ATTR_DIR;
    dot[26] = (uint8_t)(new_cluster & 0xFFu);
    dot[27] = (uint8_t)((new_cluster >> 8) & 0xFFu);

    uint16_t parent_cluster = parent.is_root ? 0u : parent.cluster;
    dotdot[26] = (uint8_t)(parent_cluster & 0xFFu);
    dotdot[27] = (uint8_t)((parent_cluster >> 8) & 0xFFu);

    uint16_t wtime = 0;
    uint16_t wdate = 0;
    fat_timestamp_now(&wtime, &wdate);
    fat_stamp_dirent(dot, wtime, wdate, true);
    fat_stamp_dirent(dotdot, wtime, wdate, true);

    memcpy(sec + 0, dot, 32);
    memcpy(sec + 32, dotdot, 32);
    if (!disk_write(base, sec)) {
        (void)free_cluster_chain(new_cluster);
        return false;
    }

    dir_loc_t slot;
    if (!dir_find_free_slot(parent, &slot)) {
        (void)free_cluster_chain(new_cluster);
        return false;
    }

    uint8_t e[32];
    memset(e, 0, sizeof(e));
    memcpy(e + 0, name11, 11);
    e[11] = FAT_ATTR_DIR;
    fat_stamp_dirent(e, wtime, wdate, true);
    e[26] = (uint8_t)(new_cluster & 0xFFu);
    e[27] = (uint8_t)((new_cluster >> 8) & 0xFFu);
    if (!write_dir_entry_at(slot, e)) {
        (void)free_cluster_chain(new_cluster);
        return false;
    }

    (void)ata_flush();
    return true;
}

bool fatdisk_rename(const char* abs_old, const char* abs_new) {
    if (!g_fs.ready || !abs_old || !abs_new) {
        return false;
    }

    fat_dir_t old_parent;
    uint8_t old_name[11];
    if (!resolve_parent_dir(abs_old, &old_parent, old_name)) {
        return false;
    }

    fat_dir_t new_parent;
    uint8_t new_name[11];
    if (!resolve_parent_dir(abs_new, &new_parent, new_name)) {
        return false;
    }

    if (old_parent.is_root != new_parent.is_root || old_parent.cluster != new_parent.cluster) {
        return false; // no cross-directory move yet
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!dir_iter_find_by_name(old_parent, old_name, &loc, ent)) {
        return false;
    }

    dir_loc_t exist_loc;
    uint8_t exist_ent[32];
    if (dir_iter_find_by_name(new_parent, new_name, &exist_loc, exist_ent)) {
        return false;
    }

    memcpy(ent + 0, new_name, 11);
    if (!write_dir_entry_at(loc, ent)) {
        return false;
    }

    (void)ata_flush();
    return true;
}

static bool dirent_is_dots(const uint8_t ent[32]) {
    if (!ent) {
        return false;
    }
    if (ent[0] != '.') {
        return false;
    }
    // "." entry: ".          "
    if (ent[1] == ' ') {
        for (int i = 2; i < 11; i++) {
            if (ent[i] != ' ') {
                return false;
            }
        }
        return true;
    }
    // ".." entry: "..         "
    if (ent[1] == '.' && ent[2] == ' ') {
        for (int i = 3; i < 11; i++) {
            if (ent[i] != ' ') {
                return false;
            }
        }
        return true;
    }
    return false;
}

static bool dir_is_empty(uint16_t start_cluster) {
    if (!g_fs.ready) {
        return false;
    }
    if (start_cluster < 2u) {
        return true;
    }

    uint16_t cluster = start_cluster;
    uint8_t sec[SECTOR_SIZE];
    uint32_t max_steps = g_fs.cluster_count + 4u;

    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(g_fs.cluster_count + 2u)) {
            return true;
        }

        uint32_t base = cluster_to_lba(cluster);
        for (uint32_t si = 0; si < g_fs.sectors_per_cluster; si++) {
            uint32_t lba = base + si;
            if (!disk_read(lba, sec)) {
                return false;
            }

            for (uint32_t off = 0; off + 32u <= SECTOR_SIZE; off += 32u) {
                const uint8_t* e = sec + off;
                if (e[0] == 0x00) {
                    return true; // end of directory
                }
                if (e[0] == 0xE5) {
                    continue; // deleted
                }
                if (!dir_entry_is_valid(e)) {
                    continue;
                }
                if (dirent_is_dots(e)) {
                    continue;
                }
                return false;
            }
        }

        uint16_t next = 0;
        if (!fat_get(cluster, &next) || fat_is_eoc(next)) {
            break;
        }
        cluster = next;
    }

    return true;
}

bool fatdisk_unlink(const char* abs_path) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return false;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }
    if (ent[11] & FAT_ATTR_DIR) {
        return false;
    }

    uint16_t start = read_le16(ent + 26);
    if (start >= 2u) {
        if (!free_cluster_chain(start)) {
            return false;
        }
    }

    ent[0] = 0xE5; // deleted
    if (!write_dir_entry_at(loc, ent)) {
        return false;
    }

    (void)ata_flush();
    return true;
}

bool fatdisk_rmdir(const char* abs_path) {
    if (!g_fs.ready || !abs_path) {
        return false;
    }
    if (is_root_path(abs_path)) {
        return false;
    }

    dir_loc_t loc;
    uint8_t ent[32];
    if (!lookup_path_entry(abs_path, &loc, ent)) {
        return false;
    }
    if ((ent[11] & FAT_ATTR_DIR) == 0) {
        return false;
    }

    uint16_t start = read_le16(ent + 26);
    if (!dir_is_empty(start)) {
        return false;
    }

    if (start >= 2u) {
        if (!free_cluster_chain(start)) {
            return false;
        }
    }

    ent[0] = 0xE5; // deleted
    if (!write_dir_entry_at(loc, ent)) {
        return false;
    }

    (void)ata_flush();
    return true;
}
