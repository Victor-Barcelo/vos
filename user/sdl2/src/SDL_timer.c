/*
 * SDL_timer.c - SDL2 Timer Subsystem Implementation for VOS
 *
 * Uses VOS syscalls for timing functionality:
 * - sys_uptime_ms() for getting milliseconds since boot
 * - sys_nanosleep() for precise delays
 */

#include "SDL2/SDL_timer.h"
#include "syscall.h"
#include <stddef.h>

/* Ticks value at SDL initialization time */
static Uint32 sdl_start_ticks = 0;

/**
 * Initialize the timer subsystem.
 * Records the starting tick count for relative time calculations.
 */
void SDL_Timer_Init(void) {
    sdl_start_ticks = sys_uptime_ms();
}

/**
 * Get milliseconds since SDL initialization (32-bit).
 */
Uint32 SDL_GetTicks(void) {
    return sys_uptime_ms() - sdl_start_ticks;
}

/**
 * Get milliseconds since SDL initialization (64-bit).
 */
Uint64 SDL_GetTicks64(void) {
    return (Uint64)(sys_uptime_ms() - sdl_start_ticks);
}

/**
 * Delay execution for a specified number of milliseconds.
 * Uses VOS nanosleep syscall for precise timing.
 */
void SDL_Delay(Uint32 ms) {
    vos_timespec_t req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    sys_nanosleep(&req, NULL);
}

/**
 * Get high resolution performance counter.
 * Returns milliseconds since boot (VOS resolution).
 */
Uint64 SDL_GetPerformanceCounter(void) {
    return (Uint64)sys_uptime_ms();
}

/**
 * Get performance counter frequency.
 * Returns 1000 since VOS provides millisecond resolution.
 */
Uint64 SDL_GetPerformanceFrequency(void) {
    return 1000;  /* millisecond resolution */
}
