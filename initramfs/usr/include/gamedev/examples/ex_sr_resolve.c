// Example: sr_resolve.h - Simple AABB Collision Resolution
// Compile: tcc ex_sr_resolve.c -lm -o ex_sr_resolve

#include <stdio.h>

#define SR_RESOLVE_IMPLEMENTATION
#include "../sr_resolve.h"

int main(void) {
    printf("=== sr_resolve.h (AABB Collision) Example ===\n\n");

    // Define rectangles (x, y, width, height)
    sr_Rect player = {100, 100, 32, 32};
    sr_Rect wall   = {120, 100, 50, 50};
    sr_Rect ground = {0, 200, 400, 20};

    printf("Player: x=%.0f, y=%.0f, w=%.0f, h=%.0f\n",
           player.x, player.y, player.w, player.h);
    printf("Wall:   x=%.0f, y=%.0f, w=%.0f, h=%.0f\n",
           wall.x, wall.y, wall.w, wall.h);

    // Simple rectangle collision check
    bool hit = sr_check_rec_vs_rec_collision(player, wall);
    printf("\nPlayer vs Wall collision: %s\n", hit ? "YES" : "NO");

    // Collision with response (how to resolve)
    sr_Contact contact;
    hit = sr_rec_vs_rec(player, wall, &contact);
    if (hit) {
        printf("  Contact normal: (%.1f, %.1f)\n", contact.normal.x, contact.normal.y);
        printf("  Contact point:  (%.1f, %.1f)\n", contact.point.x, contact.point.y);
    }

    // Ray casting (for bullets, line-of-sight, etc.)
    printf("\n--- Ray Casting ---\n");
    sr_Point ray_origin = {50, 150};
    sr_Point ray_dir = {1, 0};  // Shooting right

    printf("Ray from (%.0f, %.0f) going right\n", ray_origin.x, ray_origin.y);

    sr_Contact ray_contact;
    hit = sr_check_ray_vs_rec_collision(ray_origin, ray_dir, wall, &ray_contact);
    printf("Ray vs Wall: %s\n", hit ? "HIT" : "MISS");
    if (hit) {
        printf("  Hit point: (%.1f, %.1f)\n", ray_contact.point.x, ray_contact.point.y);
        printf("  Hit normal: (%.1f, %.1f)\n", ray_contact.normal.x, ray_contact.normal.y);
    }

    // Dynamic collision (moving rectangle)
    printf("\n--- Dynamic Collision (Moving Player) ---\n");
    sr_Rect moving_player = {50, 100, 32, 32};
    sr_Point velocity = {100, 0};  // Moving right fast

    printf("Player at x=50, moving right with vel=100\n");

    sr_Contact dyn_contact;
    hit = sr_dynamic_rect_vs_rect(moving_player, velocity, wall, &dyn_contact);
    printf("Will hit wall: %s\n", hit ? "YES" : "NO");
    if (hit) {
        printf("  Contact time: %.2f (0=now, 1=end of frame)\n", dyn_contact.time);
        printf("  Contact point: (%.1f, %.1f)\n", dyn_contact.point.x, dyn_contact.point.y);
    }

    // Move and slide (platformer-style movement)
    printf("\n--- Move and Slide ---\n");
    sr_Rect obstacles[] = {wall, ground};
    int num_obstacles = 2;

    sr_Rect character = {80, 100, 32, 32};
    sr_Point char_vel = {50, 50};  // Moving right and down

    printf("Character at (80, 100), velocity (50, 50)\n");

    // This would slide along obstacles instead of stopping
    // sr_move_and_slide(&character, char_vel, obstacles, num_obstacles);

    printf("\nDone!\n");
    return 0;
}
