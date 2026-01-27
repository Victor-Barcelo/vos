// Example: easing.h - Animation Easing Functions
// Compile: tcc ex_easing.c ../easing.c -lm -o ex_easing

#include <stdio.h>
#include "../easing.h"

// Helper to visualize easing with ASCII
void print_easing_curve(const char* name, AHFloat (*func)(AHFloat)) {
    printf("\n%s:\n", name);
    printf("0.0                    0.5                    1.0\n");
    printf("|                       |                       |\n");

    // Print curve using ASCII
    int width = 50;
    for (int y = 10; y >= 0; y--) {
        float target_y = y / 10.0f;
        printf("%3.1f ", target_y);

        for (int x = 0; x <= width; x++) {
            float t = (float)x / width;
            float val = func(t);

            // Check if this point is on the curve (within threshold)
            if (val >= target_y - 0.05f && val <= target_y + 0.05f) {
                printf("*");
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("    +");
    for (int i = 0; i < width; i++) printf("-");
    printf("+\n");
}

// Demonstrate easing in game context
void animate_position(const char* name, AHFloat (*func)(AHFloat)) {
    printf("\n%s - Object moving from X=0 to X=100:\n", name);
    printf("Time  | Position | Visual\n");
    printf("------+----------+");
    for (int i = 0; i < 21; i++) printf("-");
    printf("\n");

    for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        float eased = func(t);
        float position = eased * 100.0f;

        printf(" %3.1f  |  %5.1f   |", t, position);

        // Visual bar
        int bar_pos = (int)(eased * 20);
        for (int j = 0; j < 20; j++) {
            if (j == bar_pos) printf("O");
            else if (j < bar_pos) printf("=");
            else printf(" ");
        }
        printf("|\n");
    }
}

int main(void) {
    printf("=== easing.h (Animation Easing) Example ===\n\n");

    printf("Easing functions transform linear time (0->1) into curved motion.\n");
    printf("Use them for smooth animations, UI transitions, camera moves, etc.\n");

    // Show different easing types
    printf("\n=== EASE IN (slow start, fast end) ===");
    animate_position("QuadraticEaseIn", QuadraticEaseIn);

    printf("\n=== EASE OUT (fast start, slow end) ===");
    animate_position("QuadraticEaseOut", QuadraticEaseOut);

    printf("\n=== EASE IN-OUT (slow start, fast middle, slow end) ===");
    animate_position("QuadraticEaseInOut", QuadraticEaseInOut);

    // Show bounce effect
    printf("\n=== SPECIAL: Bounce ===");
    animate_position("BounceEaseOut", BounceEaseOut);

    // Show elastic effect
    printf("\n=== SPECIAL: Elastic ===");
    animate_position("ElasticEaseOut", ElasticEaseOut);

    // Show back (overshoot) effect
    printf("\n=== SPECIAL: Back (overshoot) ===");
    animate_position("BackEaseOut", BackEaseOut);

    // Comparison of all easing types at t=0.5
    printf("\n=== All Easing Functions at t=0.5 ===\n");
    printf("%-20s | Value\n", "Function");
    printf("---------------------+-------\n");

    float t = 0.5f;
    printf("%-20s | %.3f\n", "Linear", LinearInterpolation(t));
    printf("%-20s | %.3f\n", "QuadraticEaseIn", QuadraticEaseIn(t));
    printf("%-20s | %.3f\n", "QuadraticEaseOut", QuadraticEaseOut(t));
    printf("%-20s | %.3f\n", "QuadraticEaseInOut", QuadraticEaseInOut(t));
    printf("%-20s | %.3f\n", "CubicEaseIn", CubicEaseIn(t));
    printf("%-20s | %.3f\n", "CubicEaseOut", CubicEaseOut(t));
    printf("%-20s | %.3f\n", "SineEaseIn", SineEaseIn(t));
    printf("%-20s | %.3f\n", "SineEaseOut", SineEaseOut(t));
    printf("%-20s | %.3f\n", "CircularEaseIn", CircularEaseIn(t));
    printf("%-20s | %.3f\n", "ExponentialEaseIn", ExponentialEaseIn(t));
    printf("%-20s | %.3f\n", "BounceEaseOut", BounceEaseOut(t));
    printf("%-20s | %.3f\n", "ElasticEaseOut", ElasticEaseOut(t));
    printf("%-20s | %.3f\n", "BackEaseOut", BackEaseOut(t));

    printf("\nDone!\n");
    return 0;
}
