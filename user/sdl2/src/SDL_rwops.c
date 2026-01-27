/*
 * SDL_rwops.c - VOS minimal SDL2 shim
 * Implementation of Read/Write operations using standard C file I/O
 */

#include "SDL2/SDL_rwops.h"
#include "SDL2/SDL_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Track read-only flag for memory RWops (stored in hidden.unknown.data2 as int) */
#define MEM_READONLY_FLAG ((void*)1)

/*
 * File I/O callbacks
 */

static Sint64 file_size(SDL_RWops *context)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    long current, size;

    current = ftell(fp);
    if (current < 0) return -1;

    if (fseek(fp, 0, SEEK_END) < 0) return -1;
    size = ftell(fp);
    if (fseek(fp, current, SEEK_SET) < 0) return -1;

    return (Sint64)size;
}

static Sint64 file_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    int fwhence;

    switch (whence) {
        case RW_SEEK_SET: fwhence = SEEK_SET; break;
        case RW_SEEK_CUR: fwhence = SEEK_CUR; break;
        case RW_SEEK_END: fwhence = SEEK_END; break;
        default:
            SDL_SetError("Invalid seek whence value");
            return -1;
    }

    if (fseek(fp, (long)offset, fwhence) < 0) {
        SDL_SetError("File seek failed");
        return -1;
    }

    return (Sint64)ftell(fp);
}

static size_t file_read(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    return fread(ptr, size, maxnum, fp);
}

static size_t file_write(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    return fwrite(ptr, size, num, fp);
}

static int file_close(SDL_RWops *context)
{
    FILE *fp;
    int result = 0;

    if (context == NULL) return 0;

    fp = (FILE *)context->hidden.stdio.fp;
    if (fp != NULL) {
        if (fclose(fp) != 0) {
            SDL_SetError("Error closing file");
            result = -1;
        }
    }

    SDL_FreeRW(context);
    return result;
}

/*
 * Memory I/O callbacks - use hidden.mem structure
 */

static Sint64 mem_size(SDL_RWops *context)
{
    return (Sint64)(context->hidden.mem.stop - context->hidden.mem.base);
}

static Sint64 mem_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    Uint8 *newpos;

    switch (whence) {
        case RW_SEEK_SET:
            newpos = context->hidden.mem.base + offset;
            break;
        case RW_SEEK_CUR:
            newpos = context->hidden.mem.here + offset;
            break;
        case RW_SEEK_END:
            newpos = context->hidden.mem.stop + offset;
            break;
        default:
            SDL_SetError("Invalid seek whence value");
            return -1;
    }

    if (newpos < context->hidden.mem.base) {
        newpos = context->hidden.mem.base;
    } else if (newpos > context->hidden.mem.stop) {
        newpos = context->hidden.mem.stop;
    }

    context->hidden.mem.here = newpos;
    return (Sint64)(context->hidden.mem.here - context->hidden.mem.base);
}

static size_t mem_read(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    size_t total = size * maxnum;
    size_t available = (size_t)(context->hidden.mem.stop - context->hidden.mem.here);

    if (total > available) {
        total = available;
        maxnum = total / size;
    }

    if (maxnum > 0) {
        memcpy(ptr, context->hidden.mem.here, total);
        context->hidden.mem.here += total;
    }

    return maxnum;
}

static size_t mem_write(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    size_t total = size * num;
    size_t available;

    /* Check read-only flag stored in type */
    if (context->type == SDL_RWOPS_MEMORY_RO) {
        SDL_SetError("Cannot write to read-only memory");
        return 0;
    }

    available = (size_t)(context->hidden.mem.stop - context->hidden.mem.here);
    if (total > available) {
        total = available;
        num = total / size;
    }

    if (num > 0) {
        memcpy(context->hidden.mem.here, ptr, total);
        context->hidden.mem.here += total;
    }

    return num;
}

static int mem_close(SDL_RWops *context)
{
    if (context == NULL) return 0;
    SDL_FreeRW(context);
    return 0;
}

/*
 * Public API
 */

SDL_RWops* SDL_AllocRW(void)
{
    SDL_RWops *area;

    area = (SDL_RWops *)malloc(sizeof(SDL_RWops));
    if (area == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    memset(area, 0, sizeof(*area));
    return area;
}

void SDL_FreeRW(SDL_RWops *area)
{
    if (area != NULL) {
        free(area);
    }
}

/* fp_* functions use hidden.stdio.fp for FILE* and hidden.stdio.autoclose for autoclose flag */

static int fp_close(SDL_RWops *context)
{
    int result = 0;

    if (context == NULL) return 0;

    if (context->hidden.stdio.autoclose && context->hidden.stdio.fp != NULL) {
        if (fclose((FILE *)context->hidden.stdio.fp) != 0) {
            SDL_SetError("Error closing file");
            result = -1;
        }
    }

    SDL_FreeRW(context);
    return result;
}

static Sint64 fp_size(SDL_RWops *context)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    long current, size;

    current = ftell(fp);
    if (current < 0) return -1;

    if (fseek(fp, 0, SEEK_END) < 0) return -1;
    size = ftell(fp);
    if (fseek(fp, current, SEEK_SET) < 0) return -1;

    return (Sint64)size;
}

static Sint64 fp_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    int fwhence;

    switch (whence) {
        case RW_SEEK_SET: fwhence = SEEK_SET; break;
        case RW_SEEK_CUR: fwhence = SEEK_CUR; break;
        case RW_SEEK_END: fwhence = SEEK_END; break;
        default:
            SDL_SetError("Invalid seek whence value");
            return -1;
    }

    if (fseek(fp, (long)offset, fwhence) < 0) {
        SDL_SetError("File seek failed");
        return -1;
    }

    return (Sint64)ftell(fp);
}

static size_t fp_read(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    return fread(ptr, size, maxnum, fp);
}

static size_t fp_write(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    FILE *fp = (FILE *)context->hidden.stdio.fp;
    return fwrite(ptr, size, num, fp);
}

SDL_RWops* SDL_RWFromFP(void *fp, int autoclose)
{
    SDL_RWops *rwops;

    if (fp == NULL) {
        SDL_SetError("Invalid file pointer");
        return NULL;
    }

    rwops = SDL_AllocRW();
    if (rwops == NULL) {
        return NULL;
    }

    rwops->type = SDL_RWOPS_STDFILE;
    rwops->size = fp_size;
    rwops->seek = fp_seek;
    rwops->read = fp_read;
    rwops->write = fp_write;
    rwops->close = fp_close;
    rwops->hidden.stdio.fp = fp;
    rwops->hidden.stdio.autoclose = autoclose ? (void*)1 : NULL;

    return rwops;
}

SDL_RWops* SDL_RWFromFile(const char *file, const char *mode)
{
    SDL_RWops *rwops;
    FILE *fp;

    if (file == NULL || mode == NULL) {
        SDL_SetError("Invalid parameters");
        return NULL;
    }

    fp = fopen(file, mode);
    if (fp == NULL) {
        SDL_SetError("Could not open file: %s", file);
        return NULL;
    }

    rwops = SDL_AllocRW();
    if (rwops == NULL) {
        fclose(fp);
        return NULL;
    }

    rwops->type = SDL_RWOPS_STDFILE;
    rwops->size = file_size;
    rwops->seek = file_seek;
    rwops->read = file_read;
    rwops->write = file_write;
    rwops->close = file_close;
    rwops->hidden.stdio.fp = fp;
    rwops->hidden.stdio.autoclose = (void*)1;  /* Auto-close on SDL_RWFromFile */

    return rwops;
}

SDL_RWops* SDL_RWFromMem(void *mem, int size)
{
    SDL_RWops *rwops;

    if (mem == NULL || size < 0) {
        SDL_SetError("Invalid parameters");
        return NULL;
    }

    rwops = SDL_AllocRW();
    if (rwops == NULL) {
        return NULL;
    }

    rwops->type = SDL_RWOPS_MEMORY;
    rwops->size = mem_size;
    rwops->seek = mem_seek;
    rwops->read = mem_read;
    rwops->write = mem_write;
    rwops->close = mem_close;
    rwops->hidden.mem.base = (Uint8 *)mem;
    rwops->hidden.mem.here = rwops->hidden.mem.base;
    rwops->hidden.mem.stop = rwops->hidden.mem.base + size;

    return rwops;
}

SDL_RWops* SDL_RWFromConstMem(const void *mem, int size)
{
    SDL_RWops *rwops;

    if (mem == NULL || size < 0) {
        SDL_SetError("Invalid parameters");
        return NULL;
    }

    rwops = SDL_AllocRW();
    if (rwops == NULL) {
        return NULL;
    }

    rwops->type = SDL_RWOPS_MEMORY_RO;  /* Read-only memory */
    rwops->size = mem_size;
    rwops->seek = mem_seek;
    rwops->read = mem_read;
    rwops->write = mem_write;
    rwops->close = mem_close;
    rwops->hidden.mem.base = (Uint8 *)mem;  /* Cast away const for internal use */
    rwops->hidden.mem.here = rwops->hidden.mem.base;
    rwops->hidden.mem.stop = rwops->hidden.mem.base + size;

    return rwops;
}

int SDL_RWclose(SDL_RWops *context)
{
    if (context == NULL || context->close == NULL) {
        return 0;
    }
    return context->close(context);
}

size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    if (context == NULL || context->read == NULL) {
        return 0;
    }
    return context->read(context, ptr, size, maxnum);
}

size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    if (context == NULL || context->write == NULL) {
        return 0;
    }
    return context->write(context, ptr, size, num);
}

Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence)
{
    if (context == NULL || context->seek == NULL) {
        return -1;
    }
    return context->seek(context, offset, whence);
}

Sint64 SDL_RWtell(SDL_RWops *context)
{
    return SDL_RWseek(context, 0, RW_SEEK_CUR);
}

Sint64 SDL_RWsize(SDL_RWops *context)
{
    if (context == NULL || context->size == NULL) {
        return -1;
    }
    return context->size(context);
}

/*
 * Endian-aware read/write functions
 * VOS is little-endian (x86), so LE functions are native byte order
 */

Uint8 SDL_ReadU8(SDL_RWops *src)
{
    Uint8 value = 0;
    SDL_RWread(src, &value, 1, 1);
    return value;
}

size_t SDL_WriteU8(SDL_RWops *dst, Uint8 value)
{
    return SDL_RWwrite(dst, &value, 1, 1);
}

Uint16 SDL_ReadLE16_func(SDL_RWops *src)
{
    Uint8 data[2];
    if (SDL_RWread(src, data, 1, 2) != 2) {
        return 0;
    }
    return (Uint16)data[0] | ((Uint16)data[1] << 8);
}

Uint32 SDL_ReadLE32_func(SDL_RWops *src)
{
    Uint8 data[4];
    if (SDL_RWread(src, data, 1, 4) != 4) {
        return 0;
    }
    return (Uint32)data[0] |
           ((Uint32)data[1] << 8) |
           ((Uint32)data[2] << 16) |
           ((Uint32)data[3] << 24);
}

Uint64 SDL_ReadLE64_func(SDL_RWops *src)
{
    Uint8 data[8];
    if (SDL_RWread(src, data, 1, 8) != 8) {
        return 0;
    }
    return (Uint64)data[0] |
           ((Uint64)data[1] << 8) |
           ((Uint64)data[2] << 16) |
           ((Uint64)data[3] << 24) |
           ((Uint64)data[4] << 32) |
           ((Uint64)data[5] << 40) |
           ((Uint64)data[6] << 48) |
           ((Uint64)data[7] << 56);
}

Uint16 SDL_ReadBE16_func(SDL_RWops *src)
{
    Uint8 data[2];
    if (SDL_RWread(src, data, 1, 2) != 2) {
        return 0;
    }
    return ((Uint16)data[0] << 8) | (Uint16)data[1];
}

Uint32 SDL_ReadBE32_func(SDL_RWops *src)
{
    Uint8 data[4];
    if (SDL_RWread(src, data, 1, 4) != 4) {
        return 0;
    }
    return ((Uint32)data[0] << 24) |
           ((Uint32)data[1] << 16) |
           ((Uint32)data[2] << 8) |
           (Uint32)data[3];
}

Uint64 SDL_ReadBE64_func(SDL_RWops *src)
{
    Uint8 data[8];
    if (SDL_RWread(src, data, 1, 8) != 8) {
        return 0;
    }
    return ((Uint64)data[0] << 56) |
           ((Uint64)data[1] << 48) |
           ((Uint64)data[2] << 40) |
           ((Uint64)data[3] << 32) |
           ((Uint64)data[4] << 24) |
           ((Uint64)data[5] << 16) |
           ((Uint64)data[6] << 8) |
           (Uint64)data[7];
}

size_t SDL_WriteLE16_func(SDL_RWops *dst, Uint16 value)
{
    Uint8 data[2];
    data[0] = (Uint8)(value & 0xFF);
    data[1] = (Uint8)((value >> 8) & 0xFF);
    return SDL_RWwrite(dst, data, 1, 2) == 2 ? 1 : 0;
}

size_t SDL_WriteLE32_func(SDL_RWops *dst, Uint32 value)
{
    Uint8 data[4];
    data[0] = (Uint8)(value & 0xFF);
    data[1] = (Uint8)((value >> 8) & 0xFF);
    data[2] = (Uint8)((value >> 16) & 0xFF);
    data[3] = (Uint8)((value >> 24) & 0xFF);
    return SDL_RWwrite(dst, data, 1, 4) == 4 ? 1 : 0;
}

size_t SDL_WriteLE64_func(SDL_RWops *dst, Uint64 value)
{
    Uint8 data[8];
    data[0] = (Uint8)(value & 0xFF);
    data[1] = (Uint8)((value >> 8) & 0xFF);
    data[2] = (Uint8)((value >> 16) & 0xFF);
    data[3] = (Uint8)((value >> 24) & 0xFF);
    data[4] = (Uint8)((value >> 32) & 0xFF);
    data[5] = (Uint8)((value >> 40) & 0xFF);
    data[6] = (Uint8)((value >> 48) & 0xFF);
    data[7] = (Uint8)((value >> 56) & 0xFF);
    return SDL_RWwrite(dst, data, 1, 8) == 8 ? 1 : 0;
}

size_t SDL_WriteBE16_func(SDL_RWops *dst, Uint16 value)
{
    Uint8 data[2];
    data[0] = (Uint8)((value >> 8) & 0xFF);
    data[1] = (Uint8)(value & 0xFF);
    return SDL_RWwrite(dst, data, 1, 2) == 2 ? 1 : 0;
}

size_t SDL_WriteBE32_func(SDL_RWops *dst, Uint32 value)
{
    Uint8 data[4];
    data[0] = (Uint8)((value >> 24) & 0xFF);
    data[1] = (Uint8)((value >> 16) & 0xFF);
    data[2] = (Uint8)((value >> 8) & 0xFF);
    data[3] = (Uint8)(value & 0xFF);
    return SDL_RWwrite(dst, data, 1, 4) == 4 ? 1 : 0;
}

size_t SDL_WriteBE64_func(SDL_RWops *dst, Uint64 value)
{
    Uint8 data[8];
    data[0] = (Uint8)((value >> 56) & 0xFF);
    data[1] = (Uint8)((value >> 48) & 0xFF);
    data[2] = (Uint8)((value >> 40) & 0xFF);
    data[3] = (Uint8)((value >> 32) & 0xFF);
    data[4] = (Uint8)((value >> 24) & 0xFF);
    data[5] = (Uint8)((value >> 16) & 0xFF);
    data[6] = (Uint8)((value >> 8) & 0xFF);
    data[7] = (Uint8)(value & 0xFF);
    return SDL_RWwrite(dst, data, 1, 8) == 8 ? 1 : 0;
}
