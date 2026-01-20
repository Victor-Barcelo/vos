#ifndef ELF_H
#define ELF_H

#include "types.h"

// Loads an ELF32 i386 image into the current address space as user-accessible
// pages and prepares a user stack. Returns the entry point and initial user ESP.
bool elf_load_user_image(const uint8_t* image, uint32_t size, uint32_t* out_entry, uint32_t* out_user_esp);

#endif

