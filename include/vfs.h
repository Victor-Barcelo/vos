#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "multiboot.h"

void vfs_init(const multiboot_info_t* mbi);
bool vfs_is_ready(void);

uint32_t vfs_file_count(void);
const char* vfs_file_name(uint32_t idx);

bool vfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size);

#endif

