# PCG Basic - Random Number Generator

## Description

PCG (Permuted Congruential Generator) is a family of pseudorandom number generators developed by Melissa O'Neill. The PCG Basic library provides a minimal C implementation of PCG-XSH-RR, which offers excellent statistical properties, small state size, and fast generation speed.

PCG improves upon traditional Linear Congruential Generators (LCGs) by applying an output permutation function that eliminates the statistical weaknesses of raw LCG output while maintaining their speed and simplicity.

## Original Source

- **Repository**: [https://github.com/imneme/pcg-c-basic](https://github.com/imneme/pcg-c-basic)
- **Author**: Melissa O'Neill
- **Website**: [https://www.pcg-random.org](https://www.pcg-random.org)
- **Paper**: "PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for Random Number Generation"

## License

Apache License, Version 2.0

```
Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
```

## Features

- **Fast**: Simple operations (multiply, add, xor, shift, rotate)
- **Small state**: Only 128 bits (two 64-bit values)
- **Statistically excellent**: Passes TestU01's BigCrush test suite
- **Multiple streams**: Supports 2^63 unique sequences via stream selection
- **Reproducible**: Same seed always produces same sequence
- **Uniform distribution**: Built-in bounded random for unbiased ranges
- **Thread-safe option**: Reentrant API with explicit state parameter

## API Reference

### Types

#### `pcg32_random_t`
The RNG state structure:

```c
typedef struct pcg_state_setseq_64 {
    uint64_t state;  // RNG state - all values are possible
    uint64_t inc;    // Stream selector - must always be odd
} pcg32_random_t;
```

### Macros

#### `PCG32_INITIALIZER`
Static initializer for `pcg32_random_t`:

```c
#define PCG32_INITIALIZER { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
```

### Functions

#### Seeding Functions

##### `pcg32_srandom`
```c
void pcg32_srandom(uint64_t initstate, uint64_t initseq);
```
Seed the global RNG with an initial state and sequence selector.

- `initstate`: Initial state value (can be any 64-bit value)
- `initseq`: Sequence/stream ID (determines which sequence to use)

##### `pcg32_srandom_r`
```c
void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate, uint64_t initseq);
```
Seed a specific RNG instance (reentrant version).

#### Generation Functions

##### `pcg32_random`
```c
uint32_t pcg32_random(void);
```
Generate a uniformly distributed 32-bit random number using the global RNG.

##### `pcg32_random_r`
```c
uint32_t pcg32_random_r(pcg32_random_t* rng);
```
Generate a random number using a specific RNG instance (reentrant version).

#### Bounded Random Functions

##### `pcg32_boundedrand`
```c
uint32_t pcg32_boundedrand(uint32_t bound);
```
Generate a uniformly distributed number in range [0, bound) using global RNG.

##### `pcg32_boundedrand_r`
```c
uint32_t pcg32_boundedrand_r(pcg32_random_t* rng, uint32_t bound);
```
Generate a bounded random number using a specific RNG instance (reentrant version).

## Usage Examples

### Basic Usage with Global RNG

```c
#include "pcg_basic.h"
#include <stdio.h>
#include <time.h>

int main() {
    // Seed with current time and a fixed sequence
    pcg32_srandom((uint64_t)time(NULL), 42);

    // Generate random numbers
    for (int i = 0; i < 10; i++) {
        printf("Random: %u\n", pcg32_random());
    }

    return 0;
}
```

### Dice Rolling

```c
#include "pcg_basic.h"

// Roll a die with 'sides' faces (1 to sides)
int rollDie(int sides) {
    return (int)pcg32_boundedrand(sides) + 1;
}

// Roll multiple dice
int rollDice(int count, int sides) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += rollDie(sides);
    }
    return total;
}

// Example: 2d6 (two six-sided dice)
int roll2d6() {
    return rollDice(2, 6);
}
```

### Local RNG for Reproducibility

```c
#include "pcg_basic.h"

typedef struct {
    pcg32_random_t rng;
    int width, height;
    int *tiles;
} GameLevel;

// Generate a level with a specific seed
void generateLevel(GameLevel *level, uint64_t seed) {
    // Use the seed for reproducible generation
    pcg32_srandom_r(&level->rng, seed, 1);

    for (int y = 0; y < level->height; y++) {
        for (int x = 0; x < level->width; x++) {
            // 20% chance of wall
            int isWall = pcg32_boundedrand_r(&level->rng, 100) < 20;
            level->tiles[y * level->width + x] = isWall;
        }
    }
}

// Same seed = same level every time
void replayLevel(GameLevel *level, uint64_t savedSeed) {
    generateLevel(level, savedSeed);
}
```

### Random Float Generation

```c
#include "pcg_basic.h"

// Generate float in [0, 1)
float randomFloat() {
    return (float)pcg32_random() / (float)0x100000000ULL;
}

// Generate float in [min, max)
float randomFloatRange(float min, float max) {
    return min + randomFloat() * (max - min);
}

// Generate float in [0, 1] (inclusive)
float randomFloat01() {
    return (float)pcg32_random() / (float)0xFFFFFFFFU;
}
```

### Shuffling an Array (Fisher-Yates)

```c
#include "pcg_basic.h"

void shuffle(int *array, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = pcg32_boundedrand(i + 1);
        // Swap array[i] and array[j]
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Shuffle a deck of cards (0-51)
void shuffleDeck(int *deck) {
    for (int i = 0; i < 52; i++) {
        deck[i] = i;
    }
    shuffle(deck, 52);
}
```

### Random Selection with Weights

```c
#include "pcg_basic.h"

// Select an index based on weights
int weightedRandom(int *weights, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += weights[i];
    }

    int roll = pcg32_boundedrand(total);

    int cumulative = 0;
    for (int i = 0; i < count; i++) {
        cumulative += weights[i];
        if (roll < cumulative) {
            return i;
        }
    }
    return count - 1;  // Fallback
}

// Example: Loot table
typedef struct { const char *name; int weight; } LootItem;

const char* rollLoot() {
    static LootItem lootTable[] = {
        {"Common Sword", 50},
        {"Rare Shield", 30},
        {"Epic Armor", 15},
        {"Legendary Ring", 5}
    };
    int weights[] = {50, 30, 15, 5};
    int idx = weightedRandom(weights, 4);
    return lootTable[idx].name;
}
```

### Multiple Independent Streams

```c
#include "pcg_basic.h"

typedef struct {
    pcg32_random_t enemyRng;
    pcg32_random_t lootRng;
    pcg32_random_t effectsRng;
} GameRng;

void initGameRng(GameRng *rng, uint64_t masterSeed) {
    // Same seed, different streams = independent sequences
    pcg32_srandom_r(&rng->enemyRng, masterSeed, 1);
    pcg32_srandom_r(&rng->lootRng, masterSeed, 2);
    pcg32_srandom_r(&rng->effectsRng, masterSeed, 3);
}

// Now enemy spawns, loot drops, and particle effects
// all have reproducible but independent randomness
```

## VOS/TCC Compatibility Notes

### Compilation
The library compiles cleanly with TCC:

```c
#include "pcg_basic.h"
#include "pcg_basic.c"
```

### Dependencies
- `<inttypes.h>` - for uint32_t, uint64_t types

### TCC-Specific Considerations
- TCC supports 64-bit integer operations required by PCG
- The `ULL` suffix for 64-bit literals is supported
- No assembly or platform-specific code

### Seeding in VOS
VOS may not have high-resolution timers. Options for seeding:

```c
// Option 1: Use frame counter + user input timing
void seedFromGameplay(uint32_t frameCount, uint32_t inputHash) {
    uint64_t seed = ((uint64_t)frameCount << 32) | inputHash;
    pcg32_srandom(seed, 54321);
}

// Option 2: Use static initializer for deterministic behavior
pcg32_random_t rng = PCG32_INITIALIZER;

// Option 3: Let user provide seed (good for replays)
void initWithUserSeed(uint64_t userSeed) {
    pcg32_srandom(userSeed, 12345);
}
```

### Global vs Local RNG
- Use global functions (`pcg32_random`, etc.) for simple cases
- Use reentrant functions (`pcg32_random_r`, etc.) when you need:
  - Reproducible sequences (save/replay)
  - Multiple independent streams
  - Thread safety (if applicable)

### Performance Notes
- Each `pcg32_random()` call is very fast (few multiplies and shifts)
- `pcg32_boundedrand()` may loop to avoid bias but averages ~1.2 iterations
- Consider caching random values if generating millions per frame

### Common Patterns in VOS Games

```c
// Random position in area
int randomX = pcg32_boundedrand(mapWidth);
int randomY = pcg32_boundedrand(mapHeight);

// Random direction (0-3 for 4-way, 0-7 for 8-way)
int direction = pcg32_boundedrand(4);

// Percentage chance
int criticalHit = pcg32_boundedrand(100) < 15;  // 15% chance

// Random element from array
int idx = pcg32_boundedrand(arrayLength);
Item *randomItem = &items[idx];

// Random range (inclusive)
int damage = minDamage + pcg32_boundedrand(maxDamage - minDamage + 1);
```
