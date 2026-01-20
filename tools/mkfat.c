#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void fat12_set(uint8_t* fat, uint16_t cluster, uint16_t value) {
    uint32_t offset = (uint32_t)cluster + (uint32_t)(cluster / 2u);
    if ((cluster & 1u) == 0) {
        fat[offset + 0] = (uint8_t)(value & 0xFFu);
        fat[offset + 1] = (uint8_t)((fat[offset + 1] & 0xF0u) | ((value >> 8) & 0x0Fu));
    } else {
        fat[offset + 0] = (uint8_t)((fat[offset + 0] & 0x0Fu) | ((value << 4) & 0xF0u));
        fat[offset + 1] = (uint8_t)((value >> 4) & 0xFFu);
    }
}

static void dir_write_entry(uint8_t* entry, const char* name8, const char* ext3, uint8_t attr, uint16_t first_cluster, uint32_t size) {
    memset(entry, 0, 32);
    memcpy(entry + 0, name8, 8);
    memcpy(entry + 8, ext3, 3);
    entry[11] = attr;
    write_le16(entry + 26, first_cluster);
    write_le32(entry + 28, size);
}

static void pad_83(char out[8], const char* in) {
    memset(out, ' ', 8);
    size_t n = strlen(in);
    if (n > 8) n = 8;
    for (size_t i = 0; i < n; i++) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[i] = c;
    }
}

static void pad_3(char out[3], const char* in) {
    memset(out, ' ', 3);
    size_t n = strlen(in);
    if (n > 3) n = 3;
    for (size_t i = 0; i < n; i++) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[i] = c;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <out.img>\n", argv[0]);
        return 1;
    }

    const uint16_t bytes_per_sector = 512;
    const uint8_t sectors_per_cluster = 1;
    const uint16_t reserved_sectors = 1;
    const uint8_t num_fats = 2;
    const uint16_t root_entries = 224;
    const uint16_t total_sectors16 = 2880;
    const uint8_t media = 0xF0;
    const uint16_t fat_sectors = 9;
    const uint16_t sectors_per_track = 18;
    const uint16_t heads = 2;

    const uint32_t root_dir_sectors = ((uint32_t)root_entries * 32u + (bytes_per_sector - 1u)) / bytes_per_sector;
    const uint32_t first_root_sector = (uint32_t)reserved_sectors + (uint32_t)num_fats * (uint32_t)fat_sectors;
    const uint32_t first_data_sector = first_root_sector + root_dir_sectors;
    const uint32_t fat_bytes = (uint32_t)fat_sectors * bytes_per_sector;

    const uint32_t image_bytes = (uint32_t)total_sectors16 * bytes_per_sector;
    uint8_t* image = (uint8_t*)calloc(1, image_bytes);
    if (!image) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    // Boot sector (BPB + EBR)
    uint8_t* bs = image;
    bs[0] = 0xEB;
    bs[1] = 0x3C;
    bs[2] = 0x90;
    memcpy(bs + 3, "VOSFAT  ", 8);
    write_le16(bs + 11, bytes_per_sector);
    bs[13] = sectors_per_cluster;
    write_le16(bs + 14, reserved_sectors);
    bs[16] = num_fats;
    write_le16(bs + 17, root_entries);
    write_le16(bs + 19, total_sectors16);
    bs[21] = media;
    write_le16(bs + 22, fat_sectors);
    write_le16(bs + 24, sectors_per_track);
    write_le16(bs + 26, heads);
    write_le32(bs + 28, 0);
    write_le32(bs + 32, 0);
    bs[36] = 0x00;
    bs[37] = 0x00;
    bs[38] = 0x29;
    write_le32(bs + 39, 0x12345678u);
    memcpy(bs + 43, "VOS FAT12  ", 11);
    memcpy(bs + 54, "FAT12   ", 8);
    bs[510] = 0x55;
    bs[511] = 0xAA;

    // FAT tables
    uint8_t* fat1 = image + (uint32_t)reserved_sectors * bytes_per_sector;
    uint8_t* fat2 = fat1 + fat_bytes;
    fat1[0] = media;
    fat1[1] = 0xFF;
    fat1[2] = 0xFF;

    // Root directory
    uint8_t* root = image + first_root_sector * bytes_per_sector;

    const char hello_text[] = "Hello from FAT12 on VOS!\r\n";
    const char big_text[] =
        "This is a larger file stored in multiple clusters.\r\n"
        "It exists to validate FAT12 cluster chaining in VOS.\r\n"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n";

    uint16_t next_cluster = 2;

    // HELLO.TXT (1 cluster)
    {
        uint16_t start = next_cluster++;
        fat12_set(fat1, start, 0xFFFu);
        uint32_t data_off = (first_data_sector + (uint32_t)(start - 2u)) * bytes_per_sector;
        memcpy(image + data_off, hello_text, sizeof(hello_text) - 1u);

        char n[8], e[3];
        pad_83(n, "HELLO");
        pad_3(e, "TXT");
        dir_write_entry(root + 0 * 32, n, e, 0x20, start, (uint32_t)(sizeof(hello_text) - 1u));
    }

    // BIG.TXT (multiple clusters)
    {
        uint32_t size = (uint32_t)(sizeof(big_text) - 1u);
        uint32_t remaining = size;

        uint16_t start = next_cluster;
        uint16_t prev = 0;
        uint32_t pos = 0;

        while (remaining) {
            uint16_t cl = next_cluster++;
            if (prev) {
                fat12_set(fat1, prev, cl);
            }
            prev = cl;

            uint32_t data_off = (first_data_sector + (uint32_t)(cl - 2u)) * bytes_per_sector;
            uint32_t chunk = remaining;
            if (chunk > bytes_per_sector) chunk = bytes_per_sector;
            memcpy(image + data_off, big_text + pos, chunk);
            pos += chunk;
            remaining -= chunk;
        }
        fat12_set(fat1, prev, 0xFFFu);

        char n[8], e[3];
        pad_83(n, "BIG");
        pad_3(e, "TXT");
        dir_write_entry(root + 1 * 32, n, e, 0x20, start, size);
    }

    memcpy(fat2, fat1, fat_bytes);

    FILE* f = fopen(argv[1], "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s\n", argv[1]);
        free(image);
        return 1;
    }
    if (fwrite(image, 1, image_bytes, f) != image_bytes) {
        fprintf(stderr, "failed to write image\n");
        fclose(f);
        free(image);
        return 1;
    }
    fclose(f);
    free(image);
    return 0;
}
