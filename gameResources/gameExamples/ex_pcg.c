// Example: pcg_basic.h - PCG Random Number Generator
// Compile: tcc ex_pcg.c ../pcg_basic.c -o ex_pcg

#include <stdio.h>
#include "../pcg_basic.h"

// Helper: random float between 0 and 1
float random_float(pcg32_random_t* rng) {
    return (float)pcg32_random_r(rng) / (float)UINT32_MAX;
}

// Helper: random int in range [min, max]
int random_range(pcg32_random_t* rng, int min, int max) {
    return min + pcg32_boundedrand_r(rng, max - min + 1);
}

int main(void) {
    printf("=== pcg_basic.h (PCG Random) Example ===\n\n");

    // Initialize RNG with seed
    pcg32_random_t rng;
    uint64_t seed = 12345;  // Use any seed (time-based in real game)
    uint64_t seq = 67890;   // Stream selector

    pcg32_srandom_r(&rng, seed, seq);
    printf("PCG32 initialized with seed=%llu, seq=%llu\n\n", seed, seq);

    // Generate random 32-bit integers
    printf("--- Random uint32 values ---\n");
    for (int i = 0; i < 5; i++) {
        printf("  %u\n", pcg32_random_r(&rng));
    }

    // Generate bounded random numbers (dice rolls)
    printf("\n--- Dice Rolls (1-6) ---\n");
    printf("Rolling 10 dice: ");
    for (int i = 0; i < 10; i++) {
        int roll = 1 + pcg32_boundedrand_r(&rng, 6);
        printf("%d ", roll);
    }
    printf("\n");

    // Random floats (for positions, colors, etc.)
    printf("\n--- Random Floats (0.0 - 1.0) ---\n");
    for (int i = 0; i < 5; i++) {
        printf("  %.4f\n", random_float(&rng));
    }

    // Random positions in game world
    printf("\n--- Random Spawn Positions (0-800, 0-600) ---\n");
    for (int i = 0; i < 5; i++) {
        int x = random_range(&rng, 0, 800);
        int y = random_range(&rng, 0, 600);
        printf("  Enemy %d: (%d, %d)\n", i + 1, x, y);
    }

    // Weighted random (loot drops)
    printf("\n--- Loot Drop Simulation ---\n");
    printf("Drop chances: Common=60%%, Rare=30%%, Epic=8%%, Legendary=2%%\n");
    printf("Dropping 20 items:\n  ");

    int common = 0, rare = 0, epic = 0, legendary = 0;
    for (int i = 0; i < 20; i++) {
        int roll = random_range(&rng, 1, 100);
        if (roll <= 60) {
            printf("C ");
            common++;
        } else if (roll <= 90) {
            printf("R ");
            rare++;
        } else if (roll <= 98) {
            printf("E ");
            epic++;
        } else {
            printf("L ");
            legendary++;
        }
    }
    printf("\n");
    printf("Results: Common=%d, Rare=%d, Epic=%d, Legendary=%d\n",
           common, rare, epic, legendary);

    // Shuffle array (Fisher-Yates)
    printf("\n--- Shuffling Array ---\n");
    int deck[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    printf("Original: ");
    for (int i = 0; i < 10; i++) printf("%d ", deck[i]);
    printf("\n");

    // Fisher-Yates shuffle
    for (int i = 9; i > 0; i--) {
        int j = pcg32_boundedrand_r(&rng, i + 1);
        int temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }

    printf("Shuffled: ");
    for (int i = 0; i < 10; i++) printf("%d ", deck[i]);
    printf("\n");

    // Reproducibility demo
    printf("\n--- Reproducibility (same seed = same sequence) ---\n");
    pcg32_srandom_r(&rng, 42, 1);
    printf("Seed 42: ");
    for (int i = 0; i < 5; i++) printf("%u ", pcg32_random_r(&rng));
    printf("\n");

    pcg32_srandom_r(&rng, 42, 1);  // Reset to same seed
    printf("Seed 42: ");
    for (int i = 0; i < 5; i++) printf("%u ", pcg32_random_r(&rng));
    printf("\n");

    printf("\nDone!\n");
    return 0;
}
