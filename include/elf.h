#ifndef ELF_H
#define ELF_H

#include "types.h"

// Loads an ELF32 i386 image into the current address space as user-accessible
// pages and prepares a user stack. Returns the entry point and initial user ESP.
bool elf_load_user_image(const uint8_t* image, uint32_t size, uint32_t* out_entry, uint32_t* out_user_esp, uint32_t* out_brk);

// Builds an initial process stack with argc/argv/envp (envp is empty),
// starting from `*inout_user_esp` (typically the stack top). On success,
// updates `*inout_user_esp` to the new stack pointer (points at argc).
// Must be called while the user page directory is active.
bool elf_setup_user_stack(uint32_t* inout_user_esp, const char* const* argv, uint32_t argc);

#endif
