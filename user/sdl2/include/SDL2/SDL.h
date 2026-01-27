/*
 * SDL.h - VOS minimal SDL2 shim
 * Main SDL2 header - includes all other SDL headers
 *
 * This is a minimal SDL2-compatible API for VOS.
 * It provides the core SDL2 types and functions needed for
 * basic application compatibility.
 */

#ifndef SDL_h_
#define SDL_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_video.h"
#include "SDL_render.h"
#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_keyboard.h"
#include "SDL_mutex.h"
#include "SDL_rwops.h"
#include "SDL_endian.h"

/*
 * SDL initialization flags
 * These can be OR'd together to initialize multiple subsystems
 */
#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u
#define SDL_INIT_JOYSTICK       0x00000200u
#define SDL_INIT_HAPTIC         0x00001000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_INIT_EVENTS         0x00004000u
#define SDL_INIT_SENSOR         0x00008000u
#define SDL_INIT_NOPARACHUTE    0x00100000u  /* Compatibility, ignored */

/* Initialize everything */
#define SDL_INIT_EVERYTHING ( \
    SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | \
    SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | \
    SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR )

/*
 * SDL_Init() - Initialize the SDL library
 *
 * This function initializes the subsystems specified by flags.
 *
 * flags: A mask of the subsystems to initialize (SDL_INIT_* flags)
 *
 * Returns 0 on success, -1 on error.
 * Call SDL_GetError() for more information on failure.
 */
int SDL_Init(Uint32 flags);

/*
 * SDL_InitSubSystem() - Initialize specific SDL subsystems
 *
 * After SDL_Init() has been called, this function can initialize
 * additional subsystems.
 *
 * flags: A mask of the subsystems to initialize
 *
 * Returns 0 on success, -1 on error.
 */
int SDL_InitSubSystem(Uint32 flags);

/*
 * SDL_QuitSubSystem() - Shut down specific SDL subsystems
 *
 * flags: A mask of the subsystems to shut down
 */
void SDL_QuitSubSystem(Uint32 flags);

/*
 * SDL_WasInit() - Check which subsystems are initialized
 *
 * flags: A mask of subsystems to check, or 0 to check all
 *
 * Returns a mask of the initialized subsystems.
 * If flags is 0, returns the mask of all initialized subsystems.
 */
Uint32 SDL_WasInit(Uint32 flags);

/*
 * SDL_Quit() - Clean up all initialized subsystems
 *
 * You should call this function upon all exit conditions.
 */
void SDL_Quit(void);

/*
 * SDL version structure
 */
typedef struct SDL_version {
    Uint8 major;        /* major version */
    Uint8 minor;        /* minor version */
    Uint8 patch;        /* update version */
} SDL_version;

/* VOS SDL shim version - based on SDL 2.0 API */
#define SDL_MAJOR_VERSION   2
#define SDL_MINOR_VERSION   0
#define SDL_PATCHLEVEL      0

/* Macro to fill a version structure */
#define SDL_VERSION(x) \
    do { \
        (x)->major = SDL_MAJOR_VERSION; \
        (x)->minor = SDL_MINOR_VERSION; \
        (x)->patch = SDL_PATCHLEVEL; \
    } while (0)

/* Version number for compile-time checks */
#define SDL_VERSIONNUM(X, Y, Z) ((X) * 1000 + (Y) * 100 + (Z))
#define SDL_COMPILEDVERSION SDL_VERSIONNUM(SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL)
#define SDL_VERSION_ATLEAST(X, Y, Z) (SDL_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))

/*
 * SDL_GetVersion() - Get the version of SDL that is linked against
 */
void SDL_GetVersion(SDL_version *ver);

/*
 * SDL_GetRevision() - Get the code revision of SDL
 *
 * Returns the revision string (for VOS this returns a static string)
 */
const char *SDL_GetRevision(void);

/*
 * SDL_GetRevisionNumber() - Get the revision number
 *
 * Deprecated in SDL2, returns 0 for VOS.
 */
int SDL_GetRevisionNumber(void);

/*
 * SDL_getenv() / SDL_setenv() - Environment variable functions
 *
 * These wrap standard C getenv/setenv for compatibility.
 */
char *SDL_getenv(const char *name);
int SDL_setenv(const char *name, const char *value, int overwrite);

/*
 * SDL_GetBasePath() - Get the directory where the application was run from
 *
 * Returns a static string "/bin/" for VOS (binaries are in /bin).
 * Note: Unlike standard SDL2, caller should NOT free this.
 */
char *SDL_GetBasePath(void);

/*
 * SDL_GetPrefPath() - Get the user's preference directory
 *
 * VOS stub - returns NULL. Apps should manage their own config paths.
 */
char *SDL_GetPrefPath(const char *org, const char *app);

/*
 * SDL_SetWindowIcon() - Set window icon
 *
 * VOS stub - window icons are not supported.
 */
void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);

/*
 * Message box flags
 */
#define SDL_MESSAGEBOX_ERROR        0x00000010
#define SDL_MESSAGEBOX_WARNING      0x00000020
#define SDL_MESSAGEBOX_INFORMATION  0x00000040

/*
 * SDL_ShowSimpleMessageBox() - Display a simple modal message box
 *
 * VOS stub - message boxes are printed to stderr.
 *
 * flags: SDL_MESSAGEBOX_* flags
 * title: UTF-8 title text
 * message: UTF-8 message text
 * window: Parent window (can be NULL)
 *
 * Returns 0 on success, -1 on failure.
 */
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title,
                             const char *message, SDL_Window *window);

#endif /* SDL_h_ */
