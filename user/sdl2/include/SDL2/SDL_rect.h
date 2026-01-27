/*
 * SDL_rect.h - Minimal SDL2 rectangle/point types for VOS
 */

#ifndef SDL_RECT_H
#define SDL_RECT_H

#include "SDL_stdinc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A rectangle, with the origin at the upper left (integer).
 */
typedef struct SDL_Rect {
    int x, y;
    int w, h;
} SDL_Rect;

/**
 * A point (integer).
 */
typedef struct SDL_Point {
    int x, y;
} SDL_Point;

/**
 * A rectangle, with the origin at the upper left (floating point).
 */
typedef struct SDL_FRect {
    float x, y;
    float w, h;
} SDL_FRect;

/**
 * A point (floating point).
 */
typedef struct SDL_FPoint {
    float x, y;
} SDL_FPoint;

/**
 * Returns SDL_TRUE if the rectangle has no area.
 */
static inline SDL_bool SDL_RectEmpty(const SDL_Rect *r) {
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

/**
 * Returns SDL_TRUE if the two rectangles are equal.
 */
static inline SDL_bool SDL_RectEquals(const SDL_Rect *a, const SDL_Rect *b) {
    return (a && b && (a->x == b->x) && (a->y == b->y) &&
            (a->w == b->w) && (a->h == b->h)) ? SDL_TRUE : SDL_FALSE;
}

/**
 * Determine whether two rectangles intersect.
 */
SDL_bool SDL_HasIntersection(const SDL_Rect *A, const SDL_Rect *B);

/**
 * Calculate the intersection of two rectangles.
 */
SDL_bool SDL_IntersectRect(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result);

/**
 * Calculate the union of two rectangles.
 */
void SDL_UnionRect(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result);

/**
 * Calculate a minimal rectangle enclosing a set of points.
 */
SDL_bool SDL_EnclosePoints(const SDL_Point *points, int count,
                           const SDL_Rect *clip, SDL_Rect *result);

/**
 * Calculate the intersection of a rectangle and line segment.
 */
SDL_bool SDL_IntersectRectAndLine(const SDL_Rect *rect,
                                   int *X1, int *Y1, int *X2, int *Y2);

#ifdef __cplusplus
}
#endif

#endif /* SDL_RECT_H */
