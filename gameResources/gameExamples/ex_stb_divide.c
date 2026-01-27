// Example: stb_divide.h - Safe Integer Division
// Compile: tcc ex_stb_divide.c -o ex_stb_divide

#include <stdio.h>

#define STB_DIVIDE_IMPLEMENTATION
#include "../stb_divide.h"

int main(void) {
    printf("=== stb_divide.h (Safe Integer Division) Example ===\n\n");

    printf("stb_divide provides safe division functions that avoid\n");
    printf("undefined behavior and handle edge cases properly.\n\n");

    // Standard division problems:
    // 1. Division by zero (undefined behavior)
    // 2. INT_MIN / -1 (overflow on 2's complement)
    // 3. Different rounding modes (truncation vs floor)

    printf("--- Euclidean Division (always positive remainder) ---\n");
    printf("Useful for array indexing, tile maps, etc.\n\n");

    int test_cases[][2] = {
        {7, 3},      // Normal case
        {-7, 3},     // Negative dividend
        {7, -3},     // Negative divisor
        {-7, -3},    // Both negative
        {10, 5},     // Even division
        {0, 3},      // Zero dividend
    };

    printf("%-10s | %-6s | %-6s | %-10s | %-10s\n",
           "a / b", "Quot", "Rem", "C div", "C mod");
    printf("-----------+--------+--------+------------+------------\n");

    for (int i = 0; i < 6; i++) {
        int a = test_cases[i][0];
        int b = test_cases[i][1];

        int eq = stb_div_eucl(a, b);
        int er = stb_mod_eucl(a, b);
        int cq = a / b;
        int cr = a % b;

        printf("%3d / %3d  | %6d | %6d | %10d | %10d\n",
               a, b, eq, er, cq, cr);
    }

    printf("\nNote: Euclidean mod always returns 0 <= r < |b|\n");
    printf("C's %% can return negative remainders.\n");

    // Practical example: Tile map wrapping
    printf("\n--- Practical Example: Tile Map Wrapping ---\n");

    int map_width = 10;
    int positions[] = {5, 15, -3, -15, 0, 10, -10};

    printf("Map width: %d tiles\n", map_width);
    printf("Wrapping positions to valid tile indices:\n\n");

    printf("%-10s | %-12s | %-12s\n", "Position", "C mod", "Eucl mod");
    printf("-----------+--------------+--------------\n");

    for (int i = 0; i < 7; i++) {
        int pos = positions[i];
        int c_result = pos % map_width;
        int e_result = stb_mod_eucl(pos, map_width);

        printf("%10d | %12d | %12d %s\n",
               pos, c_result, e_result,
               c_result < 0 ? "(WRONG!)" : "");
    }

    printf("\nEuclidean mod correctly wraps negative positions!\n");

    // Floor division
    printf("\n--- Floor Division ---\n");
    printf("Always rounds toward negative infinity.\n\n");

    printf("%-10s | %-10s | %-10s\n", "a / b", "Floor div", "C div");
    printf("-----------+------------+------------\n");

    for (int i = 0; i < 6; i++) {
        int a = test_cases[i][0];
        int b = test_cases[i][1];

        int fd = stb_div_floor(a, b);
        int cd = a / b;

        printf("%3d / %3d  | %10d | %10d\n", a, b, fd, cd);
    }

    printf("\nC division truncates toward zero.\n");
    printf("Floor division always rounds down.\n");

    // Trunc division (same as C, but safer)
    printf("\n--- Truncation Division (Safe C-style) ---\n");

    int td = stb_div_trunc(7, 3);
    int tr = stb_mod_trunc(7, 3);
    printf("stb_div_trunc(7, 3) = %d, stb_mod_trunc(7, 3) = %d\n", td, tr);
    printf("Same as C division, but handles edge cases safely.\n");

    printf("\nDone!\n");
    return 0;
}
