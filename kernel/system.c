#include "system.h"
#include "multiboot.h"
#include "string.h"

static uint32_t mem_total_kb = 0;
static char cpu_vendor[13] = "unknown";
static char cpu_brand[49] = "";

static bool cpuid_supported(void) {
    uint32_t old_flags;
    uint32_t new_flags;
    __asm__ volatile ("pushfl; popl %0" : "=r"(old_flags));
    uint32_t toggled = old_flags ^ (1u << 21);
    __asm__ volatile ("pushl %0; popfl" : : "r"(toggled) : "cc");
    __asm__ volatile ("pushfl; popl %0" : "=r"(new_flags));
    __asm__ volatile ("pushl %0; popfl" : : "r"(old_flags) : "cc");
    return ((new_flags ^ old_flags) & (1u << 21)) != 0;
}

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(subleaf)
    );
    if (a) *a = eax;
    if (b) *b = ebx;
    if (c) *c = ecx;
    if (d) *d = edx;
}

static void init_cpu_strings(void) {
    strcpy(cpu_vendor, "unknown");
    cpu_brand[0] = '\0';

    if (!cpuid_supported()) {
        return;
    }

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);

    memcpy(cpu_vendor + 0, &ebx, 4);
    memcpy(cpu_vendor + 4, &edx, 4);
    memcpy(cpu_vendor + 8, &ecx, 4);
    cpu_vendor[12] = '\0';

    uint32_t max_ext;
    cpuid(0x80000000u, 0, &max_ext, &ebx, &ecx, &edx);
    if (max_ext < 0x80000004u) {
        return;
    }

    uint32_t a, b, c, d;
    cpuid(0x80000002u, 0, &a, &b, &c, &d);
    memcpy(cpu_brand + 0, &a, 4);
    memcpy(cpu_brand + 4, &b, 4);
    memcpy(cpu_brand + 8, &c, 4);
    memcpy(cpu_brand + 12, &d, 4);

    cpuid(0x80000003u, 0, &a, &b, &c, &d);
    memcpy(cpu_brand + 16, &a, 4);
    memcpy(cpu_brand + 20, &b, 4);
    memcpy(cpu_brand + 24, &c, 4);
    memcpy(cpu_brand + 28, &d, 4);

    cpuid(0x80000004u, 0, &a, &b, &c, &d);
    memcpy(cpu_brand + 32, &a, 4);
    memcpy(cpu_brand + 36, &b, 4);
    memcpy(cpu_brand + 40, &c, 4);
    memcpy(cpu_brand + 44, &d, 4);
    cpu_brand[48] = '\0';
}

void system_init(uint32_t multiboot_magic, uint32_t* mboot_info) {
    init_cpu_strings();

    mem_total_kb = 0;
    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || !mboot_info) {
        return;
    }

    const multiboot_info_t* mbi = (const multiboot_info_t*)mboot_info;
    if ((mbi->flags & 0x1u) == 0) {
        return;
    }

    mem_total_kb = mbi->mem_lower + mbi->mem_upper;
}

uint32_t system_mem_total_kb(void) {
    return mem_total_kb;
}

const char* system_cpu_vendor(void) {
    return cpu_vendor;
}

const char* system_cpu_brand(void) {
    return cpu_brand;
}
