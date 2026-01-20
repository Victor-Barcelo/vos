#ifndef USERCOPY_H
#define USERCOPY_H

#include "types.h"

// Copy bytes from a user pointer into kernel memory. Returns false if the user
// range is not mapped/user-accessible in the current address space.
bool copy_from_user(void* dst, const void* src_user, uint32_t len);

// Copy bytes from kernel memory into a user pointer. Returns false if the user
// range is not mapped/user-writable in the current address space.
bool copy_to_user(void* dst_user, const void* src, uint32_t len);

#endif
