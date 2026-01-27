# AStar - A* Pathfinding Algorithm

## Description

AStar is a C implementation of the A* (A-star) pathfinding algorithm. It provides a generic, callback-based approach to pathfinding that works with any spatial data representation, not just grids. The library uses a binary heap to implement the priority queue and an indexed array for fast lookups of previously visited nodes.

## Original Source

- **Repository**: [https://github.com/BigZaphod/AStar](https://github.com/BigZaphod/AStar)
- **Author**: Sean Heber (BigZaphod)
- **Year**: 2012

## License

BSD 3-Clause License

```
Copyright (c) 2012, Sean Heber. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of Sean Heber nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.
```

## Features

- Generic node handling (works with any data structure)
- Callback-based design for maximum flexibility
- No assumptions about spatial data representation
- Binary heap-based priority queue for efficiency
- Supports custom heuristics and cost functions
- Early exit capability for optimization
- Memory-managed path results
- Path copying support

## API Reference

### Types

#### `ASNeighborList`
Opaque type representing a list of neighbor nodes. Used within the `nodeNeighbors` callback to add neighboring nodes.

#### `ASPath`
Opaque type representing a computed path. Contains the sequence of nodes from start to goal.

#### `ASPathNodeSource`
Configuration structure for the pathfinding algorithm:

```c
typedef struct {
    size_t nodeSize;
    void (*nodeNeighbors)(ASNeighborList neighbors, void *node, void *context);
    float (*pathCostHeuristic)(void *fromNode, void *toNode, void *context);
    int (*earlyExit)(size_t visitedCount, void *visitingNode, void *goalNode, void *context);
    int (*nodeComparator)(void *node1, void *node2, void *context);
} ASPathNodeSource;
```

| Field | Description |
|-------|-------------|
| `nodeSize` | Size of the node structure in bytes (required) |
| `nodeNeighbors` | Callback to add neighboring nodes (required) |
| `pathCostHeuristic` | Estimated cost between nodes; returns 0 if not specified (optional) |
| `earlyExit` | Early termination control; return 1=success, -1=failure, 0=continue (optional) |
| `nodeComparator` | Node comparison for sorting; uses memcmp if not specified (optional) |

### Functions

#### `ASNeighborListAdd`
```c
void ASNeighborListAdd(ASNeighborList neighbors, void *node, float edgeCost);
```
Add a neighboring node with its edge cost. Called within the `nodeNeighbors` callback.

#### `ASPathCreate`
```c
ASPath ASPathCreate(const ASPathNodeSource *nodeSource, void *context,
                    void *startNode, void *goalNode);
```
Create a path from start to goal node. Returns NULL if no path exists or on error.

- If `goalNode` is NULL, searches the entire graph and returns the cheapest deepest path
- `context` is optional and passed through to all callbacks
- `startNode` and `nodeSource` are required

#### `ASPathDestroy`
```c
void ASPathDestroy(ASPath path);
```
Free memory allocated for a path. Must be called for paths created with `ASPathCreate()`.

#### `ASPathCopy`
```c
ASPath ASPathCopy(ASPath path);
```
Create a copy of a path. The copy must also be destroyed with `ASPathDestroy()`.

#### `ASPathGetCost`
```c
float ASPathGetCost(ASPath path);
```
Get the total cost of the path. Returns INFINITY if path is NULL.

#### `ASPathGetCount`
```c
size_t ASPathGetCount(ASPath path);
```
Get the number of nodes in the path. Returns 0 if path is NULL.

#### `ASPathGetNode`
```c
void *ASPathGetNode(ASPath path, size_t index);
```
Get a pointer to a node in the path by index. Returns NULL if path is NULL or index is out of bounds.

## Usage Examples

### Basic 2D Grid Pathfinding

```c
#include "AStar.h"
#include <math.h>

// Define a position node
typedef struct {
    int x;
    int y;
} Position;

// Map context
typedef struct {
    int width;
    int height;
    int *blocked;  // 1 = blocked, 0 = passable
} Map;

// Check if position is walkable
static int isWalkable(Map *map, int x, int y) {
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
        return 0;
    return !map->blocked[y * map->width + x];
}

// Add neighboring tiles
static void nodeNeighbors(ASNeighborList neighbors, void *node, void *context) {
    Position *pos = (Position *)node;
    Map *map = (Map *)context;

    // 4-directional movement
    static const int dx[] = {0, 1, 0, -1};
    static const int dy[] = {-1, 0, 1, 0};

    for (int i = 0; i < 4; i++) {
        int nx = pos->x + dx[i];
        int ny = pos->y + dy[i];

        if (isWalkable(map, nx, ny)) {
            Position neighbor = {nx, ny};
            ASNeighborListAdd(neighbors, &neighbor, 1.0f);
        }
    }
}

// Manhattan distance heuristic
static float heuristic(void *fromNode, void *toNode, void *context) {
    Position *from = (Position *)fromNode;
    Position *to = (Position *)toNode;
    return (float)(abs(to->x - from->x) + abs(to->y - from->y));
}

// Find path
void findPath(Map *map, int startX, int startY, int goalX, int goalY) {
    ASPathNodeSource source = {
        .nodeSize = sizeof(Position),
        .nodeNeighbors = nodeNeighbors,
        .pathCostHeuristic = heuristic,
        .earlyExit = NULL,
        .nodeComparator = NULL
    };

    Position start = {startX, startY};
    Position goal = {goalX, goalY};

    ASPath path = ASPathCreate(&source, map, &start, &goal);

    if (path) {
        printf("Path found! Cost: %.1f, Steps: %zu\n",
               ASPathGetCost(path), ASPathGetCount(path));

        for (size_t i = 0; i < ASPathGetCount(path); i++) {
            Position *p = (Position *)ASPathGetNode(path, i);
            printf("  Step %zu: (%d, %d)\n", i, p->x, p->y);
        }

        ASPathDestroy(path);
    } else {
        printf("No path found!\n");
    }
}
```

### 8-Directional Movement with Diagonal Costs

```c
static void nodeNeighbors8Dir(ASNeighborList neighbors, void *node, void *context) {
    Position *pos = (Position *)node;
    Map *map = (Map *)context;

    // 8 directions: N, NE, E, SE, S, SW, W, NW
    static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    for (int i = 0; i < 8; i++) {
        int nx = pos->x + dx[i];
        int ny = pos->y + dy[i];

        if (isWalkable(map, nx, ny)) {
            Position neighbor = {nx, ny};
            // Diagonal movement costs sqrt(2), cardinal costs 1
            float cost = (dx[i] != 0 && dy[i] != 0) ? 1.414f : 1.0f;
            ASNeighborListAdd(neighbors, &neighbor, cost);
        }
    }
}

// Euclidean distance heuristic (better for 8-directional)
static float euclideanHeuristic(void *fromNode, void *toNode, void *context) {
    Position *from = (Position *)fromNode;
    Position *to = (Position *)toNode;
    float dx = (float)(to->x - from->x);
    float dy = (float)(to->y - from->y);
    return sqrtf(dx * dx + dy * dy);
}
```

## VOS/TCC Compatibility Notes

### Compilation
The library compiles cleanly with TCC (Tiny C Compiler) used in VOS:

```c
#include "AStar.h"
#include "AStar.c"
```

### Dependencies
- `<stdlib.h>` - for malloc, realloc, calloc, free
- `<math.h>` - for floorf (used in heap operations)
- `<string.h>` - for memcpy, memcmp, memset
- `<stdint.h>` - for int8_t

### Memory Considerations
- The library dynamically allocates memory for internal structures
- Always call `ASPathDestroy()` to prevent memory leaks
- Memory usage scales with the number of visited nodes

### TCC-Specific Notes
- Uses designated initializers (C99), which TCC supports
- Uses compound literals for internal Node structures
- No platform-specific dependencies

### Performance Tips for VOS
- Use appropriate heuristics (Manhattan for 4-dir, Euclidean for 8-dir)
- Consider implementing `earlyExit` for large maps
- Pre-compute neighbor relationships if possible
- Keep node structures small to reduce memory copying
