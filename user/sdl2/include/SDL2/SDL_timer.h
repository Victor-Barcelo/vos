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

/* Timer callback types */
typedef Uint32 (*SDL_TimerCallback)(Uint32 interval, void *param);
typedef int SDL_TimerID;

/**
 * Add a timer callback function.
 *
 * NOTE: VOS has no threading, so timers are NOT supported.
 * This function always returns 0 (failure).
 * Applications should use SDL_GetTicks() in their main loop instead.
 *
 * @param interval The timer delay (ms) passed to callback
 * @param callback The timer callback function
 * @param param A pointer passed to the callback
 * @returns A timer ID, or 0 if timer creation failed.
 */
SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void *param);

/**
 * Remove a timer created with SDL_AddTimer().
 *
 * NOTE: VOS has no threading, so timers are NOT supported.
 * This function is a no-op stub.
 *
 * @param id The timer ID to remove
 * @returns SDL_TRUE if timer was removed, SDL_FALSE if not found.
 */
int SDL_RemoveTimer(SDL_TimerID id);

#endif /* SDL_TIMER_H */
