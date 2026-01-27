/*
 * SDL_rwops.h - VOS minimal SDL2 shim
 * Read/Write operations for file and memory I/O
 *
 * This provides SDL2-compatible RWops for file operations,
 * commonly used by applications like Klystrack for save/load.
 */

#ifndef SDL_rwops_h_
#define SDL_rwops_h_

#include "SDL_stdinc.h"

/* Seek from beginning of data */
#define RW_SEEK_SET 0
/* Seek relative to current read point */
#define RW_SEEK_CUR 1
/* Seek relative to end of data */
#define RW_SEEK_END 2

/* RWops type values */
#define SDL_RWOPS_UNKNOWN   0
#define SDL_RWOPS_WINFILE   1
#define SDL_RWOPS_STDFILE   2
#define SDL_RWOPS_JNIFILE   3
#define SDL_RWOPS_MEMORY    4
#define SDL_RWOPS_MEMORY_RO 5

/*
 * SDL_RWops - Abstract interface for reading/writing data
 *
 * This structure provides function pointers for I/O operations,
 * allowing unified access to files, memory, and other data sources.
 */
typedef struct SDL_RWops {
    /*
     * Return the size of the data, or -1 if unknown
     */
    Sint64 (*size)(struct SDL_RWops *context);

    /*
     * Seek to offset relative to whence (RW_SEEK_SET/CUR/END)
     * Returns the new offset, or -1 on error
     */
    Sint64 (*seek)(struct SDL_RWops *context, Sint64 offset, int whence);

    /*
     * Read up to maxnum objects of size bytes each
     * Returns the number of objects read, or 0 on error/EOF
     */
    size_t (*read)(struct SDL_RWops *context, void *ptr, size_t size, size_t maxnum);

    /*
     * Write exactly num objects of size bytes each
     * Returns the number of objects written, or 0 on error
     */
    size_t (*write)(struct SDL_RWops *context, const void *ptr, size_t size, size_t num);

    /*
     * Close and free the context
     * Returns 0 on success, -1 on error
     */
    int (*close)(struct SDL_RWops *context);

    /* Type of RWops */
    Uint32 type;

    /*
     * Hidden implementation data, compatible with SDL2 structure.
     * Applications may use hidden.unknown.data1 and data2 for custom types.
     */
    union {
        struct {
            void *data1;
            void *data2;
        } unknown;
        struct {
            void *autoclose;
            void *fp;
        } stdio;
        struct {
            Uint8 *base;
            Uint8 *here;
            Uint8 *stop;
        } mem;
    } hidden;
} SDL_RWops;

/*
 * SDL_RWFromFile() - Create an RWops from a file
 *
 * file: Path to the file
 * mode: fopen-style mode string (e.g., "rb", "wb", "r+b")
 *
 * Returns a new SDL_RWops, or NULL on error.
 */
SDL_RWops* SDL_RWFromFile(const char *file, const char *mode);

/*
 * SDL_RWFromMem() - Create an RWops from a memory buffer
 *
 * mem: Pointer to memory buffer (must remain valid while RWops is in use)
 * size: Size of the memory buffer
 *
 * Returns a new SDL_RWops, or NULL on error.
 * The buffer is read/write accessible.
 */
SDL_RWops* SDL_RWFromMem(void *mem, int size);

/*
 * SDL_RWFromConstMem() - Create an RWops from a read-only memory buffer
 *
 * mem: Pointer to memory buffer (must remain valid while RWops is in use)
 * size: Size of the memory buffer
 *
 * Returns a new SDL_RWops, or NULL on error.
 * Write operations will fail on this RWops.
 */
SDL_RWops* SDL_RWFromConstMem(const void *mem, int size);

/*
 * SDL_RWFromFP() - Create an RWops from a FILE pointer
 *
 * fp: FILE pointer (must remain valid while RWops is in use)
 * autoclose: If non-zero, fclose() will be called on the FILE when closed
 *
 * Returns a new SDL_RWops, or NULL on error.
 */
SDL_RWops* SDL_RWFromFP(void *fp, int autoclose);

/*
 * SDL_AllocRW() - Allocate an empty RWops structure
 *
 * Returns a new SDL_RWops, or NULL on error.
 * The structure is zeroed; you must fill in the function pointers.
 */
SDL_RWops* SDL_AllocRW(void);

/*
 * SDL_FreeRW() - Free an RWops structure allocated by SDL_AllocRW
 *
 * This does NOT call the close function; use SDL_RWclose for that.
 */
void SDL_FreeRW(SDL_RWops *area);

/*
 * SDL_RWclose() - Close and free an RWops
 *
 * context: The RWops to close
 *
 * Returns 0 on success, -1 on error.
 */
int SDL_RWclose(SDL_RWops *context);

/*
 * SDL_RWread() - Read from an RWops
 *
 * context: The RWops to read from
 * ptr: Destination buffer
 * size: Size of each object to read
 * maxnum: Maximum number of objects to read
 *
 * Returns the number of objects read.
 */
size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum);

/*
 * SDL_RWwrite() - Write to an RWops
 *
 * context: The RWops to write to
 * ptr: Source buffer
 * size: Size of each object to write
 * num: Number of objects to write
 *
 * Returns the number of objects written.
 */
size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t num);

/*
 * SDL_RWseek() - Seek within an RWops
 *
 * context: The RWops
 * offset: Offset in bytes
 * whence: RW_SEEK_SET, RW_SEEK_CUR, or RW_SEEK_END
 *
 * Returns the new position, or -1 on error.
 */
Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence);

/*
 * SDL_RWtell() - Get the current position in an RWops
 *
 * Returns the current position, or -1 on error.
 */
Sint64 SDL_RWtell(SDL_RWops *context);

/*
 * SDL_RWsize() - Get the size of an RWops
 *
 * Returns the size in bytes, or -1 if unknown.
 */
Sint64 SDL_RWsize(SDL_RWops *context);

/*
 * Endian read/write functions for little-endian data
 * These are commonly used for binary file formats.
 */

/* Read a 16-bit little-endian value */
Uint16 SDL_ReadLE16_func(SDL_RWops *src);

/* Read a 32-bit little-endian value */
Uint32 SDL_ReadLE32_func(SDL_RWops *src);

/* Read a 64-bit little-endian value */
Uint64 SDL_ReadLE64_func(SDL_RWops *src);

/* Read a 16-bit big-endian value */
Uint16 SDL_ReadBE16_func(SDL_RWops *src);

/* Read a 32-bit big-endian value */
Uint32 SDL_ReadBE32_func(SDL_RWops *src);

/* Read a 64-bit big-endian value */
Uint64 SDL_ReadBE64_func(SDL_RWops *src);

/* Write a 16-bit little-endian value */
size_t SDL_WriteLE16_func(SDL_RWops *dst, Uint16 value);

/* Write a 32-bit little-endian value */
size_t SDL_WriteLE32_func(SDL_RWops *dst, Uint32 value);

/* Write a 64-bit little-endian value */
size_t SDL_WriteLE64_func(SDL_RWops *dst, Uint64 value);

/* Write a 16-bit big-endian value */
size_t SDL_WriteBE16_func(SDL_RWops *dst, Uint16 value);

/* Write a 32-bit big-endian value */
size_t SDL_WriteBE32_func(SDL_RWops *dst, Uint32 value);

/* Write a 64-bit big-endian value */
size_t SDL_WriteBE64_func(SDL_RWops *dst, Uint64 value);

/* Convenience macros matching SDL2 API */
#define SDL_ReadLE16(src)       SDL_ReadLE16_func(src)
#define SDL_ReadLE32(src)       SDL_ReadLE32_func(src)
#define SDL_ReadLE64(src)       SDL_ReadLE64_func(src)
#define SDL_ReadBE16(src)       SDL_ReadBE16_func(src)
#define SDL_ReadBE32(src)       SDL_ReadBE32_func(src)
#define SDL_ReadBE64(src)       SDL_ReadBE64_func(src)
#define SDL_WriteLE16(dst, val) SDL_WriteLE16_func(dst, val)
#define SDL_WriteLE32(dst, val) SDL_WriteLE32_func(dst, val)
#define SDL_WriteLE64(dst, val) SDL_WriteLE64_func(dst, val)
#define SDL_WriteBE16(dst, val) SDL_WriteBE16_func(dst, val)
#define SDL_WriteBE32(dst, val) SDL_WriteBE32_func(dst, val)
#define SDL_WriteBE64(dst, val) SDL_WriteBE64_func(dst, val)

/* Read 8-bit value (trivial but for completeness) */
Uint8 SDL_ReadU8(SDL_RWops *src);

/* Write 8-bit value */
size_t SDL_WriteU8(SDL_RWops *dst, Uint8 value);

#endif /* SDL_rwops_h_ */
