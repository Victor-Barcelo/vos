// vos_gamedev.h - VOS Game Development Helper Header
// Include this BEFORE any gamedev library to ensure TCC/VOS compatibility
#ifndef VOS_GAMEDEV_H
#define VOS_GAMEDEV_H

// ============================================================================
// VOS Platform Detection
// ============================================================================
// Define __VOS__ when compiling inside VOS with TCC
// TCC inside VOS should define this, or you can define it manually
#if defined(__TINYC__) && !defined(__linux__) && !defined(_WIN32)
# ifndef __VOS__
#  define __VOS__
# endif
#endif

// ============================================================================
// TCC/VOS Compatibility Defines
// ============================================================================

// Disable SIMD in HandmadeMath (TCC doesn't support intrinsics)
// NOTE: HandmadeMath.h now auto-detects __TINYC__ and __VOS__
#ifndef HANDMADE_MATH_NO_SIMD
#define HANDMADE_MATH_NO_SIMD
#endif

// stb_image_resize2.h auto-detects TCC and disables SIMD - no action needed

// Physac standalone mode (no raylib dependency)
#define PHYSAC_STANDALONE

// Disable threading in Physac (VOS doesn't have pthreads)
#define PHYSAC_NO_THREADS

// NOTE: physac.h has been patched to support __VOS__ natively

// ============================================================================
// Standard includes commonly needed
// ============================================================================
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// VOS-specific helpers
// ============================================================================

// Simple millisecond timer using VOS syscalls
// Usage: double start = vos_get_time_ms(); ... double elapsed = vos_get_time_ms() - start;
#ifdef __VOS__
// VOS syscall declaration (also in syscall.h)
extern unsigned int sys_uptime(void);

static inline double vos_get_time_ms(void) {
    // VOS sys_uptime returns milliseconds since boot
    return (double)sys_uptime();
}
#else
// Fallback for testing on host (Linux/macOS)
#include <time.h>
static inline double vos_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}
#endif

// ============================================================================
// Physac time provider - NOT needed anymore!
// ============================================================================
// physac.h has been patched to natively support __VOS__
// It will automatically use sys_uptime() when compiled on VOS

// ============================================================================
// Memory allocation wrappers (customize if needed)
// ============================================================================
#ifndef VOS_MALLOC
#define VOS_MALLOC(sz) malloc(sz)
#endif
#ifndef VOS_FREE
#define VOS_FREE(ptr) free(ptr)
#endif
#ifndef VOS_REALLOC
#define VOS_REALLOC(ptr, sz) realloc(ptr, sz)
#endif

// ============================================================================
// Quick reference - how to use each library:
// ============================================================================
/*
MATH:
    #include "vos_gamedev.h"
    #include "linmath.h"        // or hypatia.h or HandmadeMath.h

PHYSICS:
    #include "vos_gamedev.h"
    #define PHYSAC_IMPLEMENTATION
    #include "physac.h"

COLLISION:
    #include "vos_gamedev.h"
    #define SATC_IMPLEMENTATION
    #include "satc.h"
    // or
    #define SR_RESOLVE_IMPLEMENTATION
    #include "sr_resolve.h"

ECS:
    #include "vos_gamedev.h"
    #define ECS_IMPLEMENTATION
    #include "ecs.h"

FSM:
    #include "vos_gamedev.h"
    #include "sm.h"             // simplest
    // or stateMachine.h, hsm.h

PATHFINDING:
    #include "vos_gamedev.h"
    #include "AStar.h"
    // link with AStar.c

EASING:
    #include "vos_gamedev.h"
    #include "easing.h"
    // link with easing.c

RANDOM:
    #include "vos_gamedev.h"
    #include "pcg_basic.h"
    // link with pcg_basic.c

NOISE:
    #include "vos_gamedev.h"
    #define STB_PERLIN_IMPLEMENTATION
    #include "stb_perlin.h"

DATA STRUCTURES:
    #include "vos_gamedev.h"
    #define STB_DS_IMPLEMENTATION
    #include "stb_ds.h"
*/

#endif // VOS_GAMEDEV_H
