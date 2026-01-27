/*
 * SDL_error.h - VOS minimal SDL2 shim
 * Error handling functions compatible with SDL2 API
 */

#ifndef SDL_error_h_
#define SDL_error_h_

#include "SDL_stdinc.h"

/*
 * SDL_SetError() - Set the SDL error message
 *
 * This function sets an error message that can be retrieved with SDL_GetError().
 * Returns -1 so it can be used as a return value for error conditions.
 */
int SDL_SetError(const char *fmt, ...);

/*
 * SDL_GetError() - Get the last error message
 *
 * Returns a pointer to a static buffer containing the last error message.
 * The returned string is valid until the next call to an SDL function
 * that sets an error.
 */
const char *SDL_GetError(void);

/*
 * SDL_GetErrorMsg() - Get the last error message into a buffer
 *
 * This is a thread-safe alternative to SDL_GetError().
 * Returns the message buffer for convenience.
 */
char *SDL_GetErrorMsg(char *errstr, int maxlen);

/*
 * SDL_ClearError() - Clear any previous error message
 */
void SDL_ClearError(void);

/*
 * Internal error codes (for SDL_Error)
 */
typedef enum {
    SDL_ENOMEM,
    SDL_EFREAD,
    SDL_EFWRITE,
    SDL_EFSEEK,
    SDL_UNSUPPORTED,
    SDL_LASTERROR
} SDL_errorcode;

/*
 * SDL_Error() - Set an error from a predefined error code
 * Returns -1 so it can be used as a return value.
 */
int SDL_Error(SDL_errorcode code);

/*
 * SDL_OutOfMemory() - Convenience macro for out of memory errors
 */
#define SDL_OutOfMemory() SDL_Error(SDL_ENOMEM)

/*
 * SDL_Unsupported() - Convenience macro for unsupported operations
 */
#define SDL_Unsupported() SDL_Error(SDL_UNSUPPORTED)

/*
 * SDL_InvalidParamError() - Convenience macro for invalid parameter errors
 */
#define SDL_InvalidParamError(param) SDL_SetError("Parameter '%s' is invalid", (param))

#endif /* SDL_error_h_ */
