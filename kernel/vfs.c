#include "vfs.h"
#include "kheap.h"
#include "ctype.h"
#include "paging.h"
#include "string.h"
#include "serial.h"
#include "ramfs.h"

typedef struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) tar_header_t;

typedef struct vfs_file {
    char* name;
    const uint8_t* data;
    uint32_t size;
    uint16_t wtime;
    uint16_t wdate;
} vfs_file_t;

static vfs_file_t* files = NULL;
static uint32_t file_count = 0;
static bool ready = false;

// When the initramfs grows large, GRUB/multiboot may place the TAR module above
// USER_BASE. User address spaces do not map that mid-range region, so syscalls
// running on a user CR3 would fault when reading initramfs-backed files.
//
// Map the TAR module into high kernel virtual memory (shared across all CR3s)
// and store initramfs file pointers using that mapping.
static const uint32_t INITRAMFS_TAR_VBASE = 0xC4000000u;
static const uint32_t KHEAP_BASE = 0xD0000000u;

static const uint8_t* map_tar_module_high(const multiboot_module_t* mod, uint32_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!mod || mod->mod_end <= mod->mod_start) {
        return NULL;
    }

    uint32_t len = mod->mod_end - mod->mod_start;

    uint32_t paddr_page = mod->mod_start & ~(PAGE_SIZE - 1u);
    uint32_t off = mod->mod_start - paddr_page;
    uint32_t map_size = len + off;

    // Keep the mapping below the kernel heap region. If the initramfs is huge,
    // fall back to copying into the heap instead of overlapping mappings.
    if (INITRAMFS_TAR_VBASE + map_size >= KHEAP_BASE) {
        uint8_t* copy = (uint8_t*)kmalloc(len);
        if (!copy) {
            return NULL;
        }
        memcpy(copy, (const void*)mod->mod_start, len);
        if (out_len) {
            *out_len = len;
        }
        return copy;
    }

    paging_map_range(INITRAMFS_TAR_VBASE, paddr_page, map_size, PAGE_PRESENT | PAGE_RW);
    if (out_len) {
        *out_len = len;
    }
    return (const uint8_t*)(INITRAMFS_TAR_VBASE + off);
}

static bool is_leap_year_u32(uint32_t year) {
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

static uint8_t days_in_month_u32(uint32_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 31;
    }
    uint8_t d = days[month - 1u];
    if (month == 2 && is_leap_year_u32(year)) {
        d = 29;
    }
    return d;
}

static void epoch_to_fat_ts(uint32_t epoch, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;

    if (epoch == 0) {
        return;
    }

    uint32_t days = epoch / 86400u;
    uint32_t rem = epoch % 86400u;

    uint8_t hour = (uint8_t)(rem / 3600u);
    rem %= 3600u;
    uint8_t minute = (uint8_t)(rem / 60u);
    uint8_t second = (uint8_t)(rem % 60u);

    uint32_t year = 1970u;
    while (year < 2108u) {
        uint32_t diy = is_leap_year_u32(year) ? 366u : 365u;
        if (days < diy) {
            break;
        }
        days -= diy;
        year++;
    }

    uint8_t month = 1;
    while (month <= 12) {
        uint32_t dim = days_in_month_u32(year, month);
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }
    uint8_t day = (uint8_t)(days + 1u);

    if (year < 1980u) {
        return;
    }
    if (year > 2107u) {
        year = 2107u;
        month = 12;
        day = 31;
        hour = 23;
        minute = 59;
        second = 58;
    }

    uint16_t wdate = (uint16_t)(((uint16_t)(year - 1980u) << 9) |
                               ((uint16_t)month << 5) |
                               (uint16_t)day);
    uint16_t wtime = (uint16_t)(((uint16_t)hour << 11) |
                               ((uint16_t)minute << 5) |
                               (uint16_t)(second / 2u));

    if (out_wtime) *out_wtime = wtime;
    if (out_wdate) *out_wdate = wdate;
}

static uint32_t align_up_512(uint32_t v) {
    return (v + 511u) & ~511u;
}

static uint32_t parse_octal_u32(const char* s, uint32_t max_len) {
    uint32_t value = 0;
    for (uint32_t i = 0; i < max_len; i++) {
        char c = s[i];
        if (c == '\0' || c == ' ') {
            break;
        }
        if (c < '0' || c > '7') {
            break;
        }
        value = (value << 3) + (uint32_t)(c - '0');
    }
    return value;
}

static bool is_zero_block(const uint8_t* p) {
    for (uint32_t i = 0; i < 512u; i++) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static char* dup_path(const char* prefix, const char* name) {
    while (name[0] == '.' && name[1] == '/') {
        name += 2;
    }

    uint32_t prefix_len = prefix ? (uint32_t)strlen(prefix) : 0;
    uint32_t name_len = (uint32_t)strlen(name);
    uint32_t total = name_len + 1u;
    bool use_prefix = prefix && prefix[0] != '\0';
    if (use_prefix) {
        total = prefix_len + 1u + name_len + 1u;
    }

    char* out = (char*)kmalloc(total);
    if (!out) {
        return NULL;
    }

    uint32_t pos = 0;
    if (use_prefix) {
        for (uint32_t i = 0; i < prefix_len; i++) {
            out[pos++] = prefix[i];
        }
        out[pos++] = '/';
    }
    for (uint32_t i = 0; i < name_len; i++) {
        out[pos++] = name[i];
    }
    out[pos] = '\0';
    return out;
}

static uint32_t tar_count_files(const uint8_t* tar, uint32_t tar_len) {
    if (!tar || tar_len < 512u) {
        return 0;
    }

    uint32_t count = 0;
    uint32_t off = 0;
    while (off + 512u <= tar_len) {
        const uint8_t* block = tar + off;
        if (is_zero_block(block)) {
            break;
        }

        const tar_header_t* h = (const tar_header_t*)block;
        uint32_t size = parse_octal_u32(h->size, sizeof(h->size));
        char type = h->typeflag;
        if (type == '\0' || type == '0') {
            count++;
        }

        off += 512u + align_up_512(size);
    }

    return count;
}

static uint32_t tar_fill_files(vfs_file_t* out_files, uint32_t max_files, const uint8_t* tar, uint32_t tar_len) {
    if (!out_files || max_files == 0 || !tar || tar_len < 512u) {
        return 0;
    }

    uint32_t idx = 0;
    uint32_t off = 0;
    while (off + 512u <= tar_len && idx < max_files) {
        const uint8_t* block = tar + off;
        if (is_zero_block(block)) {
            break;
        }

        const tar_header_t* h = (const tar_header_t*)block;
        uint32_t size = parse_octal_u32(h->size, sizeof(h->size));
        uint32_t mtime = parse_octal_u32(h->mtime, sizeof(h->mtime));
        char type = h->typeflag;
        const uint8_t* data = block + 512u;

        if ((type == '\0' || type == '0') && h->name[0] != '\0') {
            char name_buf[101];
            memcpy(name_buf, h->name, 100);
            name_buf[100] = '\0';

            char prefix_buf[156];
            memcpy(prefix_buf, h->prefix, 155);
            prefix_buf[155] = '\0';

            out_files[idx].name = dup_path(prefix_buf, name_buf);
            out_files[idx].data = data;
            out_files[idx].size = size;
            epoch_to_fat_ts(mtime, &out_files[idx].wtime, &out_files[idx].wdate);
            idx++;
        }

        off += 512u + align_up_512(size);
    }

    return idx;
}

typedef enum {
    FAT_KIND_NONE = 0,
    FAT_KIND_12 = 12,
    FAT_KIND_16 = 16,
} fat_kind_t;

typedef struct fat_view {
    const uint8_t* img;
    uint32_t img_len;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint32_t total_sectors;
    uint16_t fat_sectors;
    uint32_t root_dir_sectors;
    uint32_t first_root_sector;
    uint32_t first_data_sector;
    uint32_t fat_offset_bytes;
    uint32_t fat_size_bytes;
    uint32_t root_offset_bytes;
    uint32_t root_size_bytes;
    uint32_t data_offset_bytes;
    uint32_t cluster_size_bytes;
    uint32_t cluster_count;
    fat_kind_t kind;
} fat_view_t;

static bool fat_mount_view(fat_view_t* out, const uint8_t* img, uint32_t img_len) {
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->kind = FAT_KIND_NONE;

    if (!img || img_len < 512u) {
        return false;
    }

    uint16_t bytes_per_sector = read_le16(img + 11);
    uint8_t sectors_per_cluster = img[13];
    uint16_t reserved_sectors = read_le16(img + 14);
    uint8_t num_fats = img[16];
    uint16_t root_entries = read_le16(img + 17);
    uint16_t total16 = read_le16(img + 19);
    uint16_t fat_sectors = read_le16(img + 22);
    uint32_t total32 = read_le32(img + 32);

    if (bytes_per_sector != 512u) {
        return false;
    }
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1u)) != 0) {
        return false;
    }
    if (reserved_sectors == 0 || num_fats == 0 || fat_sectors == 0) {
        return false;
    }

    uint32_t total_sectors = total16 ? (uint32_t)total16 : total32;
    if (total_sectors == 0) {
        return false;
    }

    uint32_t root_dir_sectors = ((uint32_t)root_entries * 32u + (bytes_per_sector - 1u)) / bytes_per_sector;
    uint32_t first_root_sector = (uint32_t)reserved_sectors + (uint32_t)num_fats * (uint32_t)fat_sectors;
    uint32_t first_data_sector = first_root_sector + root_dir_sectors;
    if (first_data_sector > total_sectors) {
        return false;
    }

    uint32_t data_sectors = total_sectors - first_data_sector;
    uint32_t cluster_count = data_sectors / sectors_per_cluster;

    fat_kind_t kind = FAT_KIND_NONE;
    if (cluster_count < 4085u) {
        kind = FAT_KIND_12;
    } else if (cluster_count < 65525u) {
        kind = FAT_KIND_16;
    } else {
        return false;
    }

    uint32_t fat_offset_bytes = (uint32_t)reserved_sectors * bytes_per_sector;
    uint32_t fat_size_bytes = (uint32_t)fat_sectors * bytes_per_sector;
    uint32_t root_offset_bytes = first_root_sector * bytes_per_sector;
    uint32_t root_size_bytes = root_dir_sectors * bytes_per_sector;
    uint32_t data_offset_bytes = first_data_sector * bytes_per_sector;
    uint32_t cluster_size_bytes = (uint32_t)bytes_per_sector * sectors_per_cluster;

    if (fat_offset_bytes + fat_size_bytes > img_len) {
        return false;
    }
    if (root_offset_bytes + root_size_bytes > img_len) {
        return false;
    }
    if (data_offset_bytes > img_len) {
        return false;
    }

    out->img = img;
    out->img_len = img_len;
    out->bytes_per_sector = bytes_per_sector;
    out->sectors_per_cluster = sectors_per_cluster;
    out->reserved_sectors = reserved_sectors;
    out->num_fats = num_fats;
    out->root_entries = root_entries;
    out->total_sectors = total_sectors;
    out->fat_sectors = fat_sectors;
    out->root_dir_sectors = root_dir_sectors;
    out->first_root_sector = first_root_sector;
    out->first_data_sector = first_data_sector;
    out->fat_offset_bytes = fat_offset_bytes;
    out->fat_size_bytes = fat_size_bytes;
    out->root_offset_bytes = root_offset_bytes;
    out->root_size_bytes = root_size_bytes;
    out->data_offset_bytes = data_offset_bytes;
    out->cluster_size_bytes = cluster_size_bytes;
    out->cluster_count = cluster_count;
    out->kind = kind;
    return true;
}

static uint16_t fat_next_cluster(const fat_view_t* fs, uint16_t cluster) {
    if (!fs || fs->kind == FAT_KIND_NONE) {
        return 0;
    }

    const uint8_t* fat = fs->img + fs->fat_offset_bytes;
    if (fs->kind == FAT_KIND_12) {
        uint32_t offset = (uint32_t)cluster + (uint32_t)(cluster / 2u);
        if (offset + 1u >= fs->fat_size_bytes) {
            return 0;
        }
        uint16_t v = (uint16_t)fat[offset] | (uint16_t)((uint16_t)fat[offset + 1u] << 8);
        if ((cluster & 1u) == 0) {
            v &= 0x0FFFu;
        } else {
            v >>= 4;
        }
        return v;
    }

    uint32_t offset = (uint32_t)cluster * 2u;
    if (offset + 1u >= fs->fat_size_bytes) {
        return 0;
    }
    return (uint16_t)fat[offset] | (uint16_t)((uint16_t)fat[offset + 1u] << 8);
}

static bool fat_is_eoc(const fat_view_t* fs, uint16_t cluster) {
    if (!fs) {
        return true;
    }
    if (fs->kind == FAT_KIND_12) {
        return cluster >= 0x0FF8u;
    }
    if (fs->kind == FAT_KIND_16) {
        return cluster >= 0xFFF8u;
    }
    return true;
}

static bool fat_name_is_dot(const char* name) {
    if (!name || name[0] == '\0') {
        return false;
    }
    if (name[0] == '.' && name[1] == '\0') {
        return true;
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return true;
    }
    return false;
}

static void fat_make_name(const uint8_t* e, char* out, uint32_t out_len);
static uint32_t fat_count_dir_chain_files(const fat_view_t* fs, uint16_t start_cluster, uint32_t depth);

static uint32_t fat_count_root_files(const fat_view_t* fs) {
    if (!fs || fs->kind == FAT_KIND_NONE) {
        return 0;
    }

    uint32_t count = 0;
    const uint8_t* root = fs->img + fs->root_offset_bytes;
    uint32_t entries = (fs->root_size_bytes / 32u);
    for (uint32_t i = 0; i < entries; i++) {
        const uint8_t* e = root + i * 32u;
        if (e[0] == 0x00) {
            break;
        }
        if (e[0] == 0xE5) {
            continue;
        }
        uint8_t attr = e[11];
        if (attr == 0x0F) {
            continue;
        }
        if (attr & 0x08u) {
            continue;
        }
        if (e[0] == ' ') {
            continue;
        }

        char name[32];
        fat_make_name(e, name, sizeof(name));
        if (name[0] == '\0' || fat_name_is_dot(name)) {
            continue;
        }

        if (attr & 0x10u) {
            uint16_t first_cluster = read_le16(e + 26);
            count += fat_count_dir_chain_files(fs, first_cluster, 1u);
            continue;
        }

        count++;
    }

    return count;
}

static uint32_t fat_count_dir_chain_files(const fat_view_t* fs, uint16_t start_cluster, uint32_t depth) {
    if (!fs || fs->kind == FAT_KIND_NONE) {
        return 0;
    }
    if (start_cluster < 2u) {
        return 0;
    }
    if (depth >= 8u) {
        return 0;
    }

    uint32_t count = 0;
    uint16_t cluster = start_cluster;
    uint32_t max_steps = fs->cluster_count + 4u;

    for (uint32_t step = 0; step < max_steps; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(fs->cluster_count + 2u)) {
            break;
        }

        uint32_t cl_off = fs->data_offset_bytes + (uint32_t)(cluster - 2u) * fs->cluster_size_bytes;
        if (cl_off + fs->cluster_size_bytes > fs->img_len) {
            break;
        }

        const uint8_t* dir = fs->img + cl_off;
        for (uint32_t off = 0; off + 32u <= fs->cluster_size_bytes; off += 32u) {
            const uint8_t* e = dir + off;
            if (e[0] == 0x00) {
                return count;
            }
            if (e[0] == 0xE5) {
                continue;
            }
            uint8_t attr = e[11];
            if (attr == 0x0F) {
                continue;
            }
            if (attr & 0x08u) {
                continue;
            }
            if (e[0] == ' ') {
                continue;
            }

            char name[32];
            fat_make_name(e, name, sizeof(name));
            if (name[0] == '\0' || fat_name_is_dot(name)) {
                continue;
            }

            if (attr & 0x10u) {
                uint16_t first_cluster = read_le16(e + 26);
                count += fat_count_dir_chain_files(fs, first_cluster, depth + 1u);
            } else {
                count++;
            }
        }

        uint16_t next = fat_next_cluster(fs, cluster);
        if (fat_is_eoc(fs, next)) {
            break;
        }
        cluster = next;
    }

    return count;
}

static void fat_make_name(const uint8_t* e, char* out, uint32_t out_len) {
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';

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
        out[pos++] = (char)tolower(name[i]);
    }
    if (e_end >= 0 && pos + 1u < out_len) {
        out[pos++] = '.';
        for (int i = 0; i <= e_end && pos + 1u < out_len; i++) {
            out[pos++] = (char)tolower(ext[i]);
        }
    }
    out[pos] = '\0';
}

static bool fat_read_file_alloc(const fat_view_t* fs, uint16_t start_cluster, uint32_t size, uint8_t** out_buf) {
    if (!out_buf) {
        return false;
    }
    *out_buf = NULL;

    if (!fs || fs->kind == FAT_KIND_NONE) {
        return false;
    }

    uint8_t* buf = NULL;
    if (size == 0) {
        buf = (uint8_t*)kmalloc(1u);
        if (!buf) {
            return false;
        }
        buf[0] = 0;
        *out_buf = buf;
        return true;
    }

    buf = (uint8_t*)kmalloc(size);
    if (!buf) {
        return false;
    }

    uint32_t written = 0;
    uint16_t cluster = start_cluster;
    if (cluster < 2u) {
        kfree(buf);
        return false;
    }

    uint32_t max_steps = fs->cluster_count + 4u;
    for (uint32_t step = 0; step < max_steps && written < size; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(fs->cluster_count + 2u)) {
            kfree(buf);
            return false;
        }

        uint32_t cl_off = fs->data_offset_bytes + (uint32_t)(cluster - 2u) * fs->cluster_size_bytes;
        if (cl_off >= fs->img_len) {
            kfree(buf);
            return false;
        }
        uint32_t to_copy = fs->cluster_size_bytes;
        if (to_copy > (size - written)) {
            to_copy = (size - written);
        }
        if (cl_off + to_copy > fs->img_len) {
            kfree(buf);
            return false;
        }
        memcpy(buf + written, fs->img + cl_off, to_copy);
        written += to_copy;

        if (written >= size) {
            break;
        }

        uint16_t next = fat_next_cluster(fs, cluster);
        if (fat_is_eoc(fs, next)) {
            break;
        }
        cluster = next;
    }

    if (written != size) {
        kfree(buf);
        return false;
    }

    *out_buf = buf;
    return true;
}

static uint32_t fat_fill_dir_chain_files(vfs_file_t* out_files, uint32_t start_idx, uint32_t max_files, const fat_view_t* fs, const char* prefix, uint16_t start_cluster, uint32_t depth) {
    if (!out_files || !fs || fs->kind == FAT_KIND_NONE || !prefix) {
        return 0;
    }
    if (start_cluster < 2u) {
        return 0;
    }
    if (depth >= 8u) {
        return 0;
    }

    uint32_t idx = start_idx;
    uint16_t cluster = start_cluster;
    uint32_t max_steps = fs->cluster_count + 4u;

    for (uint32_t step = 0; step < max_steps && idx < max_files; step++) {
        if (cluster < 2u || cluster >= (uint16_t)(fs->cluster_count + 2u)) {
            break;
        }

        uint32_t cl_off = fs->data_offset_bytes + (uint32_t)(cluster - 2u) * fs->cluster_size_bytes;
        if (cl_off + fs->cluster_size_bytes > fs->img_len) {
            break;
        }

        const uint8_t* dir = fs->img + cl_off;
        for (uint32_t off = 0; off + 32u <= fs->cluster_size_bytes && idx < max_files; off += 32u) {
            const uint8_t* e = dir + off;
            if (e[0] == 0x00) {
                return idx - start_idx;
            }
            if (e[0] == 0xE5) {
                continue;
            }
            uint8_t attr = e[11];
            if (attr == 0x0F) {
                continue;
            }
            if (attr & 0x08u) {
                continue;
            }
            if (e[0] == ' ') {
                continue;
            }

            char name[32];
            fat_make_name(e, name, sizeof(name));
            if (name[0] == '\0' || fat_name_is_dot(name)) {
                continue;
            }

            if (attr & 0x10u) {
                uint16_t first_cluster = read_le16(e + 26);
                char* new_prefix = dup_path(prefix, name);
                if (!new_prefix) {
                    continue;
                }
                idx += fat_fill_dir_chain_files(out_files, idx, max_files, fs, new_prefix, first_cluster, depth + 1u);
                kfree(new_prefix);
                continue;
            }

            uint16_t first_cluster = read_le16(e + 26);
            uint32_t size = read_le32(e + 28);
            uint16_t wtime = read_le16(e + 22);
            uint16_t wdate = read_le16(e + 24);

            uint8_t* data = NULL;
            if (!fat_read_file_alloc(fs, first_cluster, size, &data)) {
                continue;
            }

            out_files[idx].name = dup_path(prefix, name);
            out_files[idx].data = data;
            out_files[idx].size = size;
            out_files[idx].wtime = wtime;
            out_files[idx].wdate = wdate;
            idx++;
        }

        uint16_t next = fat_next_cluster(fs, cluster);
        if (fat_is_eoc(fs, next)) {
            break;
        }
        cluster = next;
    }

    return idx - start_idx;
}

static uint32_t fat_fill_root_files(vfs_file_t* out_files, uint32_t start_idx, uint32_t max_files, const fat_view_t* fs) {
    if (!out_files || !fs || fs->kind == FAT_KIND_NONE) {
        return 0;
    }
    uint32_t idx = start_idx;
    const uint8_t* root = fs->img + fs->root_offset_bytes;
    uint32_t entries = (fs->root_size_bytes / 32u);

    for (uint32_t i = 0; i < entries && idx < max_files; i++) {
        const uint8_t* e = root + i * 32u;
        if (e[0] == 0x00) {
            break;
        }
        if (e[0] == 0xE5) {
            continue;
        }
        uint8_t attr = e[11];
        if (attr == 0x0F) {
            continue;
        }
        if (attr & 0x08u) {
            continue;
        }
        if (e[0] == ' ') {
            continue;
        }

        char name[32];
        fat_make_name(e, name, sizeof(name));
        if (name[0] == '\0' || fat_name_is_dot(name)) {
            continue;
        }

        if (attr & 0x10u) {
            uint16_t first_cluster = read_le16(e + 26);
            char* prefix = dup_path("fat", name);
            if (!prefix) {
                continue;
            }
            idx += fat_fill_dir_chain_files(out_files, idx, max_files, fs, prefix, first_cluster, 1u);
            kfree(prefix);
            continue;
        }

        uint16_t first_cluster = read_le16(e + 26);
        uint32_t size = read_le32(e + 28);
        uint16_t wtime = read_le16(e + 22);
        uint16_t wdate = read_le16(e + 24);

        uint8_t* data = NULL;
        if (!fat_read_file_alloc(fs, first_cluster, size, &data)) {
            continue;
        }

        out_files[idx].name = dup_path("fat", name);
        out_files[idx].data = data;
        out_files[idx].size = size;
        out_files[idx].wtime = wtime;
        out_files[idx].wdate = wdate;
        idx++;
    }

    return idx - start_idx;
}

void vfs_init(const multiboot_info_t* mbi) {
    ready = false;
    file_count = 0;
    files = NULL;

    ramfs_init();

    if (!mbi || (mbi->flags & MULTIBOOT_INFO_MODS) == 0 || mbi->mods_count == 0 || mbi->mods_addr == 0) {
        serial_write_string("[VFS] no multiboot modules\n");
        ready = true;
        return;
    }

    const multiboot_module_t* mods = (const multiboot_module_t*)mbi->mods_addr;
    const uint8_t* tar = NULL;
    uint32_t tar_len = 0;
    tar = map_tar_module_high(&mods[0], &tar_len);

    fat_view_t fat = {0};
    bool fat_ok = false;
    if (mbi->mods_count >= 2 && mods[1].mod_end > mods[1].mod_start) {
        const uint8_t* fat_img = (const uint8_t*)mods[1].mod_start;
        uint32_t fat_len = mods[1].mod_end - mods[1].mod_start;
        fat_ok = fat_mount_view(&fat, fat_img, fat_len);
        if (!fat_ok) {
            serial_write_string("[VFS] fat module present but unsupported\n");
        }
    }

    uint32_t tar_count = tar_count_files(tar, tar_len);
    uint32_t fat_count = fat_ok ? fat_count_root_files(&fat) : 0;
    file_count = tar_count + fat_count;

    if (file_count == 0) {
        serial_write_string("[VFS] no files\n");
        ready = true;
        return;
    }

    files = (vfs_file_t*)kcalloc(file_count, sizeof(vfs_file_t));
    if (!files) {
        serial_write_string("[VFS] out of memory\n");
        file_count = 0;
        return;
    }

    uint32_t idx = 0;
    if (tar_count) {
        idx += tar_fill_files(files + idx, file_count - idx, tar, tar_len);
    }
    if (fat_ok && fat_count) {
        idx += fat_fill_root_files(files, idx, file_count, &fat);
    }

    file_count = idx;
    ready = true;

    serial_write_string("[VFS] initramfs files=");
    serial_write_dec((int32_t)tar_count);
    if (fat_ok) {
        serial_write_string(" fat=");
        serial_write_dec((int32_t)fat_count);
    }
    serial_write_char('\n');
}

bool vfs_is_ready(void) {
    return ready;
}

uint32_t vfs_file_count(void) {
    return file_count;
}

const char* vfs_file_name(uint32_t idx) {
    if (idx >= file_count) {
        return NULL;
    }
    return files[idx].name;
}

uint32_t vfs_file_size(uint32_t idx) {
    if (idx >= file_count) {
        return 0;
    }
    return files[idx].size;
}

bool vfs_file_mtime(uint32_t idx, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (idx >= file_count) {
        return false;
    }
    if (out_wtime) *out_wtime = files[idx].wtime;
    if (out_wdate) *out_wdate = files[idx].wdate;
    return true;
}

static bool path_equals_ci(const char* a, const char* b) {
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

bool vfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size) {
    if (!ready || !path) {
        return false;
    }

    // Accept both "foo" and "/foo" paths.
    while (*path == '/') {
        path++;
    }

    if (ramfs_read_file(path, out_data, out_size)) {
        return true;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        const char* name = files[i].name;
        const char* p = name;
        while (*p == '/') p++;
        if (path_equals_ci(p, path)) {
            if (out_data) *out_data = files[i].data;
            if (out_size) *out_size = files[i].size;
            return true;
        }
    }
    return false;
}
