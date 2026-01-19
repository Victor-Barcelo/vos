#ifndef STRING_H
#define STRING_H

#include "types.h"

// Get string length
size_t strlen(const char* str);

// Compare two strings
int strcmp(const char* s1, const char* s2);

// Compare first n characters
int strncmp(const char* s1, const char* s2, size_t n);

// Copy string
char* strcpy(char* dest, const char* src);

// Copy up to n characters
char* strncpy(char* dest, const char* src, size_t n);

// Find first occurrence of character
char* strchr(const char* str, int c);

// Set memory
void* memset(void* ptr, int value, size_t num);

// Copy memory
void* memcpy(void* dest, const void* src, size_t num);

#endif
