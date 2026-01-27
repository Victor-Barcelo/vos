// Example: ecs.h - Entity Component System
// Compile: tcc ex_ecs.c -o ex_ecs

#include <stdio.h>
#include <string.h>

#define ECS_IMPLEMENTATION
#include "../ecs.h"

// Define component types
typedef struct {
    float x, y;
} Position;

typedef struct {
    float vx, vy;
} Velocity;

typedef struct {
    int health;
    int max_health;
} Health;

typedef struct {
    char name[32];
} Name;

// Component IDs
enum {
    COMP_POSITION = 0,
    COMP_VELOCITY,
    COMP_HEALTH,
    COMP_NAME,
    COMP_COUNT
};

int main(void) {
    printf("=== ecs.h (Entity Component System) Example ===\n\n");

    // Create ECS world
    ecs_t* world = ecs_new(1024, NULL);  // Max 1024 entities
    printf("Created ECS world.\n");

    // Register components with their sizes
    ecs_register(world, COMP_POSITION, sizeof(Position));
    ecs_register(world, COMP_VELOCITY, sizeof(Velocity));
    ecs_register(world, COMP_HEALTH, sizeof(Health));
    ecs_register(world, COMP_NAME, sizeof(Name));
    printf("Registered %d component types.\n\n", COMP_COUNT);

    // Create player entity
    ecs_id_t player = ecs_create(world);
    printf("Created player entity (ID: %u)\n", player);

    // Add components to player
    Position* pos = (Position*)ecs_add(world, player, COMP_POSITION);
    pos->x = 100.0f;
    pos->y = 200.0f;

    Velocity* vel = (Velocity*)ecs_add(world, player, COMP_VELOCITY);
    vel->vx = 5.0f;
    vel->vy = 0.0f;

    Health* hp = (Health*)ecs_add(world, player, COMP_HEALTH);
    hp->health = 100;
    hp->max_health = 100;

    Name* name = (Name*)ecs_add(world, player, COMP_NAME);
    strcpy(name->name, "Hero");

    printf("  Added Position, Velocity, Health, Name components.\n");

    // Create enemy entities
    for (int i = 0; i < 3; i++) {
        ecs_id_t enemy = ecs_create(world);

        Position* epos = (Position*)ecs_add(world, enemy, COMP_POSITION);
        epos->x = 300.0f + i * 50.0f;
        epos->y = 200.0f;

        Health* ehp = (Health*)ecs_add(world, enemy, COMP_HEALTH);
        ehp->health = 30;
        ehp->max_health = 30;

        printf("Created enemy %d at (%.0f, %.0f)\n", i + 1, epos->x, epos->y);
    }

    // Query: Get player's components
    printf("\n--- Querying Components ---\n");
    Position* ppos = (Position*)ecs_get(world, player, COMP_POSITION);
    Health* php = (Health*)ecs_get(world, player, COMP_HEALTH);
    Name* pname = (Name*)ecs_get(world, player, COMP_NAME);

    if (ppos && php && pname) {
        printf("Player '%s': pos=(%.0f, %.0f), HP=%d/%d\n",
               pname->name, ppos->x, ppos->y, php->health, php->max_health);
    }

    // Movement system: Update all entities with Position + Velocity
    printf("\n--- Running Movement System ---\n");
    printf("Simulating 5 frames of movement...\n");

    for (int frame = 0; frame < 5; frame++) {
        // In a real game, you'd iterate all entities with both components
        Position* p = (Position*)ecs_get(world, player, COMP_POSITION);
        Velocity* v = (Velocity*)ecs_get(world, player, COMP_VELOCITY);
        if (p && v) {
            p->x += v->vx;
            p->y += v->vy;
        }
    }

    ppos = (Position*)ecs_get(world, player, COMP_POSITION);
    printf("Player position after 5 frames: (%.0f, %.0f)\n", ppos->x, ppos->y);

    // Check if entity has component
    printf("\n--- Component Checks ---\n");
    printf("Player has Velocity: %s\n",
           ecs_has(world, player, COMP_VELOCITY) ? "YES" : "NO");

    // Remove a component
    ecs_remove(world, player, COMP_VELOCITY);
    printf("Removed Velocity from player.\n");
    printf("Player has Velocity: %s\n",
           ecs_has(world, player, COMP_VELOCITY) ? "YES" : "NO");

    // Destroy an entity
    ecs_destroy(world, player);
    printf("\nDestroyed player entity.\n");

    // Cleanup
    ecs_free(world);
    printf("\nDone!\n");
    return 0;
}
