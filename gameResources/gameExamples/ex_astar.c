// Example: AStar.h - A* Pathfinding
// Compile: tcc ex_astar.c ../AStar.c -lm -o ex_astar

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../AStar.h"

// Simple grid map
#define MAP_WIDTH 10
#define MAP_HEIGHT 8

// 0 = walkable, 1 = wall
int map[MAP_HEIGHT][MAP_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct {
    int x, y;
} Point;

// AStar callback: Get neighbors
void getNeighbors(ASNeighborList neighbors, void* node, void* context) {
    Point* p = (Point*)node;

    // 4-directional movement
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    for (int i = 0; i < 4; i++) {
        int nx = p->x + dx[i];
        int ny = p->y + dy[i];

        // Check bounds
        if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT)
            continue;

        // Check if walkable
        if (map[ny][nx] == 1)
            continue;

        // Create neighbor point
        Point* neighbor = (Point*)malloc(sizeof(Point));
        neighbor->x = nx;
        neighbor->y = ny;

        ASNeighborListAdd(neighbors, neighbor, 1.0f);  // Cost = 1
    }
}

// AStar callback: Heuristic (Manhattan distance)
float heuristic(void* fromNode, void* toNode, void* context) {
    Point* from = (Point*)fromNode;
    Point* to = (Point*)toNode;
    return (float)(abs(to->x - from->x) + abs(to->y - from->y));
}

// AStar callback: Compare nodes
int nodeComparator(void* node1, void* node2, void* context) {
    Point* p1 = (Point*)node1;
    Point* p2 = (Point*)node2;

    if (p1->x != p2->x) return p1->x - p2->x;
    return p1->y - p2->y;
}

int main(void) {
    printf("=== AStar.h (A* Pathfinding) Example ===\n\n");

    // Print map
    printf("Map (. = walkable, # = wall):\n");
    printf("   0123456789\n");
    for (int y = 0; y < MAP_HEIGHT; y++) {
        printf(" %d ", y);
        for (int x = 0; x < MAP_WIDTH; x++) {
            printf("%c", map[y][x] ? '#' : '.');
        }
        printf("\n");
    }

    // Setup A* pathfinder
    ASPathNodeSource source = {
        sizeof(Point),
        getNeighbors,
        heuristic,
        NULL,  // No early exit
        nodeComparator
    };

    // Find path from (0,0) to (9,7)
    Point start = {0, 0};
    Point goal = {9, 7};

    printf("\nFinding path from (%d,%d) to (%d,%d)...\n", start.x, start.y, goal.x, goal.y);

    ASPath path = ASPathCreate(&source, NULL, &start, &goal);

    if (ASPathGetCount(path) > 0) {
        printf("Path found! Length: %zu nodes\n\n", ASPathGetCount(path));

        // Mark path on map copy
        char display[MAP_HEIGHT][MAP_WIDTH + 1];
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                display[y][x] = map[y][x] ? '#' : '.';
            }
            display[y][MAP_WIDTH] = '\0';
        }

        // Mark path
        for (size_t i = 0; i < ASPathGetCount(path); i++) {
            Point* p = (Point*)ASPathGetNode(path, i);
            display[p->y][p->x] = '*';
        }
        display[start.y][start.x] = 'S';
        display[goal.y][goal.x] = 'E';

        printf("Path (* = path, S = start, E = end):\n");
        printf("   0123456789\n");
        for (int y = 0; y < MAP_HEIGHT; y++) {
            printf(" %d %s\n", y, display[y]);
        }

        printf("\nPath coordinates:\n");
        for (size_t i = 0; i < ASPathGetCount(path); i++) {
            Point* p = (Point*)ASPathGetNode(path, i);
            printf("  Step %zu: (%d, %d)\n", i, p->x, p->y);
        }

        printf("\nTotal cost: %.1f\n", ASPathGetCost(path));
    } else {
        printf("No path found!\n");
    }

    ASPathDestroy(path);

    printf("\nDone!\n");
    return 0;
}
