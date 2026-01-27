// Example: stb_sprintf.h - Fast sprintf replacement
// Compile: tcc ex_stb_sprintf.c -o ex_stb_sprintf

#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "../stb_sprintf.h"

int main(void) {
    printf("=== stb_sprintf.h (Fast String Formatting) Example ===\n\n");

    printf("stb_sprintf is a faster replacement for sprintf/snprintf.\n");
    printf("Useful for games that do lots of string formatting (UI, debug, etc.)\n\n");

    char buffer[256];

    // Basic formatting
    stbsp_sprintf(buffer, "Hello, %s!", "World");
    printf("Basic string: %s\n", buffer);

    // Numbers
    stbsp_sprintf(buffer, "Integer: %d, Unsigned: %u", -42, 42);
    printf("%s\n", buffer);

    // Floating point
    stbsp_sprintf(buffer, "Float: %f, Scientific: %e", 3.14159, 0.000123);
    printf("%s\n", buffer);

    // Precision control
    stbsp_sprintf(buffer, "2 decimals: %.2f, 5 decimals: %.5f", 3.14159, 3.14159);
    printf("%s\n", buffer);

    // Width and padding
    stbsp_sprintf(buffer, "Padded: [%10d] [%-10d] [%010d]", 42, 42, 42);
    printf("%s\n", buffer);

    // Hex (useful for colors, memory addresses)
    stbsp_sprintf(buffer, "Hex: 0x%X, 0x%08X", 255, 255);
    printf("%s\n", buffer);

    // Game UI example: Health bar
    printf("\n--- Game UI Examples ---\n");

    int health = 75;
    int max_health = 100;
    stbsp_sprintf(buffer, "HP: %d/%d", health, max_health);
    printf("Health display: %s\n", buffer);

    // Score with comma separators (manual)
    int score = 1234567;
    stbsp_sprintf(buffer, "Score: %d", score);
    printf("Score: %s\n", buffer);

    // Timer display
    int total_seconds = 3725;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    stbsp_sprintf(buffer, "Time: %02d:%02d:%02d", hours, minutes, seconds);
    printf("Timer: %s\n", buffer);

    // Position display
    float px = 123.456f;
    float py = 789.012f;
    stbsp_sprintf(buffer, "Pos: (%.1f, %.1f)", px, py);
    printf("Position: %s\n", buffer);

    // Color in hex
    unsigned int color = 0xFF5733;
    stbsp_sprintf(buffer, "Color: #%06X (R=%d, G=%d, B=%d)",
                  color,
                  (color >> 16) & 0xFF,
                  (color >> 8) & 0xFF,
                  color & 0xFF);
    printf("%s\n", buffer);

    // FPS counter
    float fps = 59.94f;
    float frametime = 1000.0f / fps;
    stbsp_sprintf(buffer, "FPS: %.1f (%.2fms)", fps, frametime);
    printf("%s\n", buffer);

    // Safe version with buffer size limit
    printf("\n--- Safe snprintf ---\n");
    char small_buffer[20];
    int written = stbsp_snprintf(small_buffer, sizeof(small_buffer),
                                  "This is a very long string that won't fit");
    printf("Buffer (size 20): '%s'\n", small_buffer);
    printf("Would have written: %d chars\n", written);

    // Callback version for streaming output
    printf("\n--- Callback Mode (for large outputs) ---\n");
    // stbsp_vsprintfcb can be used to write directly to files,
    // network buffers, or handle very large strings.
    printf("(Callback mode available for advanced use cases)\n");

    printf("\nDone!\n");
    return 0;
}
