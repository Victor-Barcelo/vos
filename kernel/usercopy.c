#include "usercopy.h"
#include "paging.h"
#include "string.h"

bool copy_from_user(void* dst, const void* src_user, uint32_t len) {
    if (len == 0) {
        return true;
    }
    if (!dst || !src_user) {
        return false;
    }

    uint32_t addr = (uint32_t)src_user;
    if (!paging_user_accessible_range(addr, len, false)) {
        return false;
    }

    memcpy(dst, (const void*)addr, len);
    return true;
}

bool copy_to_user(void* dst_user, const void* src, uint32_t len) {
    if (len == 0) {
        return true;
    }
    if (!dst_user || !src) {
        return false;
    }

    uint32_t addr = (uint32_t)dst_user;
    if (!paging_user_accessible_range(addr, len, true)) {
        return false;
    }

    memcpy((void*)addr, src, len);
    return true;
}
