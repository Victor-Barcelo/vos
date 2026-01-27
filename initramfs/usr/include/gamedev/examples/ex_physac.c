// Example: physac.h - 2D Physics Engine
// Compile: tcc ex_physac.c -lm -o ex_physac

#include <stdio.h>

// Required defines before including physac
#define PHYSAC_STANDALONE
#define PHYSAC_NO_THREADS
#define PHYSAC_IMPLEMENTATION
#include "../physac.h"

int main(void) {
    printf("=== physac.h Example ===\n\n");

    // Initialize physics
    InitPhysics();
    printf("Physics initialized.\n");

    // Set gravity
    SetPhysicsGravity(0.0f, 9.81f);
    printf("Gravity set to (0, 9.81)\n\n");

    // Create a static floor (mass = 0 means static)
    PhysicsBody floor = CreatePhysicsBodyRectangle(
        (Vector2){400, 500},  // position
        800,                   // width
        20,                    // height
        10.0f                  // density (ignored for static)
    );
    floor->enabled = false;  // Make it static (doesn't move)
    printf("Created static floor at y=500\n");

    // Create a dynamic ball
    PhysicsBody ball = CreatePhysicsBodyCircle(
        (Vector2){400, 100},  // position
        30.0f,                // radius
        1.0f                  // density
    );
    ball->restitution = 0.8f;  // Bounciness
    printf("Created bouncing ball at y=100\n");

    // Create a dynamic box
    PhysicsBody box = CreatePhysicsBodyRectangle(
        (Vector2){300, 50},   // position
        50,                    // width
        50,                    // height
        1.0f                   // density
    );
    printf("Created box at y=50\n");

    // Simulate a few physics steps
    printf("\nSimulating 60 physics steps...\n");
    for (int i = 0; i < 60; i++) {
        UpdatePhysics();

        // Print positions every 10 frames
        if (i % 10 == 0) {
            printf("  Frame %2d: Ball Y=%.1f, Box Y=%.1f\n",
                   i, ball->position.y, box->position.y);
        }
    }

    // Apply an impulse to the ball (like jumping)
    printf("\nApplying upward impulse to ball...\n");
    PhysicsAddForce(ball, (Vector2){0, -500});

    // Simulate more
    for (int i = 60; i < 120; i++) {
        UpdatePhysics();
        if (i % 10 == 0) {
            printf("  Frame %2d: Ball Y=%.1f\n", i, ball->position.y);
        }
    }

    // Get physics body count
    int bodyCount = GetPhysicsBodiesCount();
    printf("\nTotal physics bodies: %d\n", bodyCount);

    // Cleanup
    DestroyPhysicsBody(ball);
    DestroyPhysicsBody(box);
    DestroyPhysicsBody(floor);
    ClosePhysics();

    printf("\nDone!\n");
    return 0;
}
