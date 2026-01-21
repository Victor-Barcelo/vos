#ifndef USER_STDIO_H
#define USER_STDIO_H

#include <sys/types.h>
#include_next <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// newlib for bare-metal targets ships internal __getdelim/__getline but does not
// expose the POSIX names. Provide the standard prototypes for portability.
ssize_t getdelim(char **__restrict lineptr, size_t *__restrict n, int delim, FILE *__restrict stream);
ssize_t getline(char **__restrict lineptr, size_t *__restrict n, FILE *__restrict stream);

#ifdef __cplusplus
}
#endif

#endif

