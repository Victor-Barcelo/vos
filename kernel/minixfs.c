// Minix filesystem driver
#include "minixfs.h"
#include "ata.h"
#include "kheap.h"
#include "screen.h"
#include "string.h"
#include "rtc.h"

// Get current Unix timestamp (seconds since 1970-01-01)
static uint32_t get_current_time(void) {
    rtc_datetime_t dt;
    if (!rtc_read_datetime(&dt)) {
        return 0;
    }

    // Calculate days since epoch
    int32_t days = 0;
    for (uint16_t y = 1970; y < dt.year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    static const int32_t mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    days += mdays[dt.month - 1];
    if (dt.month > 2 && (dt.year % 4 == 0 && (dt.year % 100 != 0 || dt.year % 400 == 0))) {
        days++;
    }
    days += dt.day - 1;

    return (uint32_t)(days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second);
}

// Internal filesystem state
typedef struct {
    bool mounted;
    uint32_t partition_lba;     // Start of partition
    uint16_t version;           // 1 or 2
    uint16_t name_len;          // 14 or 30

    // Superblock info
    uint16_t ninodes;
    uint32_t nzones;
    uint16_t imap_blocks;
    uint16_t zmap_blocks;
    uint16_t firstdatazone;
    uint16_t log_zone_size;
    uint32_t max_size;

    // Computed values
    uint32_t inode_table_block; // First block of inode table
    uint16_t inodes_per_block;
    uint16_t dirents_per_block;
    uint16_t dirent_size;
} minixfs_t;

static minixfs_t g_fs;

// Cached blocks for bitmaps
static uint8_t* g_imap = NULL;
static uint8_t* g_zmap = NULL;

// Read a block from the partition
static bool read_block(uint32_t block, void* buf) {
    // Each block is 1024 bytes = 2 sectors
    uint32_t lba = g_fs.partition_lba + block * 2;
    if (!ata_read_sector(lba, buf)) return false;
    if (!ata_read_sector(lba + 1, (uint8_t*)buf + 512)) return false;
    return true;
}

// Write a block to the partition
static bool write_block(uint32_t block, const void* buf) {
    uint32_t lba = g_fs.partition_lba + block * 2;
    if (!ata_write_sector(lba, buf)) return false;
    if (!ata_write_sector(lba + 1, (const uint8_t*)buf + 512)) return false;
    return true;
}

// Get inode from inode number
static bool read_inode_v1(uint32_t ino, minix_inode_v1_t* out) {
    if (ino < 1 || ino > g_fs.ninodes) return false;

    uint32_t inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v1_t);
    uint32_t block = g_fs.inode_table_block + (ino - 1) / inodes_per_block;
    uint32_t offset = ((ino - 1) % inodes_per_block) * sizeof(minix_inode_v1_t);

    uint8_t buf[MINIX_BLOCK_SIZE];
    if (!read_block(block, buf)) return false;

    memcpy(out, buf + offset, sizeof(minix_inode_v1_t));
    return true;
}

static bool read_inode_v2(uint32_t ino, minix_inode_v2_t* out) {
    if (ino < 1 || ino > g_fs.ninodes) return false;

    uint32_t inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v2_t);
    uint32_t block = g_fs.inode_table_block + (ino - 1) / inodes_per_block;
    uint32_t offset = ((ino - 1) % inodes_per_block) * sizeof(minix_inode_v2_t);

    uint8_t buf[MINIX_BLOCK_SIZE];
    if (!read_block(block, buf)) return false;

    memcpy(out, buf + offset, sizeof(minix_inode_v2_t));
    return true;
}

static bool write_inode_v1(uint32_t ino, const minix_inode_v1_t* inode) {
    if (ino < 1 || ino > g_fs.ninodes) return false;

    uint32_t inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v1_t);
    uint32_t block = g_fs.inode_table_block + (ino - 1) / inodes_per_block;
    uint32_t offset = ((ino - 1) % inodes_per_block) * sizeof(minix_inode_v1_t);

    uint8_t buf[MINIX_BLOCK_SIZE];
    if (!read_block(block, buf)) return false;

    memcpy(buf + offset, inode, sizeof(minix_inode_v1_t));
    return write_block(block, buf);
}

static bool write_inode_v2(uint32_t ino, const minix_inode_v2_t* inode) {
    if (ino < 1 || ino > g_fs.ninodes) return false;

    uint32_t inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v2_t);
    uint32_t block = g_fs.inode_table_block + (ino - 1) / inodes_per_block;
    uint32_t offset = ((ino - 1) % inodes_per_block) * sizeof(minix_inode_v2_t);

    uint8_t buf[MINIX_BLOCK_SIZE];
    if (!read_block(block, buf)) return false;

    memcpy(buf + offset, inode, sizeof(minix_inode_v2_t));
    return write_block(block, buf);
}

// Get zone (block) number for a given file position
static uint32_t get_zone_v1(const minix_inode_v1_t* inode, uint32_t zone_idx) {
    if (zone_idx < 7) {
        return inode->i_zone[zone_idx];
    }

    zone_idx -= 7;
    uint32_t ptrs_per_block = MINIX_BLOCK_SIZE / sizeof(uint16_t);

    if (zone_idx < ptrs_per_block) {
        // Indirect block
        if (inode->i_zone[7] == 0) return 0;
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (!read_block(inode->i_zone[7], buf)) return 0;
        return ((uint16_t*)buf)[zone_idx];
    }

    zone_idx -= ptrs_per_block;
    if (zone_idx < ptrs_per_block * ptrs_per_block) {
        // Double indirect block
        if (inode->i_zone[8] == 0) return 0;
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (!read_block(inode->i_zone[8], buf)) return 0;
        uint32_t indirect_block = ((uint16_t*)buf)[zone_idx / ptrs_per_block];
        if (indirect_block == 0) return 0;
        if (!read_block(indirect_block, buf)) return 0;
        return ((uint16_t*)buf)[zone_idx % ptrs_per_block];
    }

    return 0; // Beyond file size
}

static uint32_t get_zone_v2(const minix_inode_v2_t* inode, uint32_t zone_idx) {
    if (zone_idx < 7) {
        return inode->i_zone[zone_idx];
    }

    zone_idx -= 7;
    uint32_t ptrs_per_block = MINIX_BLOCK_SIZE / sizeof(uint32_t);

    if (zone_idx < ptrs_per_block) {
        // Indirect block
        if (inode->i_zone[7] == 0) return 0;
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (!read_block(inode->i_zone[7], buf)) return 0;
        return ((uint32_t*)buf)[zone_idx];
    }

    zone_idx -= ptrs_per_block;
    if (zone_idx < ptrs_per_block * ptrs_per_block) {
        // Double indirect block
        if (inode->i_zone[8] == 0) return 0;
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (!read_block(inode->i_zone[8], buf)) return 0;
        uint32_t indirect_block = ((uint32_t*)buf)[zone_idx / ptrs_per_block];
        if (indirect_block == 0) return 0;
        if (!read_block(indirect_block, buf)) return 0;
        return ((uint32_t*)buf)[zone_idx % ptrs_per_block];
    }

    // Triple indirect (i_zone[9]) - rarely needed
    return 0;
}

// Lookup inode by path
static uint32_t lookup_path(const char* path) {
    if (!path || !g_fs.mounted) return 0;

    // Skip leading slash
    while (*path == '/') path++;

    // Empty path = root
    if (*path == '\0') return MINIX_ROOT_INO;

    uint32_t current_ino = MINIX_ROOT_INO;
    char component[32];

    while (*path) {
        // Extract next path component
        const char* end = path;
        while (*end && *end != '/') end++;

        uint32_t len = (uint32_t)(end - path);
        if (len == 0) {
            path = end + 1;
            continue;
        }
        if (len > g_fs.name_len) return 0; // Name too long
        if (len > sizeof(component) - 1) len = sizeof(component) - 1; // Bounds check

        memcpy(component, path, len);
        component[len] = '\0';

        // Read current directory inode
        uint8_t inode_buf[64];
        uint32_t dir_size;
        uint16_t dir_mode;

        if (g_fs.version == 1) {
            minix_inode_v1_t* inode = (minix_inode_v1_t*)inode_buf;
            if (!read_inode_v1(current_ino, inode)) return 0;
            if (!MINIX_S_ISDIR(inode->i_mode)) return 0;
            dir_size = inode->i_size;
            dir_mode = inode->i_mode;
        } else {
            minix_inode_v2_t* inode = (minix_inode_v2_t*)inode_buf;
            if (!read_inode_v2(current_ino, inode)) return 0;
            if (!MINIX_S_ISDIR(inode->i_mode)) return 0;
            dir_size = inode->i_size;
            dir_mode = inode->i_mode;
        }
        (void)dir_mode;

        // Search directory for component
        bool found = false;
        uint32_t offset = 0;
        uint8_t block_buf[MINIX_BLOCK_SIZE];

        while (offset < dir_size && !found) {
            uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
            uint32_t zone;

            if (g_fs.version == 1) {
                zone = get_zone_v1((minix_inode_v1_t*)inode_buf, zone_idx);
            } else {
                zone = get_zone_v2((minix_inode_v2_t*)inode_buf, zone_idx);
            }

            if (zone == 0) break;
            if (!read_block(zone, block_buf)) return 0;

            uint32_t block_offset = offset % MINIX_BLOCK_SIZE;
            while (block_offset < MINIX_BLOCK_SIZE && offset < dir_size) {
                uint16_t entry_ino;
                char entry_name[32];

                if (g_fs.name_len == 14) {
                    minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)(block_buf + block_offset);
                    entry_ino = de->inode;
                    memcpy(entry_name, de->name, 14);
                    entry_name[14] = '\0';
                    block_offset += sizeof(minix_dir_entry_14_t);
                    offset += sizeof(minix_dir_entry_14_t);
                } else {
                    minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)(block_buf + block_offset);
                    entry_ino = de->inode;
                    memcpy(entry_name, de->name, 30);
                    entry_name[30] = '\0';
                    block_offset += sizeof(minix_dir_entry_30_t);
                    offset += sizeof(minix_dir_entry_30_t);
                }

                if (entry_ino != 0 && strcmp(entry_name, component) == 0) {
                    current_ino = entry_ino;
                    found = true;
                    break;
                }
            }
        }

        if (!found) return 0; // Component not found

        path = end;
        while (*path == '/') path++;
    }

    return current_ino;
}

// Bitmap operations
static bool inode_is_used(uint32_t ino) {
    if (!g_imap || ino < 1 || ino > g_fs.ninodes) return true;
    uint32_t byte = (ino - 1) / 8;
    uint32_t bit = (ino - 1) % 8;
    return (g_imap[byte] & (1 << bit)) != 0;
}

static void inode_set_used(uint32_t ino, bool used) {
    if (!g_imap || ino < 1 || ino > g_fs.ninodes) return;
    uint32_t byte = (ino - 1) / 8;
    uint32_t bit = (ino - 1) % 8;
    if (used) {
        g_imap[byte] |= (1 << bit);
    } else {
        g_imap[byte] &= ~(1 << bit);
    }
}

static bool zone_is_used(uint32_t zone) {
    if (!g_zmap || zone < g_fs.firstdatazone || zone >= g_fs.nzones) return true;
    uint32_t idx = zone - g_fs.firstdatazone;
    uint32_t byte = idx / 8;
    uint32_t bit = idx % 8;
    return (g_zmap[byte] & (1 << bit)) != 0;
}

static void zone_set_used(uint32_t zone, bool used) {
    if (!g_zmap || zone < g_fs.firstdatazone || zone >= g_fs.nzones) return;
    uint32_t idx = zone - g_fs.firstdatazone;
    uint32_t byte = idx / 8;
    uint32_t bit = idx % 8;
    if (used) {
        g_zmap[byte] |= (1 << bit);
    } else {
        g_zmap[byte] &= ~(1 << bit);
    }
}

static uint32_t alloc_inode(void) {
    for (uint32_t i = 1; i <= g_fs.ninodes; i++) {
        if (!inode_is_used(i)) {
            inode_set_used(i, true);
            return i;
        }
    }
    return 0;
}

static uint32_t alloc_zone(void) {
    for (uint32_t z = g_fs.firstdatazone; z < g_fs.nzones; z++) {
        if (!zone_is_used(z)) {
            zone_set_used(z, true);
            return z;
        }
    }
    return 0;
}

static void free_zone(uint32_t zone) {
    zone_set_used(zone, false);
}

static void free_inode(uint32_t ino) {
    inode_set_used(ino, false);
}

// Write bitmaps to disk
static bool write_bitmaps(void) {
    // Write inode bitmap
    for (uint16_t i = 0; i < g_fs.imap_blocks; i++) {
        if (!write_block(2 + i, g_imap + i * MINIX_BLOCK_SIZE)) return false;
    }
    // Write zone bitmap
    for (uint16_t i = 0; i < g_fs.zmap_blocks; i++) {
        if (!write_block(2 + g_fs.imap_blocks + i, g_zmap + i * MINIX_BLOCK_SIZE)) return false;
    }
    return true;
}

bool minixfs_init(uint32_t partition_lba_start) {
    memset(&g_fs, 0, sizeof(g_fs));

    if (g_imap) { kfree(g_imap); g_imap = NULL; }
    if (g_zmap) { kfree(g_zmap); g_zmap = NULL; }

    g_fs.partition_lba = partition_lba_start;

    // Read superblock (block 1)
    uint8_t sb_buf[MINIX_BLOCK_SIZE];
    if (!read_block(1, sb_buf)) {
        return false;
    }

    // Check magic and determine version
    minix_super_block_v1_t* sb1 = (minix_super_block_v1_t*)sb_buf;

    switch (sb1->s_magic) {
        case MINIX_SUPER_MAGIC:
            g_fs.version = 1;
            g_fs.name_len = 14;
            break;
        case MINIX_SUPER_MAGIC2:
            g_fs.version = 1;
            g_fs.name_len = 30;
            break;
        case MINIX2_SUPER_MAGIC:
            g_fs.version = 2;
            g_fs.name_len = 14;
            break;
        case MINIX2_SUPER_MAGIC2:
            g_fs.version = 2;
            g_fs.name_len = 30;
            break;
        default:
            return false;
    }

    // Copy superblock info
    g_fs.ninodes = sb1->s_ninodes;
    g_fs.imap_blocks = sb1->s_imap_blocks;
    g_fs.zmap_blocks = sb1->s_zmap_blocks;
    g_fs.firstdatazone = sb1->s_firstdatazone;
    g_fs.log_zone_size = sb1->s_log_zone_size;
    g_fs.max_size = sb1->s_max_size;

    if (g_fs.version == 2) {
        minix_super_block_v2_t* sb2 = (minix_super_block_v2_t*)sb_buf;
        g_fs.nzones = sb2->s_zones ? sb2->s_zones : sb2->s_nzones;
    } else {
        g_fs.nzones = sb1->s_nzones;
    }

    // Calculate inode table location
    // Layout: boot(0), super(1), imap, zmap, inodes, data
    g_fs.inode_table_block = 2 + g_fs.imap_blocks + g_fs.zmap_blocks;

    if (g_fs.version == 1) {
        g_fs.inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v1_t);
        g_fs.dirent_size = (g_fs.name_len == 14) ? sizeof(minix_dir_entry_14_t) : sizeof(minix_dir_entry_30_t);
    } else {
        g_fs.inodes_per_block = MINIX_BLOCK_SIZE / sizeof(minix_inode_v2_t);
        g_fs.dirent_size = (g_fs.name_len == 14) ? sizeof(minix_dir_entry_14_t) : sizeof(minix_dir_entry_30_t);
    }
    g_fs.dirents_per_block = MINIX_BLOCK_SIZE / g_fs.dirent_size;

    // Load bitmaps
    uint32_t imap_size = (uint32_t)g_fs.imap_blocks * MINIX_BLOCK_SIZE;
    uint32_t zmap_size = (uint32_t)g_fs.zmap_blocks * MINIX_BLOCK_SIZE;

    g_imap = (uint8_t*)kmalloc(imap_size);
    g_zmap = (uint8_t*)kmalloc(zmap_size);

    if (!g_imap || !g_zmap) {
        if (g_imap) kfree(g_imap);
        if (g_zmap) kfree(g_zmap);
        g_imap = g_zmap = NULL;
        return false;
    }

    // Read inode bitmap
    for (uint16_t i = 0; i < g_fs.imap_blocks; i++) {
        if (!read_block(2 + i, g_imap + i * MINIX_BLOCK_SIZE)) {
            kfree(g_imap); kfree(g_zmap);
            g_imap = g_zmap = NULL;
            return false;
        }
    }

    // Read zone bitmap
    for (uint16_t i = 0; i < g_fs.zmap_blocks; i++) {
        if (!read_block(2 + g_fs.imap_blocks + i, g_zmap + i * MINIX_BLOCK_SIZE)) {
            kfree(g_imap); kfree(g_zmap);
            g_imap = g_zmap = NULL;
            return false;
        }
    }

    g_fs.mounted = true;

    screen_print("[MINIXFS] Mounted v");
    screen_print_dec(g_fs.version);
    screen_print(" (");
    screen_print_dec(g_fs.name_len);
    screen_print("-char names), ");
    screen_print_dec((int32_t)g_fs.ninodes);
    screen_print(" inodes, ");
    screen_print_dec((int32_t)g_fs.nzones);
    screen_println(" zones");

    return true;
}

bool minixfs_is_ready(void) {
    return g_fs.mounted;
}

bool minixfs_statfs(uint32_t* total_blocks, uint32_t* free_blocks, uint32_t* total_inodes, uint32_t* free_inodes) {
    if (!g_fs.mounted) return false;

    if (total_blocks) *total_blocks = g_fs.nzones - g_fs.firstdatazone;
    if (total_inodes) *total_inodes = g_fs.ninodes;

    if (free_blocks) {
        uint32_t free = 0;
        for (uint32_t z = g_fs.firstdatazone; z < g_fs.nzones; z++) {
            if (!zone_is_used(z)) free++;
        }
        *free_blocks = free;
    }

    if (free_inodes) {
        uint32_t free = 0;
        for (uint32_t i = 1; i <= g_fs.ninodes; i++) {
            if (!inode_is_used(i)) free++;
        }
        *free_inodes = free;
    }

    return true;
}

bool minixfs_stat(const char* path, minixfs_stat_t* out) {
    if (!g_fs.mounted || !path || !out) return false;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return false;

    memset(out, 0, sizeof(*out));
    out->ino = ino;

    if (g_fs.version == 1) {
        minix_inode_v1_t inode;
        if (!read_inode_v1(ino, &inode)) return false;
        out->mode = inode.i_mode;
        out->uid = inode.i_uid;
        out->gid = inode.i_gid;
        out->size = inode.i_size;
        out->mtime = inode.i_time;
        out->nlinks = inode.i_nlinks;
    } else {
        minix_inode_v2_t inode;
        if (!read_inode_v2(ino, &inode)) return false;
        out->mode = inode.i_mode;
        out->uid = inode.i_uid;
        out->gid = inode.i_gid;
        out->size = inode.i_size;
        out->mtime = inode.i_mtime;
        out->nlinks = inode.i_nlinks;
    }

    return true;
}

bool minixfs_is_dir(const char* path) {
    minixfs_stat_t st;
    if (!minixfs_stat(path, &st)) return false;
    return MINIX_S_ISDIR(st.mode);
}

bool minixfs_is_file(const char* path) {
    minixfs_stat_t st;
    if (!minixfs_stat(path, &st)) return false;
    return MINIX_S_ISREG(st.mode);
}

uint8_t* minixfs_read_file(const char* path, uint32_t* out_size) {
    if (!g_fs.mounted || !path || !out_size) return NULL;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return NULL;

    uint8_t inode_buf[64];
    uint32_t file_size;
    uint16_t mode;

    if (g_fs.version == 1) {
        minix_inode_v1_t* inode = (minix_inode_v1_t*)inode_buf;
        if (!read_inode_v1(ino, inode)) return NULL;
        if (!MINIX_S_ISREG(inode->i_mode) && !MINIX_S_ISLNK(inode->i_mode)) return NULL;
        file_size = inode->i_size;
        mode = inode->i_mode;
    } else {
        minix_inode_v2_t* inode = (minix_inode_v2_t*)inode_buf;
        if (!read_inode_v2(ino, inode)) return NULL;
        if (!MINIX_S_ISREG(inode->i_mode) && !MINIX_S_ISLNK(inode->i_mode)) return NULL;
        file_size = inode->i_size;
        mode = inode->i_mode;
    }
    (void)mode;

    if (file_size == 0) {
        *out_size = 0;
        return (uint8_t*)kmalloc(1); // Return valid pointer for empty file
    }

    uint8_t* data = (uint8_t*)kmalloc(file_size);
    if (!data) return NULL;

    uint32_t offset = 0;
    while (offset < file_size) {
        uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
        uint32_t zone;

        if (g_fs.version == 1) {
            zone = get_zone_v1((minix_inode_v1_t*)inode_buf, zone_idx);
        } else {
            zone = get_zone_v2((minix_inode_v2_t*)inode_buf, zone_idx);
        }

        if (zone == 0) {
            // Sparse file - fill with zeros
            uint32_t to_copy = MINIX_BLOCK_SIZE;
            if (offset + to_copy > file_size) to_copy = file_size - offset;
            memset(data + offset, 0, to_copy);
            offset += to_copy;
            continue;
        }

        uint8_t block_buf[MINIX_BLOCK_SIZE];
        if (!read_block(zone, block_buf)) {
            kfree(data);
            return NULL;
        }

        uint32_t to_copy = MINIX_BLOCK_SIZE;
        if (offset + to_copy > file_size) to_copy = file_size - offset;
        memcpy(data + offset, block_buf, to_copy);
        offset += to_copy;
    }

    *out_size = file_size;
    return data;
}

uint32_t minixfs_readdir(const char* path, minixfs_dirent_t* out, uint32_t max_entries) {
    if (!g_fs.mounted || !path || !out || max_entries == 0) return 0;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return 0;

    uint8_t inode_buf[64];
    uint32_t dir_size;

    if (g_fs.version == 1) {
        minix_inode_v1_t* inode = (minix_inode_v1_t*)inode_buf;
        if (!read_inode_v1(ino, inode)) return 0;
        if (!MINIX_S_ISDIR(inode->i_mode)) return 0;
        dir_size = inode->i_size;
    } else {
        minix_inode_v2_t* inode = (minix_inode_v2_t*)inode_buf;
        if (!read_inode_v2(ino, inode)) return 0;
        if (!MINIX_S_ISDIR(inode->i_mode)) return 0;
        dir_size = inode->i_size;
    }

    uint32_t count = 0;
    uint32_t offset = 0;
    uint8_t block_buf[MINIX_BLOCK_SIZE];

    while (offset < dir_size && count < max_entries) {
        uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
        uint32_t zone;

        if (g_fs.version == 1) {
            zone = get_zone_v1((minix_inode_v1_t*)inode_buf, zone_idx);
        } else {
            zone = get_zone_v2((minix_inode_v2_t*)inode_buf, zone_idx);
        }

        if (zone == 0) break;
        if (!read_block(zone, block_buf)) break;

        uint32_t block_offset = offset % MINIX_BLOCK_SIZE;
        while (block_offset < MINIX_BLOCK_SIZE && offset < dir_size && count < max_entries) {
            uint16_t entry_ino;
            char entry_name[32];

            if (g_fs.name_len == 14) {
                minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 14);
                entry_name[14] = '\0';
                block_offset += sizeof(minix_dir_entry_14_t);
                offset += sizeof(minix_dir_entry_14_t);
            } else {
                minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 30);
                entry_name[30] = '\0';
                block_offset += sizeof(minix_dir_entry_30_t);
                offset += sizeof(minix_dir_entry_30_t);
            }

            if (entry_ino != 0) {
                out[count].inode = entry_ino;
                strncpy(out[count].name, entry_name, 30);
                out[count].name[30] = '\0';

                // Check if it's a directory
                if (g_fs.version == 1) {
                    minix_inode_v1_t entry_inode;
                    out[count].is_dir = read_inode_v1(entry_ino, &entry_inode) &&
                                        MINIX_S_ISDIR(entry_inode.i_mode);
                } else {
                    minix_inode_v2_t entry_inode;
                    out[count].is_dir = read_inode_v2(entry_ino, &entry_inode) &&
                                        MINIX_S_ISDIR(entry_inode.i_mode);
                }
                count++;
            }
        }
    }

    return count;
}

bool minixfs_readlink(const char* path, char* buf, uint32_t bufsize) {
    if (!g_fs.mounted || !path || !buf || bufsize == 0) return false;

    minixfs_stat_t st;
    if (!minixfs_stat(path, &st)) return false;
    if (!MINIX_S_ISLNK(st.mode)) return false;

    uint32_t size;
    uint8_t* data = minixfs_read_file(path, &size);
    if (!data) return false;

    uint32_t to_copy = size;
    if (to_copy >= bufsize) to_copy = bufsize - 1;
    memcpy(buf, data, to_copy);
    buf[to_copy] = '\0';

    kfree(data);
    return true;
}

bool minixfs_chmod(const char* path, uint16_t mode) {
    if (!g_fs.mounted || !path) return false;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return false;

    if (g_fs.version == 1) {
        minix_inode_v1_t inode;
        if (!read_inode_v1(ino, &inode)) return false;
        inode.i_mode = (inode.i_mode & MINIX_S_IFMT) | (mode & 07777);
        inode.i_time = get_current_time();
        return write_inode_v1(ino, &inode);
    } else {
        minix_inode_v2_t inode;
        if (!read_inode_v2(ino, &inode)) return false;
        inode.i_mode = (inode.i_mode & MINIX_S_IFMT) | (mode & 07777);
        inode.i_ctime = get_current_time();
        return write_inode_v2(ino, &inode);
    }
}

bool minixfs_chown(const char* path, uint16_t uid, uint16_t gid) {
    if (!g_fs.mounted || !path) return false;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return false;

    if (g_fs.version == 1) {
        minix_inode_v1_t inode;
        if (!read_inode_v1(ino, &inode)) return false;
        inode.i_uid = uid;
        inode.i_gid = (uint8_t)gid;
        inode.i_time = get_current_time();
        return write_inode_v1(ino, &inode);
    } else {
        minix_inode_v2_t inode;
        if (!read_inode_v2(ino, &inode)) return false;
        inode.i_uid = uid;
        inode.i_gid = gid;
        inode.i_ctime = get_current_time();
        return write_inode_v2(ino, &inode);
    }
}

void minixfs_sync(void) {
    if (!g_fs.mounted) return;
    write_bitmaps();
    ata_flush();
}

// Helper: set zone pointer for v2 inode, allocating indirect blocks as needed
static bool set_zone_v2(minix_inode_v2_t* inode, uint32_t zone_idx, uint32_t zone_num) {
    if (zone_idx < 7) {
        inode->i_zone[zone_idx] = zone_num;
        return true;
    }

    zone_idx -= 7;
    uint32_t ptrs_per_block = MINIX_BLOCK_SIZE / sizeof(uint32_t);

    if (zone_idx < ptrs_per_block) {
        // Indirect block
        if (inode->i_zone[7] == 0) {
            uint32_t indirect = alloc_zone();
            if (indirect == 0) return false;
            inode->i_zone[7] = indirect;
            uint8_t zero[MINIX_BLOCK_SIZE];
            memset(zero, 0, MINIX_BLOCK_SIZE);
            if (!write_block(indirect, zero)) return false;
        }
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (!read_block(inode->i_zone[7], buf)) return false;
        ((uint32_t*)buf)[zone_idx] = zone_num;
        return write_block(inode->i_zone[7], buf);
    }

    // Double/triple indirect not implemented yet
    return false;
}

// Helper: find parent directory and base name from path
static bool split_path(const char* path, uint32_t* parent_ino, char* base_name, uint32_t base_max) {
    if (!path || path[0] == '\0') return false;

    // Find last slash
    const char* last_slash = NULL;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    if (!last_slash || last_slash == path) {
        // File in root directory
        *parent_ino = MINIX_ROOT_INO;
        while (*path == '/') path++;
        strncpy(base_name, path, base_max - 1);
        base_name[base_max - 1] = '\0';
    } else {
        // Get parent path
        char parent_path[512];
        size_t plen = (size_t)(last_slash - path);
        if (plen >= sizeof(parent_path)) return false;
        memcpy(parent_path, path, plen);
        parent_path[plen] = '\0';

        *parent_ino = lookup_path(parent_path);
        if (*parent_ino == 0) return false;

        strncpy(base_name, last_slash + 1, base_max - 1);
        base_name[base_max - 1] = '\0';
    }

    return base_name[0] != '\0';
}

// Helper: add directory entry to a directory
static bool add_dir_entry(uint32_t dir_ino, const char* name, uint32_t entry_ino) {
    if (g_fs.version != 2) return false; // Only v2 supported for writes

    minix_inode_v2_t dir_inode;
    if (!read_inode_v2(dir_ino, &dir_inode)) return false;
    if (!MINIX_S_ISDIR(dir_inode.i_mode)) return false;

    uint32_t entry_size = g_fs.dirent_size;
    uint32_t dir_size = dir_inode.i_size;

    // Search for empty slot or end of directory
    uint8_t block_buf[MINIX_BLOCK_SIZE];
    uint32_t offset = 0;

    while (offset < dir_size) {
        uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
        uint32_t zone = get_zone_v2(&dir_inode, zone_idx);
        if (zone == 0) break;
        if (!read_block(zone, block_buf)) return false;

        uint32_t block_offset = offset % MINIX_BLOCK_SIZE;
        while (block_offset + entry_size <= MINIX_BLOCK_SIZE && offset < dir_size) {
            if (g_fs.name_len == 30) {
                minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)(block_buf + block_offset);
                if (de->inode == 0) {
                    // Empty slot found
                    de->inode = (uint16_t)entry_ino;
                    memset(de->name, 0, 30);
                    strncpy(de->name, name, 30);
                    return write_block(zone, block_buf);
                }
            } else {
                minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)(block_buf + block_offset);
                if (de->inode == 0) {
                    de->inode = (uint16_t)entry_ino;
                    memset(de->name, 0, 14);
                    strncpy(de->name, name, 14);
                    return write_block(zone, block_buf);
                }
            }
            block_offset += entry_size;
            offset += entry_size;
        }
    }

    // Need to extend directory - allocate new zone
    uint32_t zone_idx = dir_size / MINIX_BLOCK_SIZE;
    uint32_t new_zone = alloc_zone();
    if (new_zone == 0) return false;

    if (!set_zone_v2(&dir_inode, zone_idx, new_zone)) {
        free_zone(new_zone);
        return false;
    }

    memset(block_buf, 0, MINIX_BLOCK_SIZE);
    if (g_fs.name_len == 30) {
        minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)block_buf;
        de->inode = (uint16_t)entry_ino;
        strncpy(de->name, name, 30);
    } else {
        minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)block_buf;
        de->inode = (uint16_t)entry_ino;
        strncpy(de->name, name, 14);
    }

    if (!write_block(new_zone, block_buf)) return false;

    dir_inode.i_size = dir_size + MINIX_BLOCK_SIZE;
    dir_inode.i_nlinks = dir_inode.i_nlinks; // Keep link count
    return write_inode_v2(dir_ino, &dir_inode);
}

// Helper: remove directory entry
static bool remove_dir_entry(uint32_t dir_ino, const char* name) {
    if (g_fs.version != 2) return false;

    minix_inode_v2_t dir_inode;
    if (!read_inode_v2(dir_ino, &dir_inode)) return false;
    if (!MINIX_S_ISDIR(dir_inode.i_mode)) return false;

    uint32_t entry_size = g_fs.dirent_size;
    uint32_t dir_size = dir_inode.i_size;
    uint8_t block_buf[MINIX_BLOCK_SIZE];
    uint32_t offset = 0;

    while (offset < dir_size) {
        uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
        uint32_t zone = get_zone_v2(&dir_inode, zone_idx);
        if (zone == 0) break;
        if (!read_block(zone, block_buf)) return false;

        uint32_t block_offset = offset % MINIX_BLOCK_SIZE;
        while (block_offset + entry_size <= MINIX_BLOCK_SIZE && offset < dir_size) {
            char entry_name[32];
            uint16_t entry_ino;

            if (g_fs.name_len == 30) {
                minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 30);
                entry_name[30] = '\0';
                if (entry_ino != 0 && strcmp(entry_name, name) == 0) {
                    de->inode = 0;
                    return write_block(zone, block_buf);
                }
            } else {
                minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 14);
                entry_name[14] = '\0';
                if (entry_ino != 0 && strcmp(entry_name, name) == 0) {
                    de->inode = 0;
                    return write_block(zone, block_buf);
                }
            }
            block_offset += entry_size;
            offset += entry_size;
        }
    }
    return false;
}

// Helper: free all zones used by an inode
static void free_inode_zones_v2(minix_inode_v2_t* inode) {
    uint32_t num_zones = (inode->i_size + MINIX_BLOCK_SIZE - 1) / MINIX_BLOCK_SIZE;

    for (uint32_t i = 0; i < 7 && i < num_zones; i++) {
        if (inode->i_zone[i] != 0) {
            free_zone(inode->i_zone[i]);
            inode->i_zone[i] = 0;
        }
    }

    // Free indirect block and its contents
    if (inode->i_zone[7] != 0) {
        uint8_t buf[MINIX_BLOCK_SIZE];
        if (read_block(inode->i_zone[7], buf)) {
            uint32_t ptrs_per_block = MINIX_BLOCK_SIZE / sizeof(uint32_t);
            for (uint32_t i = 0; i < ptrs_per_block; i++) {
                uint32_t z = ((uint32_t*)buf)[i];
                if (z != 0) free_zone(z);
            }
        }
        free_zone(inode->i_zone[7]);
        inode->i_zone[7] = 0;
    }

    // Don't bother with double/triple indirect for now
}

bool minixfs_write_file(const char* path, const uint8_t* data, uint32_t size) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!path || path[0] == '\0') return false;

    uint32_t parent_ino;
    char base_name[32];
    if (!split_path(path, &parent_ino, base_name, sizeof(base_name))) return false;

    uint32_t ino = lookup_path(path);
    minix_inode_v2_t inode;

    uint32_t now = get_current_time();

    if (ino == 0) {
        // Create new file
        ino = alloc_inode();
        if (ino == 0) return false;

        memset(&inode, 0, sizeof(inode));
        inode.i_mode = MINIX_S_IFREG | 0644;
        inode.i_nlinks = 1;
        inode.i_uid = 0;
        inode.i_gid = 0;
        inode.i_size = 0;
        inode.i_atime = now;
        inode.i_mtime = now;
        inode.i_ctime = now;

        if (!add_dir_entry(parent_ino, base_name, ino)) {
            free_inode(ino);
            write_bitmaps();
            return false;
        }
    } else {
        if (!read_inode_v2(ino, &inode)) return false;
        if (!MINIX_S_ISREG(inode.i_mode)) return false;

        // Free old zones
        free_inode_zones_v2(&inode);
        // Update modification time
        inode.i_mtime = now;
        inode.i_ctime = now;
    }

    // Write new data
    inode.i_size = size;
    uint32_t offset = 0;
    uint32_t zone_idx = 0;

    while (offset < size) {
        uint32_t zone = alloc_zone();
        if (zone == 0) {
            // Out of space
            inode.i_size = offset;
            write_inode_v2(ino, &inode);
            write_bitmaps();
            return false;
        }

        if (!set_zone_v2(&inode, zone_idx, zone)) {
            free_zone(zone);
            inode.i_size = offset;
            write_inode_v2(ino, &inode);
            write_bitmaps();
            return false;
        }

        uint8_t block_buf[MINIX_BLOCK_SIZE];
        memset(block_buf, 0, MINIX_BLOCK_SIZE);
        uint32_t to_write = size - offset;
        if (to_write > MINIX_BLOCK_SIZE) to_write = MINIX_BLOCK_SIZE;
        if (data) memcpy(block_buf, data + offset, to_write);

        if (!write_block(zone, block_buf)) {
            inode.i_size = offset;
            write_inode_v2(ino, &inode);
            write_bitmaps();
            return false;
        }

        offset += MINIX_BLOCK_SIZE;
        zone_idx++;
    }

    if (!write_inode_v2(ino, &inode)) return false;
    write_bitmaps();
    return true;
}

bool minixfs_mkdir(const char* path) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!path || path[0] == '\0') return false;

    uint32_t parent_ino;
    char base_name[32];
    if (!split_path(path, &parent_ino, base_name, sizeof(base_name))) return false;

    if (lookup_path(path) != 0) return false; // Already exists

    uint32_t ino = alloc_inode();
    if (ino == 0) return false;

    uint32_t zone = alloc_zone();
    if (zone == 0) {
        free_inode(ino);
        return false;
    }

    uint32_t now = get_current_time();

    // Initialize directory inode
    minix_inode_v2_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = MINIX_S_IFDIR | 0755;
    inode.i_nlinks = 2; // . and parent's link
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_size = g_fs.dirent_size * 2; // . and ..
    inode.i_atime = now;
    inode.i_mtime = now;
    inode.i_ctime = now;
    inode.i_zone[0] = zone;

    // Create . and .. entries
    uint8_t block_buf[MINIX_BLOCK_SIZE];
    memset(block_buf, 0, MINIX_BLOCK_SIZE);

    if (g_fs.name_len == 30) {
        minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)block_buf;
        de[0].inode = (uint16_t)ino;
        strcpy(de[0].name, ".");
        de[1].inode = (uint16_t)parent_ino;
        strcpy(de[1].name, "..");
    } else {
        minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)block_buf;
        de[0].inode = (uint16_t)ino;
        strcpy(de[0].name, ".");
        de[1].inode = (uint16_t)parent_ino;
        strcpy(de[1].name, "..");
    }

    if (!write_block(zone, block_buf)) {
        free_zone(zone);
        free_inode(ino);
        return false;
    }

    if (!write_inode_v2(ino, &inode)) {
        free_zone(zone);
        free_inode(ino);
        return false;
    }

    if (!add_dir_entry(parent_ino, base_name, ino)) {
        free_zone(zone);
        free_inode(ino);
        return false;
    }

    // Increment parent's link count
    minix_inode_v2_t parent_inode;
    if (read_inode_v2(parent_ino, &parent_inode)) {
        parent_inode.i_nlinks++;
        write_inode_v2(parent_ino, &parent_inode);
    }

    write_bitmaps();
    return true;
}

bool minixfs_unlink(const char* path) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!path || path[0] == '\0') return false;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return false;

    minix_inode_v2_t inode;
    if (!read_inode_v2(ino, &inode)) return false;
    if (!MINIX_S_ISREG(inode.i_mode) && !MINIX_S_ISLNK(inode.i_mode)) return false;

    uint32_t parent_ino;
    char base_name[32];
    if (!split_path(path, &parent_ino, base_name, sizeof(base_name))) return false;

    if (!remove_dir_entry(parent_ino, base_name)) return false;

    inode.i_nlinks--;
    if (inode.i_nlinks == 0) {
        free_inode_zones_v2(&inode);
        free_inode(ino);
    } else {
        write_inode_v2(ino, &inode);
    }

    write_bitmaps();
    return true;
}

bool minixfs_rmdir(const char* path) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!path || path[0] == '\0') return false;

    uint32_t ino = lookup_path(path);
    if (ino == 0) return false;
    if (ino == MINIX_ROOT_INO) return false; // Can't remove root

    minix_inode_v2_t inode;
    if (!read_inode_v2(ino, &inode)) return false;
    if (!MINIX_S_ISDIR(inode.i_mode)) return false;

    // Check if directory is empty (only . and ..)
    uint32_t entry_count = 0;
    uint8_t block_buf[MINIX_BLOCK_SIZE];
    uint32_t offset = 0;

    while (offset < inode.i_size) {
        uint32_t zone_idx = offset / MINIX_BLOCK_SIZE;
        uint32_t zone = get_zone_v2(&inode, zone_idx);
        if (zone == 0) break;
        if (!read_block(zone, block_buf)) return false;

        uint32_t block_offset = offset % MINIX_BLOCK_SIZE;
        while (block_offset + g_fs.dirent_size <= MINIX_BLOCK_SIZE && offset < inode.i_size) {
            uint16_t entry_ino;
            char entry_name[32];

            if (g_fs.name_len == 30) {
                minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 30);
                entry_name[30] = '\0';
            } else {
                minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)(block_buf + block_offset);
                entry_ino = de->inode;
                memcpy(entry_name, de->name, 14);
                entry_name[14] = '\0';
            }

            if (entry_ino != 0 && strcmp(entry_name, ".") != 0 && strcmp(entry_name, "..") != 0) {
                entry_count++;
            }
            block_offset += g_fs.dirent_size;
            offset += g_fs.dirent_size;
        }
    }

    if (entry_count > 0) return false; // Directory not empty

    uint32_t parent_ino;
    char base_name[32];
    if (!split_path(path, &parent_ino, base_name, sizeof(base_name))) return false;

    if (!remove_dir_entry(parent_ino, base_name)) return false;

    free_inode_zones_v2(&inode);
    free_inode(ino);

    // Decrement parent's link count
    minix_inode_v2_t parent_inode;
    if (read_inode_v2(parent_ino, &parent_inode)) {
        if (parent_inode.i_nlinks > 0) parent_inode.i_nlinks--;
        write_inode_v2(parent_ino, &parent_inode);
    }

    write_bitmaps();
    return true;
}

bool minixfs_symlink(const char* target, const char* linkpath) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!target || !linkpath || linkpath[0] == '\0') return false;

    uint32_t parent_ino;
    char base_name[32];
    if (!split_path(linkpath, &parent_ino, base_name, sizeof(base_name))) return false;

    if (lookup_path(linkpath) != 0) return false; // Already exists

    uint32_t ino = alloc_inode();
    if (ino == 0) return false;

    uint32_t target_len = (uint32_t)strlen(target);
    uint32_t now = get_current_time();

    minix_inode_v2_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = MINIX_S_IFLNK | 0777;
    inode.i_nlinks = 1;
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_size = target_len;
    inode.i_atime = now;
    inode.i_mtime = now;
    inode.i_ctime = now;

    // Allocate zone for symlink target
    if (target_len > 0) {
        uint32_t zone = alloc_zone();
        if (zone == 0) {
            free_inode(ino);
            return false;
        }
        inode.i_zone[0] = zone;

        uint8_t block_buf[MINIX_BLOCK_SIZE];
        memset(block_buf, 0, MINIX_BLOCK_SIZE);
        memcpy(block_buf, target, target_len);
        if (!write_block(zone, block_buf)) {
            free_zone(zone);
            free_inode(ino);
            return false;
        }
    }

    if (!write_inode_v2(ino, &inode)) {
        if (inode.i_zone[0]) free_zone(inode.i_zone[0]);
        free_inode(ino);
        return false;
    }

    if (!add_dir_entry(parent_ino, base_name, ino)) {
        if (inode.i_zone[0]) free_zone(inode.i_zone[0]);
        free_inode(ino);
        return false;
    }

    write_bitmaps();
    return true;
}

bool minixfs_rename(const char* oldpath, const char* newpath) {
    if (!g_fs.mounted || g_fs.version != 2) return false;
    if (!oldpath || !newpath) return false;

    uint32_t ino = lookup_path(oldpath);
    if (ino == 0) return false;

    uint32_t old_parent_ino, new_parent_ino;
    char old_base[32], new_base[32];
    if (!split_path(oldpath, &old_parent_ino, old_base, sizeof(old_base))) return false;
    if (!split_path(newpath, &new_parent_ino, new_base, sizeof(new_base))) return false;

    // Check if destination exists
    uint32_t dest_ino = lookup_path(newpath);
    if (dest_ino != 0) {
        // Remove destination first
        minix_inode_v2_t dest_inode;
        if (!read_inode_v2(dest_ino, &dest_inode)) return false;
        if (MINIX_S_ISDIR(dest_inode.i_mode)) return false; // Can't overwrite directory
        if (!remove_dir_entry(new_parent_ino, new_base)) return false;
        free_inode_zones_v2(&dest_inode);
        free_inode(dest_ino);
    }

    if (!remove_dir_entry(old_parent_ino, old_base)) return false;
    if (!add_dir_entry(new_parent_ino, new_base, ino)) {
        // Try to restore old entry
        add_dir_entry(old_parent_ino, old_base, ino);
        return false;
    }

    // If it's a directory and parent changed, update .. entry
    minix_inode_v2_t inode;
    if (read_inode_v2(ino, &inode) && MINIX_S_ISDIR(inode.i_mode) && old_parent_ino != new_parent_ino) {
        // Update .. entry in the moved directory
        uint32_t zone = get_zone_v2(&inode, 0);
        if (zone != 0) {
            uint8_t block_buf[MINIX_BLOCK_SIZE];
            if (read_block(zone, block_buf)) {
                if (g_fs.name_len == 30) {
                    minix_dir_entry_30_t* de = (minix_dir_entry_30_t*)block_buf;
                    de[1].inode = (uint16_t)new_parent_ino;
                } else {
                    minix_dir_entry_14_t* de = (minix_dir_entry_14_t*)block_buf;
                    de[1].inode = (uint16_t)new_parent_ino;
                }
                write_block(zone, block_buf);
            }
        }

        // Update link counts
        minix_inode_v2_t old_parent, new_parent;
        if (read_inode_v2(old_parent_ino, &old_parent)) {
            if (old_parent.i_nlinks > 0) old_parent.i_nlinks--;
            write_inode_v2(old_parent_ino, &old_parent);
        }
        if (read_inode_v2(new_parent_ino, &new_parent)) {
            new_parent.i_nlinks++;
            write_inode_v2(new_parent_ino, &new_parent);
        }
    }

    write_bitmaps();
    return true;
}
