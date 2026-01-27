// Example: stb_perlin.h - Perlin/Simplex Noise
// Compile: tcc ex_stb_perlin.c -lm -o ex_stb_perlin

#include <stdio.h>

#define STB_PERLIN_IMPLEMENTATION
#include "../stb_perlin.h"

// Map noise value (-1 to 1) to ASCII character
char noise_to_char(float n) {
    // Normalize from [-1,1] to [0,1]
    float normalized = (n + 1.0f) / 2.0f;

    if (normalized < 0.2f) return ' ';       // Deep water
    if (normalized < 0.3f) return '~';       // Shallow water
    if (normalized < 0.4f) return '.';       // Beach
    if (normalized < 0.6f) return ',';       // Grass
    if (normalized < 0.75f) return '*';      // Forest
    if (normalized < 0.85f) return '^';      // Hills
    return '#';                               // Mountains
}

int main(void) {
    printf("=== stb_perlin.h (Perlin Noise) Example ===\n\n");

    printf("Perlin noise generates smooth, natural-looking random values.\n");
    printf("Great for terrain, clouds, textures, animations, etc.\n\n");

    // 1D noise (for animation, simple variation)
    printf("--- 1D Noise (time-based variation) ---\n");
    printf("X:     ");
    for (float x = 0; x < 5; x += 0.5f) {
        printf("%4.1f ", x);
    }
    printf("\nNoise: ");
    for (float x = 0; x < 5; x += 0.5f) {
        float n = stb_perlin_noise3(x, 0, 0, 0, 0, 0);
        printf("%+.2f ", n);
    }
    printf("\n");

    // 2D noise terrain map
    printf("\n--- 2D Noise Terrain Map ---\n");
    printf("Legend: ' '=water, '~'=shallow, '.'=beach, ','=grass, '*'=forest, '^'=hills, '#'=mountain\n\n");

    int width = 60;
    int height = 20;
    float scale = 0.1f;  // Lower = smoother, larger features

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float n = stb_perlin_noise3(x * scale, y * scale, 0, 0, 0, 0);
            printf("%c", noise_to_char(n));
        }
        printf("\n");
    }

    // Fractal noise (multiple octaves for more detail)
    printf("\n--- Fractal Noise (Octaves for Detail) ---\n");
    printf("Adding multiple layers of noise at different scales:\n\n");

    scale = 0.05f;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Combine multiple octaves
            float n = 0;
            float amplitude = 1.0f;
            float frequency = 1.0f;
            float max_value = 0;

            for (int oct = 0; oct < 4; oct++) {
                n += amplitude * stb_perlin_noise3(
                    x * scale * frequency,
                    y * scale * frequency,
                    0, 0, 0, 0
                );
                max_value += amplitude;
                amplitude *= 0.5f;   // Each octave is half as strong
                frequency *= 2.0f;   // Each octave is twice as detailed
            }
            n /= max_value;  // Normalize

            printf("%c", noise_to_char(n));
        }
        printf("\n");
    }

    // Turbulence (absolute value for cloud-like patterns)
    printf("\n--- Turbulence (Cloud-like) ---\n");
    scale = 0.08f;
    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < width; x++) {
            float n = stb_perlin_turbulence_noise3(
                x * scale, y * scale, 0,
                2.0f,    // lacunarity (frequency multiplier)
                0.5f,    // gain (amplitude multiplier)
                4        // octaves
            );
            // Turbulence returns 0-1 range
            char c = (n < 0.3f) ? ' ' : (n < 0.5f) ? '.' : (n < 0.7f) ? 'o' : 'O';
            printf("%c", c);
        }
        printf("\n");
    }

    // Ridge noise (good for mountains, veins)
    printf("\n--- Ridge Noise (Mountain Ridges) ---\n");
    scale = 0.06f;
    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < width; x++) {
            float n = stb_perlin_ridge_noise3(
                x * scale, y * scale, 0,
                2.0f,    // lacunarity
                0.5f,    // gain
                1.0f,    // offset
                4        // octaves
            );
            char c = (n < 0.3f) ? ' ' : (n < 0.5f) ? '.' : (n < 0.7f) ? '^' : '#';
            printf("%c", c);
        }
        printf("\n");
    }

    printf("\nDone!\n");
    return 0;
}
