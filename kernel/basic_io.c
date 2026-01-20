#include "types.h"
#include "screen.h"
#include "io.h"

// Variadic argument handling (simplified for our needs)
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

// Simple printf for BASIC interpreter
void basic_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) screen_print(s);
                    break;
                }
                case 'd': {
                    int d = va_arg(args, int);
                    screen_print_dec(d);
                    break;
                }
                case 'c': {
                    int c = va_arg(args, int);
                    screen_putchar((char)c);
                    break;
                }
                case '%':
                    screen_putchar('%');
                    break;
                default:
                    screen_putchar('%');
                    screen_putchar(*fmt);
                    break;
            }
        } else {
            screen_putchar(*fmt);
        }
        fmt++;
    }

    va_end(args);
}

// Exit function - halt the system with error message
void exit(int status) {
    screen_set_color(VGA_LIGHT_RED, VGA_BLUE);
    screen_print("\nBASIC Error (exit code: ");
    screen_print_dec(status);
    screen_println(")");
    screen_set_color(VGA_WHITE, VGA_BLUE);

    // Don't actually halt - just return to let shell continue
    // The BASIC interpreter will check ubasic_finished()
}
