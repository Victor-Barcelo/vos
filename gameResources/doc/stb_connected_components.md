# stb_connected_components.h

Fast connected components computation on 2D grids with efficient incremental updates.

## Source

- **Repository**: [https://github.com/nothings/stb](https://github.com/nothings/stb)
- **Direct Link**: [https://github.com/nothings/stb/blob/master/stb_connected_components.h](https://github.com/nothings/stb/blob/master/stb_connected_components.h)
- **Version**: 0.96
- **Author**: Sean Barrett

## License

Public domain / MIT dual license. You can choose whichever you prefer.

## Description

stb_connected_components computes connected components on 2D grids for testing reachability between two points. It features fast updates when changing reachability (typically ~0.2ms for a 1024x1024 grid). This is useful for pathfinding preprocessing, determining if two points can reach each other without computing the full path.

## Features

- Fast reachability queries between any two grid points
- Efficient incremental updates when grid state changes
- Orthogonal connectivity only (not diagonal)
- Batch update mode for multiple changes
- Unique component IDs for each connected region
- Memory-efficient storage (~6-7 bytes per grid square)

## Limitations

- Grid squares can only be "open" or "closed" (binary traversability)
- Only orthogonal neighbors are connected (no diagonals)
- Map sizes must be multiples of cluster size (typically 32)
- Maximum practical size depends on configuration

## Performance

On a Core i7-2700K at 3.5 GHz with a 1024x1024 map:

| Operation | Time |
|-----------|------|
| Creating map | 44.85 ms |
| Making one square traversable | 0.27 ms average |
| Making one square untraversable | 0.23 ms average |
| Reachability query | 0.00001 ms average |

Update time is O(N^0.5) on non-degenerate maps, O(N^0.75) on degenerate maps (checkerboards, 50% random).

## Configuration Macros

You **must** define these before including with the implementation:

```c
#define STBCC_GRID_COUNT_X_LOG2    10   // log2 of max X dimension
#define STBCC_GRID_COUNT_Y_LOG2    10   // log2 of max Y dimension
#define STB_CONNECTED_COMPONENTS_IMPLEMENTATION
#include "stb_connected_components.h"
```

This creates an implementation that can run on maps up to 2^10 x 2^10 = 1024x1024.

### Optional Configuration

```c
#define STBCC_CLUSTER_SIZE_X_LOG2   5   // log2 of cluster X size (default: GRID_COUNT_X_LOG2/2)
#define STBCC_CLUSTER_SIZE_Y_LOG2   5   // log2 of cluster Y size (default: GRID_COUNT_Y_LOG2/2)
```

## Memory Usage

Approximately 6-7 bytes per grid square. For a 1024x1024 grid, this is about 7MB.

The memory is allocated as a single worst-case allocation that you pass in.

## API Reference

### Types

```c
typedef struct st_stbcc_grid stbcc_grid;

#define STBCC_NULL_UNIQUE_ID 0xffffffff  // Returned for closed map squares
```

### Initialization Functions

```c
size_t stbcc_grid_sizeof(void);
// Returns the size in bytes needed for the grid data structure
// You allocate this memory and pass it to stbcc_init_grid

void stbcc_init_grid(stbcc_grid *g, unsigned char *map, int w, int h);
// Initialize the grid from a map array
// map[y*w + x] = 0 means traversable, non-0 means solid/blocked
// w, h must be multiples of cluster size (typically 32)
```

### Main Functionality

```c
void stbcc_update_grid(stbcc_grid *g, int x, int y, int solid);
// Update a single grid square's traversability
// solid = 0: make traversable
// solid = non-0: make blocked

int stbcc_query_grid_node_connection(stbcc_grid *g, int x1, int y1, int x2, int y2);
// Query if two grid squares are reachable from each other
// Returns 1 if reachable, 0 if not reachable or either square is blocked
```

### Bonus Functions

```c
void stbcc_update_batch_begin(stbcc_grid *g);
void stbcc_update_batch_end(stbcc_grid *g);
// Wrap multiple stbcc_update_grid calls in these functions
// to compute multiple updates more efficiently
// Cannot make queries while inside a batch

int stbcc_query_grid_open(stbcc_grid *g, int x, int y);
// Query whether a given square is open (traversable)
// Returns non-0 if open, 0 if closed

unsigned int stbcc_get_unique_id(stbcc_grid *g, int x, int y);
// Get a unique ID for the connected component containing (x,y)
// Returns STBCC_NULL_UNIQUE_ID for closed squares
// Note: IDs are not necessarily small or contiguous
```

## Algorithm

The algorithm uses a hierarchical approach:

1. The NxN grid is split into sqrt(N) x sqrt(N) blocks called "clusters"
2. Each cluster independently computes connected components ("clumps") using union-find
3. Clumps maintain adjacency lists to clumps in neighboring clusters
4. A global union-find connects clumps across the whole map
5. Reachability is checked by finding which clump each point belongs to and checking if they share a global component

Updates are efficient because:
- Changing a single grid square only requires recomputing one cluster's local clumps
- Adjacent clusters only update their adjacency lists
- Global connectivity is recomputed from clump connections

## Usage Example

```c
#define STBCC_GRID_COUNT_X_LOG2    10   // 1024 max width
#define STBCC_GRID_COUNT_Y_LOG2    10   // 1024 max height
#define STB_CONNECTED_COMPONENTS_IMPLEMENTATION
#include "stb_connected_components.h"

int main() {
    // Create a 256x256 map (must be multiple of cluster size, typically 32)
    int w = 256, h = 256;
    unsigned char *map = malloc(w * h);

    // Initialize map: 0 = open, non-0 = blocked
    memset(map, 0, w * h);  // All open

    // Add some walls
    for (int x = 50; x < 200; x++) {
        map[100 * w + x] = 1;  // Horizontal wall at y=100
    }

    // Allocate and initialize the grid
    stbcc_grid *grid = malloc(stbcc_grid_sizeof());
    stbcc_init_grid(grid, map, w, h);

    // Query reachability
    int can_reach = stbcc_query_grid_node_connection(grid,
        10, 10,    // Start point
        10, 150);  // End point
    printf("Can reach: %d\n", can_reach);  // Will print 0 (blocked by wall)

    // Open a gap in the wall
    stbcc_update_grid(grid, 100, 100, 0);  // Make traversable

    // Query again
    can_reach = stbcc_query_grid_node_connection(grid, 10, 10, 10, 150);
    printf("Can reach after gap: %d\n", can_reach);  // Will print 1

    // Batch updates for efficiency
    stbcc_update_batch_begin(grid);
    for (int x = 101; x < 110; x++) {
        stbcc_update_grid(grid, x, 100, 0);  // Open more gaps
    }
    stbcc_update_batch_end(grid);

    // Get component ID
    unsigned int id1 = stbcc_get_unique_id(grid, 10, 10);
    unsigned int id2 = stbcc_get_unique_id(grid, 10, 150);
    printf("Same component: %d\n", id1 == id2);  // Will print 1

    free(grid);
    free(map);
    return 0;
}
```

## Use Cases

1. **Pathfinding Preprocessing**: Quickly determine if a path exists before running expensive A* search

2. **Game AI**: Check if an enemy can reach the player

3. **Level Validation**: Verify that all required areas are accessible

4. **Dynamic Terrain**: Efficiently update reachability when doors open/close or terrain changes

5. **Flood Fill Optimization**: Quickly identify all cells in a region

## VOS/TCC Compatibility Notes

### Compatibility Status

This library should be compatible with TCC and VOS with the following considerations:

### Potential Issues

1. **Large Memory Allocation**: The grid requires ~7 bytes per cell. For a 1024x1024 grid, this is ~7MB.

2. **Recursive Functions**: The union-find uses recursion (`stbcc__clump_find`). Deep recursion could be an issue with limited stack.

3. **Bit Manipulation**: Uses bitwise operations extensively, which TCC supports.

4. **Assertions**: Uses `assert()` - ensure this is available or define `NDEBUG`.

### Recommended Configuration for VOS

For memory-constrained environments:

```c
// Smaller grid for VOS
#define STBCC_GRID_COUNT_X_LOG2    8    // 256 max width
#define STBCC_GRID_COUNT_Y_LOG2    8    // 256 max height
#define STB_CONNECTED_COMPONENTS_IMPLEMENTATION
#include "stb_connected_components.h"
```

This reduces memory usage to ~400KB for a 256x256 grid.

### Memory Estimation

| Grid Size | LOG2 Values | Approx Memory |
|-----------|-------------|---------------|
| 64x64     | 6, 6        | ~28 KB |
| 128x128   | 7, 7        | ~112 KB |
| 256x256   | 8, 8        | ~450 KB |
| 512x512   | 9, 9        | ~1.8 MB |
| 1024x1024 | 10, 10      | ~7 MB |

### Integration Example for VOS

```c
// vos_pathfind.c
#define STBCC_GRID_COUNT_X_LOG2    7    // 128 max
#define STBCC_GRID_COUNT_Y_LOG2    7    // 128 max
#define STB_CONNECTED_COMPONENTS_IMPLEMENTATION
#include "stb_connected_components.h"

static stbcc_grid *game_grid;
static unsigned char level_map[128][128];

void init_pathfinding(void) {
    // Initialize level map from game data
    // ...

    game_grid = malloc(stbcc_grid_sizeof());
    stbcc_init_grid(game_grid, &level_map[0][0], 128, 128);
}

int can_enemy_reach_player(int ex, int ey, int px, int py) {
    return stbcc_query_grid_node_connection(game_grid, ex, ey, px, py);
}

void on_door_open(int x, int y) {
    stbcc_update_grid(game_grid, x, y, 0);  // Make traversable
}

void on_door_close(int x, int y) {
    stbcc_update_grid(game_grid, x, y, 1);  // Make blocked
}
```

### Performance Tips

1. Use batch updates when modifying multiple cells
2. Keep map dimensions as powers of 2 for best alignment
3. Pre-compute reachability zones if the map doesn't change often
4. Consider caching query results for frequently-checked paths
