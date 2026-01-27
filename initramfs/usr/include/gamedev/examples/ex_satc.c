// Example: satc.h - SAT Collision Detection
// Compile: tcc ex_satc.c -lm -o ex_satc

#include <stdio.h>

#define SATC_IMPLEMENTATION
#include "../satc.h"

int main(void) {
    printf("=== satc.h (SAT Collision) Example ===\n\n");

    // Create two circles
    satc_circle_t* circle1 = satc_circle_new(100.0, 100.0, 30.0);  // x, y, radius
    satc_circle_t* circle2 = satc_circle_new(120.0, 100.0, 30.0);  // overlapping

    printf("Circle 1: center=(100, 100), radius=30\n");
    printf("Circle 2: center=(120, 100), radius=30\n");

    // Test circle-circle collision
    satc_response_t* response = satc_response_new();

    bool colliding = satc_test_circle_circle(circle1, circle2, response);
    printf("\nCircle-Circle collision: %s\n", colliding ? "YES" : "NO");
    if (colliding) {
        printf("  Overlap: %.2f\n", response->overlap);
        printf("  Overlap vector: (%.2f, %.2f)\n",
               response->overlap_v[0], response->overlap_v[1]);
    }

    // Move circle2 away
    printf("\nMoving circle2 to (200, 100)...\n");
    satc_circle_set_offset(circle2, 200.0, 100.0);

    satc_response_clear(response);
    colliding = satc_test_circle_circle(circle1, circle2, response);
    printf("Circle-Circle collision: %s\n", colliding ? "YES" : "NO");

    // Create polygons (boxes)
    printf("\n--- Polygon Collision ---\n");

    // Box 1: 50x50 at (0, 0)
    satc_polygon_t* box1 = satc_box_new(0, 0, 50, 50)->polygon;
    // Box 2: 50x50 at (40, 0) - overlapping
    satc_polygon_t* box2 = satc_box_new(40, 0, 50, 50)->polygon;

    printf("Box 1: 50x50 at (0, 0)\n");
    printf("Box 2: 50x50 at (40, 0)\n");

    satc_response_clear(response);
    colliding = satc_test_polygon_polygon(box1, box2, response);
    printf("\nBox-Box collision: %s\n", colliding ? "YES" : "NO");
    if (colliding) {
        printf("  Overlap: %.2f\n", response->overlap);
        printf("  To separate box2, move by: (%.2f, %.2f)\n",
               response->overlap_v[0], response->overlap_v[1]);
    }

    // Circle vs Polygon
    printf("\n--- Circle vs Polygon ---\n");
    satc_circle_set_offset(circle1, 25, 25);  // Center of box1
    printf("Circle at center of box1 (25, 25)\n");

    satc_response_clear(response);
    colliding = satc_test_circle_polygon(circle1, box1, response);
    printf("Circle-Box collision: %s\n", colliding ? "YES" : "NO");

    // Cleanup
    satc_circle_free(circle1);
    satc_circle_free(circle2);
    satc_polygon_free(box1);
    satc_polygon_free(box2);
    satc_response_free(response);

    printf("\nDone!\n");
    return 0;
}
