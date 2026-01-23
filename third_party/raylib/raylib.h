#ifndef VOS_RAYLIB_H
#define VOS_RAYLIB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Color;

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

// Basic colors (subset of raylib)
#define LIGHTGRAY ((Color){200, 200, 200, 255})
#define GRAY      ((Color){130, 130, 130, 255})
#define DARKGRAY  ((Color){80, 80, 80, 255})
#define YELLOW    ((Color){253, 249, 0, 255})
#define GOLD      ((Color){255, 203, 0, 255})
#define ORANGE    ((Color){255, 161, 0, 255})
#define PINK      ((Color){255, 109, 194, 255})
#define RED       ((Color){230, 41, 55, 255})
#define MAROON    ((Color){190, 33, 55, 255})
#define GREEN     ((Color){0, 228, 48, 255})
#define LIME      ((Color){0, 158, 47, 255})
#define DARKGREEN ((Color){0, 117, 44, 255})
#define SKYBLUE   ((Color){102, 191, 255, 255})
#define BLUE      ((Color){0, 121, 241, 255})
#define DARKBLUE  ((Color){0, 82, 172, 255})
#define PURPLE    ((Color){200, 122, 255, 255})
#define VIOLET    ((Color){135, 60, 190, 255})
#define DARKPURPLE ((Color){112, 31, 126, 255})
#define BEIGE     ((Color){211, 176, 131, 255})
#define BROWN     ((Color){127, 106, 79, 255})
#define DARKBROWN ((Color){76, 63, 47, 255})
#define WHITE     ((Color){255, 255, 255, 255})
#define BLACK     ((Color){0, 0, 0, 255})
#define BLANK     ((Color){0, 0, 0, 0})
#define MAGENTA   ((Color){255, 0, 255, 255})
#define RAYWHITE  ((Color){245, 245, 245, 255})

void InitWindow(int width, int height, const char* title);
bool IsWindowReady(void);
void CloseWindow(void);
bool WindowShouldClose(void);

int GetScreenWidth(void);
int GetScreenHeight(void);

void SetTargetFPS(int fps);
float GetFrameTime(void);
double GetTime(void);

void BeginDrawing(void);
void EndDrawing(void);

void ClearBackground(Color color);
void DrawPixel(int posX, int posY, Color color);
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
void DrawRectangle(int posX, int posY, int width, int height, Color color);
void DrawRectangleLines(int posX, int posY, int width, int height, Color color);
void DrawText(const char* text, int posX, int posY, int fontSize, Color color);

#ifdef __cplusplus
}
#endif

#endif

