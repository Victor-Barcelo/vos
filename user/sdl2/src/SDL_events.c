/**
 * SDL events subsystem implementation for VOS
 *
 * Handles keyboard and mouse input from terminal:
 * - Keyboard: Read from stdin in raw mode using termios
 * - Mouse: xterm mouse sequences (\x1b[M followed by 3 bytes)
 * - Uses poll() syscall for non-blocking input
 */

#include "SDL2/SDL.h"
#include "syscall.h"
#include <termios.h>
#include <unistd.h>
#include <sys/poll.h>
#include <string.h>

/* Event queue */
#define EVENT_QUEUE_SIZE 256
static SDL_Event event_queue[EVENT_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

/* Terminal state */
static struct termios orig_termios;
static int raw_mode_enabled = 0;

/* Modifier key state */
static SDL_Keymod current_modstate = KMOD_NONE;

/* Keyboard state array */
static Uint8 key_state[SDL_NUM_SCANCODES];

/* Mouse state */
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_xrel = 0;
static int mouse_yrel = 0;
static Uint32 mouse_buttons = 0;

/* Escape sequence buffer */
#define ESC_BUFFER_SIZE 32
static unsigned char esc_buffer[ESC_BUFFER_SIZE];
static int esc_len = 0;
static int in_escape = 0;

/* Events initialized flag */
static int events_initialized = 0;

/* Get timestamp in milliseconds */
static Uint32 get_timestamp(void) {
    return sys_uptime_ms();
}

/* Enable raw mode for stdin */
static int enable_raw_mode(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return -1;
    }

    raw = orig_termios;

    /* Input modes: no break, no CR to NL, no parity check, no strip char,
       no start/stop output control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Output modes: disable post processing */
    raw.c_oflag &= ~(OPOST);

    /* Control modes: set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* Local modes: no echo, no canonical, no extended functions,
       no signal chars (^C, ^Z, etc) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Control chars: set return condition - min bytes and timer */
    raw.c_cc[VMIN] = 0;   /* Return immediately with whatever is available */
    raw.c_cc[VTIME] = 0;  /* No timeout */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    raw_mode_enabled = 1;

    /* Enable mouse tracking (xterm SGR mode) */
    /* Use basic xterm mouse mode for wider compatibility */
    write(STDOUT_FILENO, "\x1b[?1000h", 8);  /* Enable mouse click tracking */
    write(STDOUT_FILENO, "\x1b[?1002h", 8);  /* Enable mouse motion tracking */
    write(STDOUT_FILENO, "\x1b[?1006h", 8);  /* Enable SGR extended mode */

    return 0;
}

/* Disable raw mode */
static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        /* Disable mouse tracking */
        write(STDOUT_FILENO, "\x1b[?1006l", 8);
        write(STDOUT_FILENO, "\x1b[?1002l", 8);
        write(STDOUT_FILENO, "\x1b[?1000l", 8);

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

/* Queue an event */
static int queue_event(SDL_Event *event) {
    if (queue_count >= EVENT_QUEUE_SIZE) {
        return -1;  /* Queue full */
    }

    event_queue[queue_tail] = *event;
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    queue_count++;
    return 0;
}

/* Dequeue an event */
static int dequeue_event(SDL_Event *event) {
    if (queue_count == 0) {
        return 0;  /* Queue empty */
    }

    if (event) {
        *event = event_queue[queue_head];
    }
    queue_head = (queue_head + 1) % EVENT_QUEUE_SIZE;
    queue_count--;
    return 1;
}

/* Map character to scancode */
static SDL_Scancode char_to_scancode(unsigned char c) {
    if (c >= 'a' && c <= 'z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'A'));
    }
    if (c >= '1' && c <= '9') {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (c - '1'));
    }
    if (c == '0') {
        return SDL_SCANCODE_0;
    }

    switch (c) {
        case '\r':
        case '\n':
            return SDL_SCANCODE_RETURN;
        case '\x1b':
            return SDL_SCANCODE_ESCAPE;
        case '\b':
        case 127:
            return SDL_SCANCODE_BACKSPACE;
        case '\t':
            return SDL_SCANCODE_TAB;
        case ' ':
            return SDL_SCANCODE_SPACE;
        case '-':
            return SDL_SCANCODE_MINUS;
        case '=':
            return SDL_SCANCODE_EQUALS;
        case '[':
            return SDL_SCANCODE_LEFTBRACKET;
        case ']':
            return SDL_SCANCODE_RIGHTBRACKET;
        case '\\':
            return SDL_SCANCODE_BACKSLASH;
        case ';':
            return SDL_SCANCODE_SEMICOLON;
        case '\'':
            return SDL_SCANCODE_APOSTROPHE;
        case '`':
            return SDL_SCANCODE_GRAVE;
        case ',':
            return SDL_SCANCODE_COMMA;
        case '.':
            return SDL_SCANCODE_PERIOD;
        case '/':
            return SDL_SCANCODE_SLASH;
        default:
            return SDL_SCANCODE_UNKNOWN;
    }
}

/* Map character to keycode */
static SDL_Keycode char_to_keycode(unsigned char c) {
    if (c >= 'a' && c <= 'z') {
        return (SDL_Keycode)c;
    }
    if (c >= 'A' && c <= 'Z') {
        return (SDL_Keycode)(c + 32);  /* Convert to lowercase */
    }
    if (c >= '0' && c <= '9') {
        return (SDL_Keycode)c;
    }
    if (c >= 32 && c < 127) {
        return (SDL_Keycode)c;
    }

    switch (c) {
        case '\r':
        case '\n':
            return SDLK_RETURN;
        case '\x1b':
            return SDLK_ESCAPE;
        case '\b':
        case 127:
            return SDLK_BACKSPACE;
        case '\t':
            return SDLK_TAB;
        default:
            return SDLK_UNKNOWN;
    }
}

/* Generate a key event */
static void generate_key_event(Uint32 type, SDL_Scancode scancode,
                                SDL_Keycode keycode) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.key.type = type;
    event.key.timestamp = get_timestamp();
    event.key.windowID = 0;
    event.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = keycode;
    event.key.keysym.mod = current_modstate;

    /* Update key state */
    if (scancode < SDL_NUM_SCANCODES) {
        key_state[scancode] = (type == SDL_KEYDOWN) ? 1 : 0;
    }

    queue_event(&event);
}

/* Generate a mouse motion event */
static void generate_mouse_motion(int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    int old_x = mouse_x;
    int old_y = mouse_y;

    mouse_x = x;
    mouse_y = y;
    mouse_xrel = x - old_x;
    mouse_yrel = y - old_y;

    event.motion.type = SDL_MOUSEMOTION;
    event.motion.timestamp = get_timestamp();
    event.motion.windowID = 0;
    event.motion.which = 0;
    event.motion.state = mouse_buttons;
    event.motion.x = mouse_x;
    event.motion.y = mouse_y;
    event.motion.xrel = mouse_xrel;
    event.motion.yrel = mouse_yrel;

    queue_event(&event);
}

/* Generate a mouse button event */
static void generate_mouse_button(Uint32 type, Uint8 button, int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    mouse_x = x;
    mouse_y = y;

    /* Update button state */
    if (type == SDL_MOUSEBUTTONDOWN) {
        mouse_buttons |= (1 << (button - 1));
    } else {
        mouse_buttons &= ~(1 << (button - 1));
    }

    event.button.type = type;
    event.button.timestamp = get_timestamp();
    event.button.windowID = 0;
    event.button.which = 0;
    event.button.button = button;
    event.button.state = (type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = mouse_x;
    event.button.y = mouse_y;

    queue_event(&event);
}

/* Generate a mouse wheel event */
static void generate_mouse_wheel(int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.wheel.type = SDL_MOUSEWHEEL;
    event.wheel.timestamp = get_timestamp();
    event.wheel.windowID = 0;
    event.wheel.which = 0;
    event.wheel.x = x;
    event.wheel.y = y;
    event.wheel.direction = 0;

    queue_event(&event);
}

/* Generate a quit event */
static void generate_quit_event(void) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.quit.type = SDL_QUIT;
    event.quit.timestamp = get_timestamp();

    queue_event(&event);
}

/* Parse xterm mouse sequence (basic mode: \x1b[M followed by 3 bytes) */
static int parse_mouse_basic(void) {
    if (esc_len < 6) {
        return 0;  /* Need more data */
    }

    /* Format: \x1b[M Cb Cx Cy */
    unsigned char cb = esc_buffer[3] - 32;
    unsigned char cx = esc_buffer[4] - 32;
    unsigned char cy = esc_buffer[5] - 32;

    int button = (cb & 0x03);
    int x = (cx - 1) * 8;  /* Approximate pixel position */
    int y = (cy - 1) * 16;

    /* Check for wheel events */
    if (cb & 64) {
        if (button == 0) {
            generate_mouse_wheel(0, 1);  /* Scroll up */
        } else if (button == 1) {
            generate_mouse_wheel(0, -1); /* Scroll down */
        }
    }
    /* Check for motion */
    else if (cb & 32) {
        generate_mouse_motion(x, y);
    }
    /* Button press/release */
    else if (button == 3) {
        /* Button release (basic mode doesn't tell which button) */
        generate_mouse_button(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
    } else {
        int sdl_button = SDL_BUTTON_LEFT;
        if (button == 1) sdl_button = SDL_BUTTON_MIDDLE;
        else if (button == 2) sdl_button = SDL_BUTTON_RIGHT;

        generate_mouse_button(SDL_MOUSEBUTTONDOWN, sdl_button, x, y);
    }

    return 6;  /* Consumed 6 bytes */
}

/* Parse xterm SGR mouse sequence (\x1b[<...M or \x1b[<...m) */
static int parse_mouse_sgr(void) {
    /* Format: \x1b[<Cb;Cx;CyM or \x1b[<Cb;Cx;Cym */
    int cb = 0, cx = 0, cy = 0;
    int i = 3;  /* Start after \x1b[< */
    int state = 0;
    int pressed = 1;

    while (i < esc_len) {
        char c = esc_buffer[i];
        if (c >= '0' && c <= '9') {
            if (state == 0) cb = cb * 10 + (c - '0');
            else if (state == 1) cx = cx * 10 + (c - '0');
            else if (state == 2) cy = cy * 10 + (c - '0');
        } else if (c == ';') {
            state++;
        } else if (c == 'M' || c == 'm') {
            pressed = (c == 'M');
            i++;
            break;
        } else {
            return -1;  /* Invalid sequence */
        }
        i++;
    }

    if (i > esc_len) {
        return 0;  /* Need more data */
    }

    int button = (cb & 0x03);
    int x = (cx - 1) * 8;  /* Approximate pixel position */
    int y = (cy - 1) * 16;

    /* Check for wheel events */
    if (cb & 64) {
        if (button == 0) {
            generate_mouse_wheel(0, 1);  /* Scroll up */
        } else if (button == 1) {
            generate_mouse_wheel(0, -1); /* Scroll down */
        }
    }
    /* Check for motion */
    else if (cb & 32) {
        generate_mouse_motion(x, y);
    }
    /* Button press/release */
    else {
        int sdl_button = SDL_BUTTON_LEFT;
        if (button == 1) sdl_button = SDL_BUTTON_MIDDLE;
        else if (button == 2) sdl_button = SDL_BUTTON_RIGHT;

        if (pressed) {
            generate_mouse_button(SDL_MOUSEBUTTONDOWN, sdl_button, x, y);
        } else {
            generate_mouse_button(SDL_MOUSEBUTTONUP, sdl_button, x, y);
        }
    }

    return i;  /* Consumed i bytes */
}

/* Parse escape sequence */
static void parse_escape_sequence(void) {
    if (esc_len < 2) {
        /* Just ESC key */
        generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        generate_key_event(SDL_KEYUP, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        return;
    }

    /* Check for CSI sequences (\x1b[) */
    if (esc_buffer[1] == '[') {
        /* Mouse sequences */
        if (esc_len >= 3 && esc_buffer[2] == 'M') {
            parse_mouse_basic();
            return;
        }
        if (esc_len >= 3 && esc_buffer[2] == '<') {
            int consumed = parse_mouse_sgr();
            if (consumed > 0) {
                return;
            }
        }

        /* Arrow keys and other special keys */
        if (esc_len >= 3) {
            switch (esc_buffer[2]) {
                case 'A':  /* Up arrow */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_UP, SDLK_UP);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_UP, SDLK_UP);
                    return;
                case 'B':  /* Down arrow */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_DOWN, SDLK_DOWN);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_DOWN, SDLK_DOWN);
                    return;
                case 'C':  /* Right arrow */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_RIGHT, SDLK_RIGHT);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_RIGHT, SDLK_RIGHT);
                    return;
                case 'D':  /* Left arrow */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_LEFT, SDLK_LEFT);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_LEFT, SDLK_LEFT);
                    return;
                case 'H':  /* Home */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_HOME, SDLK_HOME);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_HOME, SDLK_HOME);
                    return;
                case 'F':  /* End */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_END, SDLK_END);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_END, SDLK_END);
                    return;
            }
        }

        /* Extended CSI sequences like \x1b[1~ (Home), \x1b[4~ (End), etc. */
        if (esc_len >= 4 && esc_buffer[esc_len - 1] == '~') {
            int code = 0;
            for (int i = 2; i < esc_len - 1; i++) {
                if (esc_buffer[i] >= '0' && esc_buffer[i] <= '9') {
                    code = code * 10 + (esc_buffer[i] - '0');
                }
            }

            switch (code) {
                case 1:  /* Home */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_HOME, SDLK_HOME);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_HOME, SDLK_HOME);
                    return;
                case 2:  /* Insert */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_INSERT, SDLK_INSERT);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_INSERT, SDLK_INSERT);
                    return;
                case 3:  /* Delete */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_DELETE, SDLK_DELETE);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_DELETE, SDLK_DELETE);
                    return;
                case 4:  /* End */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_END, SDLK_END);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_END, SDLK_END);
                    return;
                case 5:  /* Page Up */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_PAGEUP, SDLK_PAGEUP);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_PAGEUP, SDLK_PAGEUP);
                    return;
                case 6:  /* Page Down */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_PAGEDOWN, SDLK_PAGEDOWN);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_PAGEDOWN, SDLK_PAGEDOWN);
                    return;
                case 11: /* F1 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F1, SDLK_F1);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F1, SDLK_F1);
                    return;
                case 12: /* F2 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F2, SDLK_F2);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F2, SDLK_F2);
                    return;
                case 13: /* F3 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F3, SDLK_F3);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F3, SDLK_F3);
                    return;
                case 14: /* F4 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F4, SDLK_F4);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F4, SDLK_F4);
                    return;
                case 15: /* F5 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F5, SDLK_F5);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F5, SDLK_F5);
                    return;
                case 17: /* F6 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F6, SDLK_F6);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F6, SDLK_F6);
                    return;
                case 18: /* F7 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F7, SDLK_F7);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F7, SDLK_F7);
                    return;
                case 19: /* F8 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F8, SDLK_F8);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F8, SDLK_F8);
                    return;
                case 20: /* F9 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F9, SDLK_F9);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F9, SDLK_F9);
                    return;
                case 21: /* F10 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F10, SDLK_F10);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F10, SDLK_F10);
                    return;
                case 23: /* F11 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F11, SDLK_F11);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F11, SDLK_F11);
                    return;
                case 24: /* F12 */
                    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_F12, SDLK_F12);
                    generate_key_event(SDL_KEYUP, SDL_SCANCODE_F12, SDLK_F12);
                    return;
            }
        }
    }

    /* Alt+key combinations (\x1b followed by a character) */
    if (esc_len == 2 && esc_buffer[1] >= 32 && esc_buffer[1] < 127) {
        current_modstate |= KMOD_ALT;
        SDL_Scancode sc = char_to_scancode(esc_buffer[1]);
        SDL_Keycode kc = char_to_keycode(esc_buffer[1]);
        generate_key_event(SDL_KEYDOWN, sc, kc);
        generate_key_event(SDL_KEYUP, sc, kc);
        current_modstate &= ~KMOD_ALT;
        return;
    }

    /* Unknown escape sequence - just generate ESC key */
    generate_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
    generate_key_event(SDL_KEYUP, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
}

/* Process a single input character */
static void process_input_char(unsigned char c) {
    /* Handle escape sequences */
    if (in_escape) {
        esc_buffer[esc_len++] = c;

        /* Check if escape sequence is complete */
        int complete = 0;

        if (esc_len == 1 && c != '[' && c != 'O') {
            /* Alt+key or unknown */
            complete = 1;
        } else if (esc_len >= 2 && esc_buffer[1] == '[') {
            /* CSI sequence */
            if (esc_len >= 3) {
                if (esc_buffer[2] == 'M' && esc_len >= 6) {
                    /* Basic mouse sequence complete */
                    complete = 1;
                } else if (esc_buffer[2] == '<') {
                    /* SGR mouse - ends with M or m */
                    if (c == 'M' || c == 'm') {
                        complete = 1;
                    }
                } else if ((c >= 'A' && c <= 'Z') || c == '~') {
                    /* Other CSI sequences end with letter or ~ */
                    complete = 1;
                }
            }
        } else if (esc_len >= 2 && esc_buffer[1] == 'O') {
            /* SS3 sequence - single character after O */
            complete = 1;
        }

        if (complete || esc_len >= ESC_BUFFER_SIZE - 1) {
            parse_escape_sequence();
            in_escape = 0;
            esc_len = 0;
        }
        return;
    }

    /* Start of escape sequence */
    if (c == '\x1b') {
        in_escape = 1;
        esc_len = 0;
        esc_buffer[esc_len++] = c;
        return;
    }

    /* Ctrl+C generates quit event */
    if (c == 3) {  /* ASCII ETX (Ctrl+C) */
        generate_quit_event();
        return;
    }

    /* Handle control characters */
    if (c < 32) {
        current_modstate |= KMOD_CTRL;

        /* Map control character back to the original key */
        unsigned char orig = c + 'a' - 1;  /* Ctrl+A = 1, etc. */
        if (c == 0) orig = ' ';  /* Ctrl+Space */
        else if (c == '\r' || c == '\n') orig = c;
        else if (c == '\t') orig = c;
        else if (c == '\b') orig = c;

        SDL_Scancode sc = char_to_scancode(orig);
        SDL_Keycode kc = char_to_keycode(orig);
        generate_key_event(SDL_KEYDOWN, sc, kc);
        generate_key_event(SDL_KEYUP, sc, kc);
        current_modstate &= ~KMOD_CTRL;
        return;
    }

    /* Check for shift (uppercase letters) */
    if (c >= 'A' && c <= 'Z') {
        current_modstate |= KMOD_SHIFT;
    }

    /* Regular character */
    SDL_Scancode sc = char_to_scancode(c);
    SDL_Keycode kc = char_to_keycode(c);
    generate_key_event(SDL_KEYDOWN, sc, kc);
    generate_key_event(SDL_KEYUP, sc, kc);

    /* Clear shift for uppercase */
    if (c >= 'A' && c <= 'Z') {
        current_modstate &= ~KMOD_SHIFT;
    }
}

/* Check for timeout on escape sequence */
static void check_escape_timeout(void) {
    /* If we're in an escape sequence and no more data, process what we have */
    if (in_escape && esc_len > 0) {
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        /* Short timeout to see if more data is coming */
        int ret = poll(&pfd, 1, 10);
        if (ret <= 0) {
            /* Timeout - process the escape sequence as-is */
            parse_escape_sequence();
            in_escape = 0;
            esc_len = 0;
        }
    }
}

/* Read input from stdin */
static void read_input(int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        unsigned char buf[64];
        int n = read(STDIN_FILENO, buf, sizeof(buf));
        for (int i = 0; i < n; i++) {
            process_input_char(buf[i]);
        }
    }

    /* Handle escape sequence timeout */
    check_escape_timeout();
}

/* Public API */

int SDL_EventsInit(void) {
    if (events_initialized) {
        return 0;
    }

    memset(event_queue, 0, sizeof(event_queue));
    memset(key_state, 0, sizeof(key_state));
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    current_modstate = KMOD_NONE;
    mouse_x = 0;
    mouse_y = 0;
    mouse_xrel = 0;
    mouse_yrel = 0;
    mouse_buttons = 0;
    in_escape = 0;
    esc_len = 0;

    if (enable_raw_mode() < 0) {
        /* Not a terminal, but that's okay */
    }

    events_initialized = 1;
    return 0;
}

void SDL_EventsQuit(void) {
    if (!events_initialized) {
        return;
    }

    disable_raw_mode();
    events_initialized = 0;
}

void SDL_PumpEvents(void) {
    if (!events_initialized) {
        return;
    }

    /* Non-blocking read */
    read_input(0);
}

int SDL_PollEvent(SDL_Event *event) {
    if (!events_initialized) {
        return 0;
    }

    /* Pump events first */
    SDL_PumpEvents();

    /* Return next event from queue */
    return dequeue_event(event);
}

int SDL_WaitEvent(SDL_Event *event) {
    if (!events_initialized) {
        return 0;
    }

    while (1) {
        /* Check queue first */
        if (dequeue_event(event)) {
            return 1;
        }

        /* Wait for input with long timeout */
        read_input(1000);
    }
}

int SDL_WaitEventTimeout(SDL_Event *event, int timeout) {
    if (!events_initialized) {
        return 0;
    }

    /* Check queue first */
    if (dequeue_event(event)) {
        return 1;
    }

    /* Wait for input with specified timeout */
    read_input(timeout);

    return dequeue_event(event);
}

int SDL_PushEvent(SDL_Event *event) {
    if (!events_initialized || !event) {
        return -1;
    }

    return queue_event(event) == 0 ? 1 : -1;
}

SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType) {
    for (int i = 0; i < queue_count; i++) {
        int idx = (queue_head + i) % EVENT_QUEUE_SIZE;
        if (event_queue[idx].type >= minType &&
            event_queue[idx].type <= maxType) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

void SDL_FlushEvents(Uint32 minType, Uint32 maxType) {
    /* Full flush if entire range requested */
    if (minType <= SDL_FIRSTEVENT && maxType >= SDL_LASTEVENT) {
        queue_head = 0;
        queue_tail = 0;
        queue_count = 0;
        return;
    }

    /* Selective flush: rebuild queue without matching events */
    if (queue_count == 0) {
        return;
    }

    SDL_Event temp_queue[EVENT_QUEUE_SIZE];
    int temp_count = 0;

    /* Copy non-matching events to temp queue */
    for (int i = 0; i < queue_count; i++) {
        int idx = (queue_head + i) % EVENT_QUEUE_SIZE;
        Uint32 type = event_queue[idx].type;
        if (type < minType || type > maxType) {
            /* Keep this event */
            temp_queue[temp_count++] = event_queue[idx];
        }
    }

    /* Copy back to main queue */
    queue_head = 0;
    queue_tail = temp_count;
    queue_count = temp_count;
    for (int i = 0; i < temp_count; i++) {
        event_queue[i] = temp_queue[i];
    }
}

SDL_Keymod SDL_GetModState(void) {
    return current_modstate;
}

void SDL_SetModState(SDL_Keymod modstate) {
    current_modstate = modstate;
}

const Uint8 *SDL_GetKeyboardState(int *numkeys) {
    if (numkeys) {
        *numkeys = SDL_NUM_SCANCODES;
    }
    return key_state;
}

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode) {
    /* Simple mapping - real SDL has a more complex table */
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return (SDL_Keycode)('a' + (scancode - SDL_SCANCODE_A));
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return (SDL_Keycode)('1' + (scancode - SDL_SCANCODE_1));
    }
    if (scancode == SDL_SCANCODE_0) {
        return SDLK_0;
    }
    return SDL_SCANCODE_TO_KEYCODE(scancode);
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key) {
    if (key >= 'a' && key <= 'z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (key - 'a'));
    }
    if (key >= '1' && key <= '9') {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (key - '1'));
    }
    if (key == '0') {
        return SDL_SCANCODE_0;
    }
    if (key & SDLK_SCANCODE_MASK) {
        return (SDL_Scancode)(key & ~SDLK_SCANCODE_MASK);
    }
    return SDL_SCANCODE_UNKNOWN;
}

const char *SDL_GetScancodeName(SDL_Scancode scancode) {
    /* Minimal implementation */
    static char buf[2];
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        buf[0] = 'A' + (scancode - SDL_SCANCODE_A);
        buf[1] = '\0';
        return buf;
    }
    return "Unknown";
}

const char *SDL_GetKeyName(SDL_Keycode key) {
    /* Minimal implementation */
    static char buf[2];
    if (key >= 'a' && key <= 'z') {
        buf[0] = key - 32;  /* To uppercase */
        buf[1] = '\0';
        return buf;
    }
    if (key >= '0' && key <= '9') {
        buf[0] = key;
        buf[1] = '\0';
        return buf;
    }
    return "Unknown";
}

Uint32 SDL_GetMouseState(int *x, int *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    return mouse_buttons;
}

Uint32 SDL_GetRelativeMouseState(int *x, int *y) {
    if (x) *x = mouse_xrel;
    if (y) *y = mouse_yrel;
    /* Reset relative motion after reading */
    mouse_xrel = 0;
    mouse_yrel = 0;
    return mouse_buttons;
}

/* Text input functions - VOS always accepts text input */
static int text_input_active = 1;

void SDL_StartTextInput(void) {
    text_input_active = 1;
}

void SDL_StopTextInput(void) {
    text_input_active = 0;
}

SDL_bool SDL_IsTextInputActive(void) {
    return text_input_active ? SDL_TRUE : SDL_FALSE;
}

void SDL_SetTextInputRect(const SDL_Rect *rect) {
    (void)rect;  /* No-op on VOS */
}

/* Cursor functions - VOS doesn't support cursor changing */

SDL_Cursor* SDL_CreateSystemCursor(SDL_SystemCursor id) {
    (void)id;
    return NULL;  /* No cursor support in VOS */
}

SDL_Cursor* SDL_CreateColorCursor(SDL_Surface *surface, int hot_x, int hot_y) {
    (void)surface;
    (void)hot_x;
    (void)hot_y;
    return NULL;  /* No cursor support in VOS */
}

void SDL_FreeCursor(SDL_Cursor *cursor) {
    (void)cursor;
    /* No-op on VOS */
}

void SDL_SetCursor(SDL_Cursor *cursor) {
    (void)cursor;
    /* No-op on VOS */
}

SDL_Cursor* SDL_GetCursor(void) {
    return NULL;  /* No cursor support in VOS */
}

SDL_Cursor* SDL_GetDefaultCursor(void) {
    return NULL;  /* No cursor support in VOS */
}
