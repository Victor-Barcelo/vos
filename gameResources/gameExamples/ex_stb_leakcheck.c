// Example: stb_leakcheck.h - Memory Leak Detection
// Compile: tcc ex_stb_leakcheck.c -o ex_stb_leakcheck

#include <stdio.h>

// IMPORTANT: Include stb_leakcheck.h BEFORE stdlib.h
// It redefines malloc/free to track allocations
#define STB_LEAKCHECK_IMPLEMENTATION
#include "../stb_leakcheck.h"

#include <string.h>

// Simulate game objects
typedef struct {
    float x, y;
    float vx, vy;
    int health;
    char* name;
} GameObject;

GameObject* create_object(const char* name, float x, float y) {
    GameObject* obj = (GameObject*)malloc(sizeof(GameObject));
    obj->x = x;
    obj->y = y;
    obj->vx = 0;
    obj->vy = 0;
    obj->health = 100;

    // Allocate name separately (potential leak!)
    obj->name = (char*)malloc(strlen(name) + 1);
    strcpy(obj->name, name);

    return obj;
}

void destroy_object(GameObject* obj) {
    // Correct cleanup: free name first
    free(obj->name);
    free(obj);
}

void destroy_object_LEAKY(GameObject* obj) {
    // BUG: Forgot to free name!
    free(obj);
}

int main(void) {
    printf("=== stb_leakcheck.h (Memory Leak Detection) Example ===\n\n");

    printf("stb_leakcheck helps find memory leaks during development.\n");
    printf("It wraps malloc/free to track all allocations.\n\n");

    // ========================================
    // Test 1: Proper cleanup (no leaks)
    // ========================================
    printf("--- Test 1: Proper Cleanup ---\n");

    GameObject* player = create_object("Hero", 100, 200);
    printf("Created: %s at (%.0f, %.0f)\n", player->name, player->x, player->y);

    GameObject* enemy = create_object("Goblin", 300, 200);
    printf("Created: %s at (%.0f, %.0f)\n", enemy->name, enemy->x, enemy->y);

    // Proper cleanup
    destroy_object(player);
    destroy_object(enemy);
    printf("Destroyed both objects properly.\n\n");

    // Check for leaks
    printf("Checking for leaks after Test 1...\n");
    stb_leakcheck_dumpmem();
    printf("(No output = no leaks!)\n\n");

    // ========================================
    // Test 2: Intentional leak
    // ========================================
    printf("--- Test 2: Intentional Memory Leak ---\n");

    GameObject* leaky1 = create_object("LeakyObject1", 0, 0);
    printf("Created: %s (will leak!)\n", leaky1->name);

    // Oops! Using the buggy destroy function
    destroy_object_LEAKY(leaky1);
    printf("Destroyed with LEAKY function (forgot to free name).\n\n");

    // Another leak: not freeing at all
    GameObject* leaky2 = create_object("LeakyObject2", 50, 50);
    printf("Created: %s (never freed!)\n", leaky2->name);

    // More allocations that we "forget"
    char* forgotten_string = (char*)malloc(100);
    strcpy(forgotten_string, "This string was never freed!");
    printf("Allocated string: \"%s\"\n\n", forgotten_string);

    // Check for leaks
    printf("Checking for leaks after Test 2...\n");
    printf("=========================================\n");
    stb_leakcheck_dumpmem();
    printf("=========================================\n");
    printf("(Above shows leaked memory locations)\n\n");

    // ========================================
    // Cleanup remaining leaks for clean exit
    // ========================================
    printf("--- Cleaning up remaining allocations ---\n");

    // Fix the leaks
    free(leaky2->name);
    free(leaky2);
    free(forgotten_string);

    printf("Freed remaining allocations.\n\n");

    // Final check
    printf("Final leak check:\n");
    stb_leakcheck_dumpmem();
    printf("(No output = all clean!)\n");

    // ========================================
    // Usage tips
    // ========================================
    printf("\n--- Usage Tips ---\n");
    printf("1. Include stb_leakcheck.h FIRST (before stdlib.h)\n");
    printf("2. Call stb_leakcheck_dumpmem() periodically or at exit\n");
    printf("3. Output shows file:line where leaked memory was allocated\n");
    printf("4. Remove stb_leakcheck for release builds (small overhead)\n");
    printf("5. Use #define STB_LEAKCHECK_SHOWALL to see all allocs\n");

    printf("\nDone!\n");
    return 0;
}
