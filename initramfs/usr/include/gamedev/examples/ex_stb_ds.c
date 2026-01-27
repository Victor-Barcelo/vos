// Example: stb_ds.h - Dynamic Arrays and Hash Maps
// Compile: tcc ex_stb_ds.c -o ex_stb_ds

#include <stdio.h>
#include <string.h>

#define STB_DS_IMPLEMENTATION
#include "../stb_ds.h"

// Example struct for entities
typedef struct {
    int id;
    float x, y;
    int health;
    char name[32];
} Entity;

// Hash map key-value for string->int mapping
typedef struct {
    char* key;
    int value;
} StrIntPair;

int main(void) {
    printf("=== stb_ds.h (Dynamic Arrays & Hash Maps) Example ===\n\n");

    // ============================================
    // DYNAMIC ARRAYS
    // ============================================
    printf("--- Dynamic Arrays ---\n\n");

    // Create dynamic array of integers (starts as NULL)
    int* numbers = NULL;

    // Add elements
    arrput(numbers, 10);
    arrput(numbers, 20);
    arrput(numbers, 30);
    arrput(numbers, 40);
    arrput(numbers, 50);

    printf("Array after arrput: ");
    for (int i = 0; i < arrlen(numbers); i++) {
        printf("%d ", numbers[i]);
    }
    printf("\nLength: %d\n", (int)arrlen(numbers));

    // Insert at position
    arrins(numbers, 2, 25);  // Insert 25 at index 2
    printf("After insert 25 at [2]: ");
    for (int i = 0; i < arrlen(numbers); i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    // Delete at position
    arrdel(numbers, 0);  // Remove first element
    printf("After delete [0]: ");
    for (int i = 0; i < arrlen(numbers); i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    // Pop last element
    int last = arrpop(numbers);
    printf("Popped: %d, remaining: %d items\n", last, (int)arrlen(numbers));

    arrfree(numbers);
    printf("Array freed.\n\n");

    // ============================================
    // ARRAY OF STRUCTS (Game Entities)
    // ============================================
    printf("--- Array of Structs (Entities) ---\n\n");

    Entity* entities = NULL;

    // Add entities
    Entity player = {1, 100.0f, 200.0f, 100, "Player"};
    Entity enemy1 = {2, 300.0f, 200.0f, 50, "Goblin"};
    Entity enemy2 = {3, 400.0f, 250.0f, 30, "Slime"};

    arrput(entities, player);
    arrput(entities, enemy1);
    arrput(entities, enemy2);

    printf("Entities (%d total):\n", (int)arrlen(entities));
    for (int i = 0; i < arrlen(entities); i++) {
        Entity* e = &entities[i];
        printf("  [%d] %s: pos=(%.0f,%.0f) hp=%d\n",
               e->id, e->name, e->x, e->y, e->health);
    }

    // Modify in place
    entities[0].x += 10;
    entities[0].y += 5;
    printf("\nPlayer moved to (%.0f, %.0f)\n", entities[0].x, entities[0].y);

    arrfree(entities);
    printf("Entities freed.\n\n");

    // ============================================
    // HASH MAP (String Keys)
    // ============================================
    printf("--- Hash Map (String -> Int) ---\n\n");

    StrIntPair* scores = NULL;

    // Add entries (shput for string keys)
    shput(scores, "Alice", 1500);
    shput(scores, "Bob", 2300);
    shput(scores, "Charlie", 1800);
    shput(scores, "Diana", 3100);

    printf("High Scores:\n");
    for (int i = 0; i < shlen(scores); i++) {
        printf("  %s: %d\n", scores[i].key, scores[i].value);
    }

    // Lookup by key
    int bob_score = shget(scores, "Bob");
    printf("\nBob's score: %d\n", bob_score);

    // Check if key exists
    int idx = shgeti(scores, "Eve");
    printf("Eve exists: %s\n", idx >= 0 ? "YES" : "NO");

    idx = shgeti(scores, "Alice");
    printf("Alice exists: %s (index %d)\n", idx >= 0 ? "YES" : "NO", idx);

    // Update value
    shput(scores, "Bob", 2500);  // Bob got more points
    printf("\nBob's updated score: %d\n", shget(scores, "Bob"));

    // Delete entry
    shdel(scores, "Charlie");
    printf("\nAfter deleting Charlie (%d entries):\n", (int)shlen(scores));
    for (int i = 0; i < shlen(scores); i++) {
        printf("  %s: %d\n", scores[i].key, scores[i].value);
    }

    shfree(scores);
    printf("\nHash map freed.\n\n");

    // ============================================
    // INTEGER KEY HASH MAP
    // ============================================
    printf("--- Hash Map (Int -> Entity) ---\n\n");

    struct { int key; Entity value; }* entity_map = NULL;

    // Add entities by ID
    Entity e1 = {100, 50, 50, 100, "Hero"};
    Entity e2 = {200, 150, 50, 50, "Enemy"};
    Entity e3 = {300, 250, 100, 30, "NPC"};

    hmput(entity_map, 100, e1);
    hmput(entity_map, 200, e2);
    hmput(entity_map, 300, e3);

    // Lookup by ID
    Entity hero = hmget(entity_map, 100);
    printf("Entity 100: %s at (%.0f, %.0f)\n", hero.name, hero.x, hero.y);

    // Check existence
    int exists = hmgeti(entity_map, 999);
    printf("Entity 999 exists: %s\n", exists >= 0 ? "YES" : "NO");

    hmfree(entity_map);
    printf("Entity map freed.\n");

    printf("\nDone!\n");
    return 0;
}
