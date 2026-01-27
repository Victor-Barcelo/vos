#ifndef SDL_events_h_
#define SDL_events_h_

#include "SDL_stdinc.h"
#include "SDL_keyboard.h"

/**
 * SDL events subsystem for VOS
 *
 * Handles keyboard and mouse input from terminal:
 * - Keyboard: Read from stdin in raw mode
 * - Mouse: xterm mouse sequences (\x1b[M followed by 3 bytes)
 */

/**
 * Event types
 */
typedef enum {
    SDL_FIRSTEVENT = 0,

    /* Application events */
    SDL_QUIT = 0x100,

    /* Window events */
    SDL_WINDOWEVENT = 0x200,
    SDL_SYSWMEVENT,

    /* Keyboard events */
    SDL_KEYDOWN = 0x300,
    SDL_KEYUP = 0x301,
    SDL_TEXTEDITING = 0x302,
    SDL_TEXTINPUT = 0x303,

    /* Mouse events */
    SDL_MOUSEMOTION = 0x400,
    SDL_MOUSEBUTTONDOWN = 0x401,
    SDL_MOUSEBUTTONUP = 0x402,
    SDL_MOUSEWHEEL = 0x403,

    /* Joystick events */
    SDL_JOYAXISMOTION = 0x600,
    SDL_JOYBALLMOTION = 0x601,
    SDL_JOYHATMOTION = 0x602,
    SDL_JOYBUTTONDOWN = 0x603,
    SDL_JOYBUTTONUP = 0x604,
    SDL_JOYDEVICEADDED = 0x605,
    SDL_JOYDEVICEREMOVED = 0x606,

    /* Game controller events */
    SDL_CONTROLLERAXISMOTION = 0x650,
    SDL_CONTROLLERBUTTONDOWN = 0x651,
    SDL_CONTROLLERBUTTONUP = 0x652,
    SDL_CONTROLLERDEVICEADDED = 0x653,
    SDL_CONTROLLERDEVICEREMOVED = 0x654,
    SDL_CONTROLLERDEVICEREMAPPED = 0x655,

    /* User events */
    SDL_USEREVENT = 0x8000,

    SDL_LASTEVENT = 0xFFFF
} SDL_EventType;

/* Pressed/released states */
#define SDL_RELEASED 0
#define SDL_PRESSED 1

/* Mouse button definitions */
#define SDL_BUTTON_LEFT     1
#define SDL_BUTTON_MIDDLE   2
#define SDL_BUTTON_RIGHT    3
#define SDL_BUTTON_X1       4
#define SDL_BUTTON_X2       5

#define SDL_BUTTON_LMASK    (1 << (SDL_BUTTON_LEFT - 1))
#define SDL_BUTTON_MMASK    (1 << (SDL_BUTTON_MIDDLE - 1))
#define SDL_BUTTON_RMASK    (1 << (SDL_BUTTON_RIGHT - 1))
#define SDL_BUTTON_X1MASK   (1 << (SDL_BUTTON_X1 - 1))
#define SDL_BUTTON_X2MASK   (1 << (SDL_BUTTON_X2 - 1))

/**
 * Keyboard event structure
 */
typedef struct SDL_KeyboardEvent {
    Uint32 type;        /**< SDL_KEYDOWN or SDL_KEYUP */
    Uint32 timestamp;   /**< timestamp of the event */
    Uint32 windowID;    /**< window with keyboard focus */
    Uint8 state;        /**< SDL_PRESSED or SDL_RELEASED */
    Uint8 repeat;       /**< non-zero if this is a key repeat */
    Uint8 padding2;
    Uint8 padding3;
    SDL_Keysym keysym;  /**< key that was pressed or released */
} SDL_KeyboardEvent;

/**
 * Mouse motion event structure
 */
typedef struct SDL_MouseMotionEvent {
    Uint32 type;        /**< SDL_MOUSEMOTION */
    Uint32 timestamp;   /**< timestamp of the event */
    Uint32 windowID;    /**< window with mouse focus */
    Uint32 which;       /**< mouse instance id */
    Uint32 state;       /**< button state */
    Sint32 x;           /**< X coordinate, relative to window */
    Sint32 y;           /**< Y coordinate, relative to window */
    Sint32 xrel;        /**< relative X motion */
    Sint32 yrel;        /**< relative Y motion */
} SDL_MouseMotionEvent;

/**
 * Mouse button event structure
 */
typedef struct SDL_MouseButtonEvent {
    Uint32 type;        /**< SDL_MOUSEBUTTONDOWN or SDL_MOUSEBUTTONUP */
    Uint32 timestamp;   /**< timestamp of the event */
    Uint32 windowID;    /**< window with mouse focus */
    Uint32 which;       /**< mouse instance id */
    Uint8 button;       /**< mouse button index */
    Uint8 state;        /**< SDL_PRESSED or SDL_RELEASED */
    Uint8 clicks;       /**< 1 for single-click, 2 for double-click */
    Uint8 padding1;
    Sint32 x;           /**< X coordinate, relative to window */
    Sint32 y;           /**< Y coordinate, relative to window */
} SDL_MouseButtonEvent;

/**
 * Mouse wheel event structure
 */
typedef struct SDL_MouseWheelEvent {
    Uint32 type;        /**< SDL_MOUSEWHEEL */
    Uint32 timestamp;   /**< timestamp of the event */
    Uint32 windowID;    /**< window with mouse focus */
    Uint32 which;       /**< mouse instance id */
    Sint32 x;           /**< horizontal scroll amount */
    Sint32 y;           /**< vertical scroll amount */
    Uint32 direction;   /**< scroll direction */
} SDL_MouseWheelEvent;

/**
 * Window event structure
 */
typedef struct SDL_WindowEvent {
    Uint32 type;        /**< SDL_WINDOWEVENT */
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 event;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint32 data1;
    Sint32 data2;
} SDL_WindowEvent;

/**
 * Quit event structure
 */
typedef struct SDL_QuitEvent {
    Uint32 type;        /**< SDL_QUIT */
    Uint32 timestamp;
} SDL_QuitEvent;

/**
 * User event structure
 */
typedef struct SDL_UserEvent {
    Uint32 type;        /**< SDL_USEREVENT through SDL_LASTEVENT-1 */
    Uint32 timestamp;
    Uint32 windowID;
    Sint32 code;
    void *data1;
    void *data2;
} SDL_UserEvent;

/**
 * General event structure
 */
typedef union SDL_Event {
    Uint32 type;                    /**< event type */
    SDL_KeyboardEvent key;          /**< keyboard event data */
    SDL_MouseMotionEvent motion;    /**< mouse motion event data */
    SDL_MouseButtonEvent button;    /**< mouse button event data */
    SDL_MouseWheelEvent wheel;      /**< mouse wheel event data */
    SDL_WindowEvent window;         /**< window event data */
    SDL_QuitEvent quit;             /**< quit event data */
    SDL_UserEvent user;             /**< user event data */

    /* Padding to ensure union is large enough */
    Uint8 padding[56];
} SDL_Event;

/**
 * Initialize the events subsystem.
 * Called internally by SDL_Init.
 *
 * @return 0 on success, -1 on error
 */
int SDL_EventsInit(void);

/**
 * Shutdown the events subsystem.
 * Called internally by SDL_Quit.
 */
void SDL_EventsQuit(void);

/**
 * Pump the event loop, gathering events from input devices.
 *
 * This function gathers all pending input and queues it.
 * Without calls to SDL_PumpEvents, no events will be queued.
 */
void SDL_PumpEvents(void);

/**
 * Poll for currently pending events.
 *
 * @param event the SDL_Event structure to be filled with the next event,
 *              or NULL to just check for events
 * @return 1 if there is a pending event, 0 if there are none
 */
int SDL_PollEvent(SDL_Event *event);

/**
 * Wait indefinitely for the next available event.
 *
 * @param event the SDL_Event structure to be filled in with the next event
 * @return 1 on success, 0 if there was an error
 */
int SDL_WaitEvent(SDL_Event *event);

/**
 * Wait until the specified timeout for the next available event.
 *
 * @param event the SDL_Event structure to be filled in with the next event
 * @param timeout the maximum number of milliseconds to wait
 * @return 1 on success, 0 if there was an error or timeout
 */
int SDL_WaitEventTimeout(SDL_Event *event, int timeout);

/**
 * Add an event to the event queue.
 *
 * @param event the SDL_Event to be added to the queue
 * @return 1 on success, 0 if the event was filtered, -1 on error
 */
int SDL_PushEvent(SDL_Event *event);

/**
 * Check if events are available.
 *
 * @return SDL_TRUE if events are available, SDL_FALSE otherwise
 */
SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType);

/**
 * Clear events from the event queue.
 *
 * @param minType minimum event type to clear
 * @param maxType maximum event type to clear
 */
void SDL_FlushEvents(Uint32 minType, Uint32 maxType);

/**
 * Get mouse state.
 *
 * @param x pointer to receive X position, or NULL
 * @param y pointer to receive Y position, or NULL
 * @return button state bitmask
 */
Uint32 SDL_GetMouseState(int *x, int *y);

/**
 * Get relative mouse state.
 *
 * @param x pointer to receive relative X motion, or NULL
 * @param y pointer to receive relative Y motion, or NULL
 * @return button state bitmask
 */
Uint32 SDL_GetRelativeMouseState(int *x, int *y);

#endif /* SDL_events_h_ */
