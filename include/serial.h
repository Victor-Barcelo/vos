#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

void serial_init(void);
bool serial_is_initialized(void);
void serial_write_char(char c);
void serial_write_string(const char* str);
void serial_write_hex(uint32_t value);
void serial_write_dec(int32_t value);

#endif
