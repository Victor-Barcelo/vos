# Chapter 20: User Mode and ELF

User mode (Ring 3) provides memory protection and privilege separation. VOS loads user programs in ELF format.

## Ring 3 Execution

### Privilege Levels

```
Ring 0: Kernel - Full hardware access
Ring 3: User   - Restricted access
```

User mode restrictions:
- Cannot execute privileged instructions (cli, hlt, lgdt, etc.)
- Cannot access I/O ports without permission
- Cannot access kernel memory
- Must use syscalls for kernel services

### User Segments

From the GDT (Chapter 6):

| Selector | Purpose | DPL |
|----------|---------|-----|
| 0x1B | User code | 3 |
| 0x23 | User data | 3 |

### Entering User Mode

```c
void enter_usermode(uint32_t entry, uint32_t stack) {
    // Disable interrupts during setup
    cli();

    // Load user data segments
    __asm__ volatile(
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        ::: "eax"
    );

    // Build iret frame and return to user mode
    __asm__ volatile(
        "push $0x23\n"      // SS
        "push %1\n"         // ESP
        "pushf\n"           // EFLAGS
        "pop %%eax\n"
        "or $0x200, %%eax\n"// Set IF (enable interrupts)
        "push %%eax\n"
        "push $0x1B\n"      // CS
        "push %0\n"         // EIP
        "iret\n"
        :: "r"(entry), "r"(stack)
        : "eax"
    );
}
```

## ELF Format

### ELF Header

```c
typedef struct {
    uint8_t  e_ident[16];   // Magic number and info
    uint16_t e_type;        // Object file type
    uint16_t e_machine;     // Architecture
    uint32_t e_version;     // Object file version
    uint32_t e_entry;       // Entry point address
    uint32_t e_phoff;       // Program header offset
    uint32_t e_shoff;       // Section header offset
    uint32_t e_flags;       // Processor flags
    uint16_t e_ehsize;      // ELF header size
    uint16_t e_phentsize;   // Program header entry size
    uint16_t e_phnum;       // Number of program headers
    uint16_t e_shentsize;   // Section header entry size
    uint16_t e_shnum;       // Number of section headers
    uint16_t e_shstrndx;    // Section name string table index
} Elf32_Ehdr;
```

### ELF Magic

```c
#define ELF_MAGIC       0x7F, 'E', 'L', 'F'
#define ELFCLASS32      1       // 32-bit
#define ELFDATA2LSB     1       // Little endian
#define ET_EXEC         2       // Executable
#define EM_386          3       // x86
```

### Program Header

```c
typedef struct {
    uint32_t p_type;        // Segment type
    uint32_t p_offset;      // Offset in file
    uint32_t p_vaddr;       // Virtual address
    uint32_t p_paddr;       // Physical address (unused)
    uint32_t p_filesz;      // Size in file
    uint32_t p_memsz;       // Size in memory
    uint32_t p_flags;       // Segment flags
    uint32_t p_align;       // Alignment
} Elf32_Phdr;

#define PT_NULL     0       // Unused
#define PT_LOAD     1       // Loadable segment
#define PT_DYNAMIC  2       // Dynamic linking info
#define PT_INTERP   3       // Interpreter path

#define PF_X        0x1     // Execute
#define PF_W        0x2     // Write
#define PF_R        0x4     // Read
```

## ELF Loader

```c
typedef struct {
    uint32_t entry;         // Entry point
    uint32_t stack;         // Initial stack pointer
} elf_load_result_t;

int elf_load(const void *elf_data, uint32_t size, elf_load_result_t *result) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_data;

    // Validate ELF header
    if (memcmp(ehdr->e_ident, "\x7fELF", 4) != 0) {
        return -ENOEXEC;
    }

    if (ehdr->e_ident[4] != ELFCLASS32 ||
        ehdr->e_ident[5] != ELFDATA2LSB ||
        ehdr->e_type != ET_EXEC ||
        ehdr->e_machine != EM_386) {
        return -ENOEXEC;
    }

    // Load program segments
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint8_t *)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        int ret = load_segment(&phdr[i], elf_data);
        if (ret < 0) return ret;
    }

    // Set up user stack
    result->stack = setup_user_stack();
    result->entry = ehdr->e_entry;

    return 0;
}
```

### Loading a Segment

```c
static int load_segment(Elf32_Phdr *phdr, const void *elf_data) {
    uint32_t vaddr = phdr->p_vaddr;
    uint32_t memsz = phdr->p_memsz;
    uint32_t filesz = phdr->p_filesz;
    const void *src = (uint8_t *)elf_data + phdr->p_offset;

    // Validate address range
    if (vaddr < 0x08048000 || vaddr + memsz > 0xC0000000) {
        return -EINVAL;
    }

    // Calculate page flags
    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (phdr->p_flags & PF_W) {
        flags |= PAGE_RW;
    }

    // Allocate and map pages
    uint32_t page_start = vaddr & ~0xFFF;
    uint32_t page_end = (vaddr + memsz + 0xFFF) & ~0xFFF;

    for (uint32_t addr = page_start; addr < page_end; addr += 0x1000) {
        uint32_t phys = pmm_alloc_frame();
        paging_map_page(addr, phys, flags);
    }

    // Copy data
    memcpy((void *)vaddr, src, filesz);

    // Zero BSS
    if (memsz > filesz) {
        memset((void *)(vaddr + filesz), 0, memsz - filesz);
    }

    return 0;
}
```

## User Address Space

```
0xC0000000 +-------------------+
           |    Kernel Space   |
           |   (not accessible)|
0xBFFFF000 +-------------------+
           |    User Stack     |
           |        |          |
           |        v          |
           +-------------------+
           |                   |
           |    (unmapped)     |
           |                   |
           +-------------------+
           |        ^          |
           |        |          |
           |    User Heap      |
           +-------------------+
           |    .bss           |
           +-------------------+
           |    .data          |
           +-------------------+
           |    .text          |
0x08048000 +-------------------+
           |    (reserved)     |
0x00000000 +-------------------+
```

### Stack Setup

```c
#define USER_STACK_TOP  0xBFFFF000
#define USER_STACK_SIZE (8 * 0x1000)  // 32 KB

static uint32_t setup_user_stack(void) {
    uint32_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    // Map stack pages
    for (uint32_t addr = stack_bottom; addr < USER_STACK_TOP; addr += 0x1000) {
        uint32_t phys = pmm_alloc_frame();
        paging_map_page(addr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }

    return USER_STACK_TOP - 4;  // Leave room for return address
}
```

### Heap (sbrk)

```c
int32_t sys_sbrk(int32_t increment) {
    task_t *task = current_task;
    uint32_t old_brk = task->brk;

    if (increment == 0) {
        return old_brk;
    }

    uint32_t new_brk = old_brk + increment;

    if (increment > 0) {
        // Extend heap
        for (uint32_t addr = old_brk & ~0xFFF; addr < new_brk; addr += 0x1000) {
            if (!paging_is_mapped(addr)) {
                uint32_t phys = pmm_alloc_frame();
                paging_map_page(addr, phys,
                               PAGE_PRESENT | PAGE_RW | PAGE_USER);
            }
        }
    }
    // Note: shrinking heap doesn't unmap pages (simple implementation)

    task->brk = new_brk;
    return old_brk;
}
```

## execve Implementation

```c
int32_t sys_execve(const char *path, char *const argv[], char *const envp[]) {
    // Load ELF
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return -ENOENT;

    void *elf_data = kmalloc(node->length);
    vfs_read_file(node, elf_data, node->length);

    elf_load_result_t result;
    int ret = elf_load(elf_data, node->length, &result);
    kfree(elf_data);

    if (ret < 0) return ret;

    // Copy arguments to user stack
    uint32_t sp = result.stack;
    int argc;
    sp = copy_args_to_stack(sp, argv, envp, &argc);

    // Update task
    task_t *task = current_task;

    // Free old memory (except kernel stack)
    free_user_pages(task);

    // Set new program counter and stack
    // This modifies the frame that will be restored on return
    interrupt_frame_t *frame = (interrupt_frame_t *)task->esp;
    frame->eip = result.entry;
    frame->esp = sp;  // User stack
    frame->eax = 0;

    // Reset signals
    for (int i = 0; i < 32; i++) {
        task->signal_handlers[i] = SIG_DFL;
    }

    return 0;  // Return value goes to new program
}
```

### Copying Arguments

```c
static uint32_t copy_args_to_stack(uint32_t sp, char *const argv[],
                                   char *const envp[], int *argc_out) {
    // Count arguments
    int argc = 0;
    int envc = 0;
    if (argv) while (argv[argc]) argc++;
    if (envp) while (envp[envc]) envc++;

    // Copy strings
    char *string_area = (char *)sp;
    char *argv_ptrs[argc + 1];
    char *envp_ptrs[envc + 1];

    for (int i = argc - 1; i >= 0; i--) {
        int len = strlen(argv[i]) + 1;
        string_area -= len;
        memcpy(string_area, argv[i], len);
        argv_ptrs[i] = string_area;
    }
    argv_ptrs[argc] = NULL;

    for (int i = envc - 1; i >= 0; i--) {
        int len = strlen(envp[i]) + 1;
        string_area -= len;
        memcpy(string_area, envp[i], len);
        envp_ptrs[i] = string_area;
    }
    envp_ptrs[envc] = NULL;

    // Align stack
    sp = (uint32_t)string_area & ~0xF;

    // Push envp array
    sp -= (envc + 1) * 4;
    memcpy((void *)sp, envp_ptrs, (envc + 1) * 4);
    uint32_t envp_start = sp;

    // Push argv array
    sp -= (argc + 1) * 4;
    memcpy((void *)sp, argv_ptrs, (argc + 1) * 4);
    uint32_t argv_start = sp;

    // Push envp, argv, argc
    sp -= 4; *(uint32_t *)sp = envp_start;
    sp -= 4; *(uint32_t *)sp = argv_start;
    sp -= 4; *(uint32_t *)sp = argc;

    *argc_out = argc;
    return sp;
}
```

## User-Space Runtime

### crt0.asm

```nasm
section .text
global _start
extern main
extern exit

_start:
    ; Stack: argc, argv, envp
    pop eax         ; argc
    mov ebx, esp    ; argv
    lea ecx, [ebx + eax*4 + 4]  ; envp

    ; Call main(argc, argv, envp)
    push ecx
    push ebx
    push eax
    call main
    add esp, 12

    ; Call exit(return_value)
    push eax
    call exit
    ; Never returns
```

### Syscall Wrappers

```c
// user/syscall.h
static inline int32_t syscall3(int num, int arg1, int arg2, int arg3) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return result;
}

#define write(fd, buf, len) syscall3(SYS_WRITE, fd, (int)buf, len)
#define read(fd, buf, len)  syscall3(SYS_READ, fd, (int)buf, len)
#define exit(code)          syscall1(SYS_EXIT, code)
```

## Summary

VOS user mode provides:

1. **Ring 3 execution** for memory protection
2. **ELF32 loading** for standard executable format
3. **Separate address space** per process
4. **Stack and heap** management
5. **execve** for program replacement
6. **C runtime** for user programs

This enables running real programs with proper isolation.

---

*Previous: [Chapter 19: Tasking and Scheduling](19_tasking.md)*
*Next: [Chapter 21: System Calls](21_syscalls.md)*
