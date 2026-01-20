#include "vfs.h"
#include "kheap.h"
#include "string.h"
#include "serial.h"

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
} vfs_file_t;

static vfs_file_t* files = NULL;
static uint32_t file_count = 0;
static bool ready = false;

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

void vfs_init(const multiboot_info_t* mbi) {
    ready = false;
    file_count = 0;
    files = NULL;

    if (!mbi || (mbi->flags & MULTIBOOT_INFO_MODS) == 0 || mbi->mods_count == 0 || mbi->mods_addr == 0) {
        serial_write_string("[VFS] no multiboot modules\n");
        return;
    }

    const multiboot_module_t* mods = (const multiboot_module_t*)mbi->mods_addr;
    uint32_t start = mods[0].mod_start;
    uint32_t end = mods[0].mod_end;
    if (end <= start) {
        serial_write_string("[VFS] initramfs module invalid\n");
        return;
    }

    const uint8_t* tar = (const uint8_t*)start;
    uint32_t tar_len = end - start;

    // First pass: count files.
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
            file_count++;
        }

        off += 512u + align_up_512(size);
    }

    if (file_count == 0) {
        serial_write_string("[VFS] initramfs: no files\n");
        return;
    }

    files = (vfs_file_t*)kcalloc(file_count, sizeof(vfs_file_t));
    if (!files) {
        serial_write_string("[VFS] out of memory\n");
        file_count = 0;
        return;
    }

    // Second pass: populate file list.
    uint32_t idx = 0;
    off = 0;
    while (off + 512u <= tar_len && idx < file_count) {
        const uint8_t* block = tar + off;
        if (is_zero_block(block)) {
            break;
        }

        const tar_header_t* h = (const tar_header_t*)block;
        uint32_t size = parse_octal_u32(h->size, sizeof(h->size));
        char type = h->typeflag;
        const uint8_t* data = block + 512u;

        if ((type == '\0' || type == '0') && h->name[0] != '\0') {
            char name_buf[101];
            memcpy(name_buf, h->name, 100);
            name_buf[100] = '\0';

            char prefix_buf[156];
            memcpy(prefix_buf, h->prefix, 155);
            prefix_buf[155] = '\0';

            files[idx].name = dup_path(prefix_buf, name_buf);
            files[idx].data = data;
            files[idx].size = size;
            idx++;
        }

        off += 512u + align_up_512(size);
    }

    file_count = idx;
    ready = true;

    serial_write_string("[VFS] initramfs files=");
    serial_write_dec((int32_t)file_count);
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

bool vfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size) {
    if (!ready || !path) {
        return false;
    }

    // Accept both "foo" and "/foo" paths.
    while (*path == '/') {
        path++;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        const char* name = files[i].name;
        const char* p = name;
        while (*p == '/') p++;
        if (strcmp(p, path) == 0) {
            if (out_data) *out_data = files[i].data;
            if (out_size) *out_size = files[i].size;
            return true;
        }
    }
    return false;
}
