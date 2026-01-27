/*
 * SDL_timer.h - SDL2 Timer Subsystem for VOS
 *
 * Minimal SDL2 shim implementation using VOS syscalls.
 */

#ifndef SDL_TIMER_H
#define SDL_TIMER_H

#include <stdint.h>

/* SDL type definitions */
typedef uint32_t Uint32;
typedef uint64_t Uint64;

/**
 * Initialize the timer subsystem (internal use).
 * Called during SDL_Init().
 */
void SDL_Timer_Init(void);

/**
 * Get the number of milliseconds since SDL library initialization.
 *
 * This value wraps if the program runs for more than ~49 days.
 *
 * @returns An unsigned 32-bit value representing the number of milliseconds
 *          since the SDL library initialized.
 */
Uint32 SDL_GetTicks(void);

/**
 * Get the number of milliseconds since SDL library initialization.
 *
 * 64-bit version that doesn't wrap for ~584 million years.
 *
 * @returns An unsigned 64-bit value representing the number of milliseconds
 *          since the SDL library initialized.
 */
Uint64 SDL_GetTicks64(void);

/**
 * Wait a specified number of milliseconds before returning.
 *
 * This function waits at least the specified time, but possibly longer due
 * to OS scheduling.
 *
 * @param ms The number of milliseconds to delay.
 */
void SDL_Delay(Uint32 ms);

/**
 * Get the current value of the high resolution counter.
 *
 * This function is typically used for profiling.
 *
 * The counter values are only meaningful relative to each other. Differences
 * between values can be converted to times by using
 * SDL_GetPerformanceFrequency().
 *
 * @returns The current counter value.
 */
Uint64 SDL_GetPerformanceCounter(void);

/**
 * Get the count per second of the high resolution counter.
 *
 * @returns A platform-specific count per second.
 */
Uint64 SDL_GetPerformanceFrequency(void);

#endif /* SDL_TIMER_H */
