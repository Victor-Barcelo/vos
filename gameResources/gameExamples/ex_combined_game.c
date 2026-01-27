// Example: Combined Game Demo
// Uses multiple libraries together for a simple game loop
// Compile: tcc ex_combined_game.c ../easing.c ../pcg_basic.c -lm -o ex_combined_game

#include <stdio.h>
#include <string.h>
#include <math.h>

// Include helper header for VOS compatibility
#include "../vos_gamedev.h"

// Libraries
#include "../linmath.h"
#include "../easing.h"
#include "../pcg_basic.h"

#define STB_DS_IMPLEMENTATION
#include "../stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "../stb_sprintf.h"

// ============================================
// Game Types
// ============================================

typedef struct {
    vec2 position;
    vec2 velocity;
    float radius;
    int health;
    int max_health;
    char name[32];
    int state;  // 0=idle, 1=moving, 2=attacking, 3=dead
} Entity;

typedef struct {
    float x, y;
    float target_x, target_y;
    float anim_time;
    int is_animating;
} AnimatedValue;

// Entity states
enum { IDLE, MOVING, ATTACKING, DEAD };
const char* state_names[] = {"IDLE", "MOVING", "ATTACKING", "DEAD"};

// ============================================
// Game State
// ============================================

Entity* entities = NULL;  // Dynamic array
pcg32_random_t rng;
int game_frame = 0;
int score = 0;

// ============================================
// Helper Functions
// ============================================

float randf(void) {
    return (float)pcg32_random_r(&rng) / (float)UINT32_MAX;
}

int randi(int min, int max) {
    return min + pcg32_boundedrand_r(&rng, max - min + 1);
}

void spawn_enemy(float x, float y) {
    Entity enemy = {0};
    enemy.position[0] = x;
    enemy.position[1] = y;
    enemy.velocity[0] = randf() * 2 - 1;
    enemy.velocity[1] = randf() * 2 - 1;
    enemy.radius = 10 + randf() * 10;
    enemy.health = 20 + randi(0, 30);
    enemy.max_health = enemy.health;
    enemy.state = MOVING;

    const char* names[] = {"Goblin", "Orc", "Troll", "Slime", "Bat"};
    stbsp_sprintf(enemy.name, "%s_%d", names[randi(0, 4)], (int)arrlen(entities));

    arrput(entities, enemy);
}

void update_animation(AnimatedValue* anim, float dt) {
    if (!anim->is_animating) return;

    anim->anim_time += dt;
    if (anim->anim_time >= 1.0f) {
        anim->x = anim->target_x;
        anim->y = anim->target_y;
        anim->is_animating = 0;
        return;
    }

    // Use easing for smooth animation
    float t = QuadraticEaseOut(anim->anim_time);
    anim->x = anim->x + (anim->target_x - anim->x) * t;
    anim->y = anim->y + (anim->target_y - anim->y) * t;
}

// ============================================
// Game Logic
// ============================================

void game_init(void) {
    printf("=== Initializing Game ===\n");

    // Seed RNG
    pcg32_srandom_r(&rng, 12345, 67890);

    // Create player
    Entity player = {0};
    player.position[0] = 400;
    player.position[1] = 300;
    player.radius = 15;
    player.health = 100;
    player.max_health = 100;
    player.state = IDLE;
    strcpy(player.name, "Player");

    arrput(entities, player);
    printf("  Created player at (%.0f, %.0f)\n", player.position[0], player.position[1]);

    // Spawn some enemies
    for (int i = 0; i < 5; i++) {
        spawn_enemy(randf() * 800, randf() * 600);
    }
    printf("  Spawned %d enemies\n", (int)arrlen(entities) - 1);
}

void game_update(float dt) {
    game_frame++;

    // Update all entities
    for (int i = 0; i < arrlen(entities); i++) {
        Entity* e = &entities[i];

        if (e->state == DEAD) continue;

        // Simple movement
        if (e->state == MOVING) {
            e->position[0] += e->velocity[0] * dt * 50;
            e->position[1] += e->velocity[1] * dt * 50;

            // Bounce off walls
            if (e->position[0] < 0 || e->position[0] > 800) {
                e->velocity[0] *= -1;
            }
            if (e->position[1] < 0 || e->position[1] > 600) {
                e->velocity[1] *= -1;
            }
        }

        // Random state changes for enemies
        if (i > 0 && randi(0, 100) < 2) {
            e->state = (e->state == IDLE) ? MOVING : IDLE;
        }
    }

    // Simple collision between player and enemies
    Entity* player = &entities[0];
    for (int i = 1; i < arrlen(entities); i++) {
        Entity* enemy = &entities[i];
        if (enemy->state == DEAD) continue;

        vec2 diff;
        vec2_sub(diff, enemy->position, player->position);
        float dist = vec2_len(diff);

        if (dist < player->radius + enemy->radius) {
            // Collision! Damage both
            player->health -= 5;
            enemy->health -= 10;

            // Push apart
            vec2_norm(diff, diff);
            vec2_scale(diff, diff, 5);
            vec2_add(enemy->position, enemy->position, diff);

            if (enemy->health <= 0) {
                enemy->state = DEAD;
                score += 100;
            }
        }
    }

    if (player->health <= 0) {
        player->state = DEAD;
    }
}

void game_render(void) {
    char buffer[256];

    printf("\n--- Frame %d ---\n", game_frame);

    // Render UI
    Entity* player = &entities[0];
    stbsp_sprintf(buffer, "HP: %d/%d | Score: %d",
                  player->health, player->max_health, score);
    printf("UI: %s\n", buffer);

    // Render entities
    printf("Entities (%d total):\n", (int)arrlen(entities));
    for (int i = 0; i < arrlen(entities); i++) {
        Entity* e = &entities[i];
        stbsp_sprintf(buffer, "  [%s] %s: (%.0f, %.0f) HP=%d/%d",
                      state_names[e->state], e->name,
                      e->position[0], e->position[1],
                      e->health, e->max_health);
        printf("%s\n", buffer);
    }
}

void game_cleanup(void) {
    printf("\n=== Cleanup ===\n");
    printf("Final score: %d\n", score);
    arrfree(entities);
}

// ============================================
// Main
// ============================================

int main(void) {
    printf("=== Combined Game Demo ===\n");
    printf("Using: linmath, easing, pcg_basic, stb_ds, stb_sprintf\n\n");

    game_init();

    // Simulate 5 game frames
    float dt = 1.0f / 60.0f;  // 60 FPS
    for (int frame = 0; frame < 5; frame++) {
        game_update(dt * 10);  // Speed up for demo
        game_render();
    }

    // Demonstrate easing animation
    printf("\n--- Easing Animation Demo ---\n");
    AnimatedValue anim = {0, 0, 100, 100, 0, 1};
    printf("Animating from (0,0) to (100,100) with QuadraticEaseOut:\n");

    for (float t = 0; t <= 1.0f; t += 0.2f) {
        float eased = QuadraticEaseOut(t);
        float x = eased * 100;
        float y = eased * 100;
        printf("  t=%.1f: eased=%.2f -> (%.0f, %.0f)\n", t, eased, x, y);
    }

    game_cleanup();

    printf("\nDone!\n");
    return 0;
}
