// Example: stb_rect_pack.h - Rectangle Packing (Sprite Atlases)
// Compile: tcc ex_stb_rect_pack.c -o ex_stb_rect_pack

#include <stdio.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "../stb_rect_pack.h"

#define ATLAS_WIDTH 64
#define ATLAS_HEIGHT 48
#define MAX_RECTS 20

int main(void) {
    printf("=== stb_rect_pack.h (Rectangle Packing) Example ===\n\n");

    printf("Rectangle packing is used for creating sprite atlases,\n");
    printf("texture atlases, and UI layout optimization.\n\n");

    // Define sprites/rectangles to pack
    stbrp_rect rects[] = {
        {0, 16, 16},  // id=0, 16x16 sprite
        {1, 32, 32},  // id=1, 32x32 sprite
        {2, 8, 8},    // id=2, 8x8 sprite
        {3, 16, 8},   // id=3, 16x8 sprite
        {4, 8, 16},   // id=4, 8x16 sprite
        {5, 24, 16},  // id=5, 24x16 sprite
        {6, 12, 12},  // id=6, 12x12 sprite
        {7, 10, 10},  // id=7, 10x10 sprite
        {8, 6, 6},    // id=8, 6x6 sprite
        {9, 4, 4},    // id=9, 4x4 sprite
    };
    int num_rects = sizeof(rects) / sizeof(rects[0]);

    printf("Packing %d rectangles into %dx%d atlas:\n\n", num_rects, ATLAS_WIDTH, ATLAS_HEIGHT);

    printf("Input rectangles:\n");
    for (int i = 0; i < num_rects; i++) {
        printf("  Rect %d: %dx%d\n", rects[i].id, rects[i].w, rects[i].h);
    }

    // Initialize packer
    stbrp_context ctx;
    stbrp_node nodes[ATLAS_WIDTH];  // Nodes array (width of atlas)

    stbrp_init_target(&ctx, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, ATLAS_WIDTH);

    // Pack rectangles
    int all_packed = stbrp_pack_rects(&ctx, rects, num_rects);

    printf("\n--- Packing Results ---\n");
    printf("All rectangles packed: %s\n\n", all_packed ? "YES" : "NO");

    // Create visual representation of atlas
    char atlas[ATLAS_HEIGHT][ATLAS_WIDTH + 1];
    for (int y = 0; y < ATLAS_HEIGHT; y++) {
        for (int x = 0; x < ATLAS_WIDTH; x++) {
            atlas[y][x] = '.';
        }
        atlas[y][ATLAS_WIDTH] = '\0';
    }

    // Draw packed rectangles
    printf("Packed positions:\n");
    for (int i = 0; i < num_rects; i++) {
        if (rects[i].was_packed) {
            printf("  Rect %d (%dx%d): x=%d, y=%d\n",
                   rects[i].id, rects[i].w, rects[i].h, rects[i].x, rects[i].y);

            // Draw rectangle in atlas (use digit as char)
            char c = '0' + rects[i].id;
            for (int y = rects[i].y; y < rects[i].y + rects[i].h && y < ATLAS_HEIGHT; y++) {
                for (int x = rects[i].x; x < rects[i].x + rects[i].w && x < ATLAS_WIDTH; x++) {
                    atlas[y][x] = c;
                }
            }
        } else {
            printf("  Rect %d: FAILED TO PACK!\n", rects[i].id);
        }
    }

    // Print atlas visualization
    printf("\nAtlas visualization (. = empty, 0-9 = rect IDs):\n");
    printf("   ");
    for (int x = 0; x < ATLAS_WIDTH; x += 10) {
        printf("%-10d", x);
    }
    printf("\n");

    for (int y = 0; y < ATLAS_HEIGHT; y++) {
        printf("%2d %s\n", y, atlas[y]);
    }

    // Calculate usage efficiency
    int used_pixels = 0;
    for (int i = 0; i < num_rects; i++) {
        if (rects[i].was_packed) {
            used_pixels += rects[i].w * rects[i].h;
        }
    }
    int total_pixels = ATLAS_WIDTH * ATLAS_HEIGHT;
    float efficiency = 100.0f * used_pixels / total_pixels;

    printf("\nAtlas efficiency: %d/%d pixels = %.1f%%\n", used_pixels, total_pixels, efficiency);

    printf("\nDone!\n");
    return 0;
}
