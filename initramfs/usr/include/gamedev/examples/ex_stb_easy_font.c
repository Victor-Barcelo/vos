// Example: stb_easy_font.h - Quick Bitmap Font Rendering
// Compile: tcc ex_stb_easy_font.c -o ex_stb_easy_font

#include <stdio.h>

// stb_easy_font generates vertex data for simple text rendering
// It outputs quads that can be rendered with OpenGL, software renderer, etc.

#include "../stb_easy_font.h"

// Vertex structure (stb_easy_font outputs positions + colors)
typedef struct {
    float x, y, z;
    unsigned char color[4];
} Vertex;

int main(void) {
    printf("=== stb_easy_font.h (Quick Bitmap Font) Example ===\n\n");

    printf("stb_easy_font generates vertices for rendering simple text.\n");
    printf("It's designed for quick debug text, FPS counters, etc.\n\n");

    // Buffer for vertex data
    // Each character can generate up to ~270 bytes of vertex data
    char vertex_buffer[4096];

    // Generate vertices for text
    const char* text = "Hello, VOS!";
    float x = 10.0f;
    float y = 10.0f;

    int num_quads = stb_easy_font_print(x, y, (char*)text, NULL, vertex_buffer, sizeof(vertex_buffer));

    printf("Text: \"%s\"\n", text);
    printf("Generated %d quads (%d vertices, %d triangles)\n",
           num_quads, num_quads * 4, num_quads * 2);

    // Each quad is 4 vertices, each vertex is:
    // - 3 floats for position (x, y, z)
    // - 4 bytes for color (RGBA)
    int vertex_size = 3 * sizeof(float) + 4 * sizeof(char);
    printf("Vertex size: %d bytes\n", vertex_size);
    printf("Total vertex data: %d bytes\n\n", num_quads * 4 * vertex_size);

    // Print first few vertices as example
    printf("First quad vertices (position only):\n");
    float* verts = (float*)vertex_buffer;
    for (int i = 0; i < 4 && i < num_quads * 4; i++) {
        // Each vertex: x, y, z, then color bytes
        int offset = i * (3 + 1);  // 3 floats + 1 float worth of color bytes
        printf("  Vertex %d: (%.1f, %.1f, %.1f)\n",
               i, verts[offset], verts[offset + 1], verts[offset + 2]);
    }

    // Get text dimensions
    printf("\n--- Text Dimensions ---\n");
    int width = stb_easy_font_width((char*)text);
    int height = stb_easy_font_height((char*)text);
    printf("Text \"%s\": %d x %d pixels\n", text, width, height);

    // Different texts
    const char* texts[] = {
        "FPS: 60",
        "Score: 12345",
        "Level 1",
        "Press SPACE to start",
        "GAME OVER"
    };

    printf("\n--- Various Text Measurements ---\n");
    for (int i = 0; i < 5; i++) {
        int w = stb_easy_font_width((char*)texts[i]);
        int h = stb_easy_font_height((char*)texts[i]);
        printf("  \"%s\": %d x %d\n", texts[i], w, h);
    }

    // Spacing control
    printf("\n--- Character Spacing ---\n");
    printf("Default spacing: %d\n", 0);  // stb_easy_font uses fixed spacing

    // Multi-line text
    printf("\n--- Multi-line Text ---\n");
    const char* multiline = "Line 1\nLine 2\nLine 3";
    int mh = stb_easy_font_height((char*)multiline);
    printf("3-line text height: %d pixels\n", mh);

    // Usage pattern for rendering
    printf("\n--- Typical Usage Pattern ---\n");
    printf("1. Call stb_easy_font_print() to generate vertex data\n");
    printf("2. Upload vertices to GPU (or use with software renderer)\n");
    printf("3. Draw quads with simple shader or pixel plotting\n");
    printf("4. Each quad = 2 triangles = 6 indices\n");

    // Index pattern for rendering quads as triangles
    printf("\nIndex pattern for quad %d: [%d,%d,%d, %d,%d,%d]\n",
           0, 0, 1, 2, 0, 2, 3);

    printf("\nDone!\n");
    return 0;
}
