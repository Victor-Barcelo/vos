#ifndef FATDISK_H
#define FATDISK_H

#include "types.h"

typedef struct fatdisk_dirent {
    char name[64];
    bool is_dir;
    uint32_t size;
} fatdisk_dirent_t;

// Mount a FAT16 "superfloppy" volume at /disk (BPB in LBA0).
// Requires an ATA device on the primary master.
bool fatdisk_init(void);
bool fatdisk_is_ready(void);
const char* fatdisk_label(void);

bool fatdisk_is_dir(const char* abs_path);
bool fatdisk_is_file(const char* abs_path);
bool fatdisk_stat(const char* abs_path, bool* out_is_dir, uint32_t* out_size);

uint32_t fatdisk_list_dir(const char* abs_path, fatdisk_dirent_t* out, uint32_t max);

// Allocates a buffer with kmalloc; caller must kfree().
bool fatdisk_read_file_alloc(const char* abs_path, uint8_t** out_data, uint32_t* out_size);

bool fatdisk_write_file(const char* abs_path, const uint8_t* data, uint32_t size, bool overwrite);
bool fatdisk_mkdir(const char* abs_path);
bool fatdisk_rename(const char* abs_old, const char* abs_new);

#endif
