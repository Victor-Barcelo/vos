#ifndef SDL_keyboard_h_
#define SDL_keyboard_h_

#include "SDL_stdinc.h"
#include "SDL_keycode.h"
#include "SDL_scancode.h"

/**
 * SDL keyboard interface for VOS
 */

/**
 * Keysym structure - describes a key press/release
 */
typedef struct SDL_Keysym {
    SDL_Scancode scancode;  /**< SDL physical key code */
    SDL_Keycode sym;        /**< SDL virtual key code */
    Uint16 mod;             /**< current key modifiers */
    Uint32 unused;
} SDL_Keysym;

/**
 * Get the current key modifier state.
 *
 * @return An OR'd combination of the modifier keys for the keyboard.
 */
SDL_Keymod SDL_GetModState(void);

/**
 * Set the current key modifier state.
 *
 * @param modstate the desired SDL_Keymod for the keyboard
 */
void SDL_SetModState(SDL_Keymod modstate);

/**
 * Get a snapshot of the current state of the keyboard.
 *
 * @param numkeys if non-NULL, receives the length of the returned array
 * @return a pointer to an array of key states
 */
const Uint8 *SDL_GetKeyboardState(int *numkeys);

/**
 * Get the key code corresponding to the given scancode.
 *
 * @param scancode the desired SDL_Scancode
 * @return the SDL_Keycode that corresponds to the given SDL_Scancode
 */
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode);

/**
 * Get the scancode corresponding to the given key code.
 *
 * @param key the desired SDL_Keycode
 * @return the SDL_Scancode that corresponds to the given SDL_Keycode
 */
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key);

/**
 * Get a human-readable name for a scancode.
 *
 * @param scancode the desired SDL_Scancode
 * @return a pointer to the name of the scancode
 */
const char *SDL_GetScancodeName(SDL_Scancode scancode);

/**
 * Get a human-readable name for a key.
 *
 * @param key the desired SDL_Keycode
 * @return a pointer to the name of the key
 */
const char *SDL_GetKeyName(SDL_Keycode key);

#endif /* SDL_keyboard_h_ */
