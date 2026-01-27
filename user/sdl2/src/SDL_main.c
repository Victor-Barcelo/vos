/*
 * SDL_main.c - VOS minimal SDL2 shim main initialization
 *
 * Handles SDL_Init, SDL_Quit, and subsystem management.
 * Coordinates initialization of all SDL subsystems:
 * - Video (SDL_video.c)
 * - Audio (SDL_audio.c)
 * - Events (SDL_events.c)
 * - Timer (SDL_timer.c)
 */

#include "SDL2/SDL.h"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_error.h"
#include <string.h>
#include <stdint.h>
#include "syscall.h"

/* Track which subsystems are initialized */
static Uint32 sdl_initialized_subsystems = 0;

/* Error message storage */
static char sdl_error_msg[256] = "";

/* Forward declarations for subsystem init/quit functions */
extern int SDL_VideoInit(const char *driver_name);
extern void SDL_VideoQuit(void);
extern int SDL_EventsInit(void);
extern void SDL_EventsQuit(void);

/* Timer subsystem (simple implementation) */
static int timer_initialized = 0;

static int SDL_TimerInit(void) {
    timer_initialized = 1;
    return 0;
}

static void SDL_TimerQuit(void) {
    timer_initialized = 0;
}

/* Audio subsystem is initialized on-demand by SDL_OpenAudio */
static int audio_subsystem_initialized = 0;

static int SDL_AudioSubsystemInit(void) {
    audio_subsystem_initialized = 1;
    return 0;
}

static void SDL_AudioSubsystemQuit(void) {
    audio_subsystem_initialized = 0;
}

int SDL_Init(Uint32 flags) {
    int result = 0;

    /* Initialize requested subsystems */
    if (flags & SDL_INIT_TIMER) {
        if (SDL_TimerInit() == 0) {
            sdl_initialized_subsystems |= SDL_INIT_TIMER;
        } else {
            result = -1;
        }
    }

    if (flags & SDL_INIT_EVENTS) {
        if (SDL_EventsInit() == 0) {
            sdl_initialized_subsystems |= SDL_INIT_EVENTS;
        } else {
            result = -1;
        }
    }

    if (flags & SDL_INIT_VIDEO) {
        /* Video requires events */
        if (!(sdl_initialized_subsystems & SDL_INIT_EVENTS)) {
            if (SDL_EventsInit() == 0) {
                sdl_initialized_subsystems |= SDL_INIT_EVENTS;
            }
        }

        if (SDL_VideoInit(NULL) == 0) {
            sdl_initialized_subsystems |= SDL_INIT_VIDEO;
        } else {
            result = -1;
        }
    }

    if (flags & SDL_INIT_AUDIO) {
        if (SDL_AudioSubsystemInit() == 0) {
            sdl_initialized_subsystems |= SDL_INIT_AUDIO;
        } else {
            result = -1;
        }
    }

    /* Joystick, haptic, game controller, sensor are not supported */
    /* Just mark them as "initialized" for compatibility */
    if (flags & SDL_INIT_JOYSTICK) {
        sdl_initialized_subsystems |= SDL_INIT_JOYSTICK;
    }
    if (flags & SDL_INIT_HAPTIC) {
        sdl_initialized_subsystems |= SDL_INIT_HAPTIC;
    }
    if (flags & SDL_INIT_GAMECONTROLLER) {
        sdl_initialized_subsystems |= SDL_INIT_GAMECONTROLLER;
    }
    if (flags & SDL_INIT_SENSOR) {
        sdl_initialized_subsystems |= SDL_INIT_SENSOR;
    }

    return result;
}

int SDL_InitSubSystem(Uint32 flags) {
    return SDL_Init(flags);
}

void SDL_QuitSubSystem(Uint32 flags) {
    if (flags & SDL_INIT_VIDEO) {
        if (sdl_initialized_subsystems & SDL_INIT_VIDEO) {
            SDL_VideoQuit();
            sdl_initialized_subsystems &= ~SDL_INIT_VIDEO;
        }
    }

    if (flags & SDL_INIT_AUDIO) {
        if (sdl_initialized_subsystems & SDL_INIT_AUDIO) {
            SDL_AudioSubsystemQuit();
            sdl_initialized_subsystems &= ~SDL_INIT_AUDIO;
        }
    }

    if (flags & SDL_INIT_EVENTS) {
        if (sdl_initialized_subsystems & SDL_INIT_EVENTS) {
            SDL_EventsQuit();
            sdl_initialized_subsystems &= ~SDL_INIT_EVENTS;
        }
    }

    if (flags & SDL_INIT_TIMER) {
        if (sdl_initialized_subsystems & SDL_INIT_TIMER) {
            SDL_TimerQuit();
            sdl_initialized_subsystems &= ~SDL_INIT_TIMER;
        }
    }

    /* Clear unsupported subsystem flags */
    sdl_initialized_subsystems &= ~(flags & (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC |
                                              SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR));
}

Uint32 SDL_WasInit(Uint32 flags) {
    if (flags == 0) {
        return sdl_initialized_subsystems;
    }
    return sdl_initialized_subsystems & flags;
}

void SDL_Quit(void) {
    /* Quit all subsystems in reverse order */
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    SDL_QuitSubSystem(SDL_INIT_TIMER);

    sdl_initialized_subsystems = 0;
}

void SDL_GetVersion(SDL_version *ver) {
    if (ver) {
        ver->major = SDL_MAJOR_VERSION;
        ver->minor = SDL_MINOR_VERSION;
        ver->patch = SDL_PATCHLEVEL;
    }
}

const char *SDL_GetRevision(void) {
    return "VOS-SDL2-shim";
}

int SDL_GetRevisionNumber(void) {
    return 0;
}

/* Error handling */
int SDL_SetError(const char *fmt, ...) {
    /* Simple implementation - just copy the format string */
    if (fmt) {
        strncpy(sdl_error_msg, fmt, sizeof(sdl_error_msg) - 1);
        sdl_error_msg[sizeof(sdl_error_msg) - 1] = '\0';
    }
    return -1;
}

const char *SDL_GetError(void) {
    return sdl_error_msg;
}

void SDL_ClearError(void) {
    sdl_error_msg[0] = '\0';
}

char *SDL_GetErrorMsg(char *errstr, int maxlen) {
    if (errstr && maxlen > 0) {
        strncpy(errstr, sdl_error_msg, maxlen - 1);
        errstr[maxlen - 1] = '\0';
    }
    return errstr;
}

int SDL_Error(SDL_errorcode code) {
    switch (code) {
        case SDL_ENOMEM:
            return SDL_SetError("Out of memory");
        case SDL_EFREAD:
            return SDL_SetError("Error reading file");
        case SDL_EFWRITE:
            return SDL_SetError("Error writing file");
        case SDL_EFSEEK:
            return SDL_SetError("Error seeking in file");
        case SDL_UNSUPPORTED:
            return SDL_SetError("Operation not supported");
        default:
            return SDL_SetError("Unknown error");
    }
}

/* Delay function */
void SDL_Delay(Uint32 ms) {
    sys_sleep(ms);
}

/* Get ticks (milliseconds since SDL init) */
Uint32 SDL_GetTicks(void) {
    return sys_uptime_ms();
}

/* Get high-resolution counter */
Uint64 SDL_GetPerformanceCounter(void) {
    return (Uint64)SDL_GetTicks() * 1000;
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return 1000000;  /* Microseconds */
}

/* Environment variable functions */
char *SDL_getenv(const char *name) {
    /* VOS doesn't have environment variables, return NULL */
    (void)name;
    return NULL;
}

int SDL_setenv(const char *name, const char *value, int overwrite) {
    /* VOS doesn't have environment variables, no-op */
    (void)name;
    (void)value;
    (void)overwrite;
    return 0;
}

/* Window icon stub - VOS doesn't support window icons */
void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
    (void)window;
    (void)icon;
    /* No-op on VOS */
}

/* Message box stub - print to stderr on VOS */
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title,
                             const char *message, SDL_Window *window) {
    (void)flags;
    (void)window;
    /* Print to stderr since we don't have GUI message boxes */
    /* Note: In VOS environment this would use sys_write to stderr */
    if (title) {
        /* Could use sys_write here but keeping it simple */
    }
    if (message) {
        /* Could use sys_write here but keeping it simple */
    }
    return 0;
}
