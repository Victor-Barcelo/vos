#include <raylib.h>
#include <small3d.h>

#include <stdio.h>
#include <string.h>

static int32_t i32_min(int32_t a, int32_t b) {
    return (a < b) ? a : b;
}

int main(void) {
    InitWindow(0, 0, "raycube");
    if (!IsWindowReady()) {
        puts("raycube: framebuffer console not available");
        return 1;
    }

    int32_t w = (int32_t)GetScreenWidth();
    int32_t h = (int32_t)GetScreenHeight();
    if (w <= 0 || h <= 0) {
        puts("raycube: could not query framebuffer size");
        CloseWindow();
        return 1;
    }

    int32_t cx = w / 2;
    int32_t cy = h / 2;
    float size = (float)i32_min(w, h) * 0.25f;

    SetTargetFPS(30);

    ClearBackground(BLACK);
    DrawText("raycube (VOS): press 'q' or ESC to quit", 8, 8, 16, YELLOW);

    s3d_point2i_t prev[8];
    memset(prev, 0, sizeof(prev));
    bool have_prev = false;

    while (!WindowShouldClose()) {
        float t = (float)GetTime();

        s3d_point2i_t cur[8];
        s3d_project_wire_cube(t * 0.7f, t * 1.1f, t * 0.4f, size, w, h, cx, cy, cur);

        BeginDrawing();

        if (have_prev) {
            for (int e = 0; e < 12; e++) {
                uint8_t a = s3d_cube_edges[e][0];
                uint8_t b = s3d_cube_edges[e][1];
                DrawLine(prev[a].x, prev[a].y, prev[b].x, prev[b].y, BLACK);
            }
        }

        for (int e = 0; e < 12; e++) {
            uint8_t a = s3d_cube_edges[e][0];
            uint8_t b = s3d_cube_edges[e][1];
            DrawLine(cur[a].x, cur[a].y, cur[b].x, cur[b].y, RAYWHITE);
        }

        memcpy(prev, cur, sizeof(prev));
        have_prev = true;

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

