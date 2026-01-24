# Game Development Resources for VOS

Single-file and minimal C libraries compatible with TCC for game development in VOS.

## Graphics (Already Integrated)

| Library | Description | Status |
|---------|-------------|--------|
| **olive.c** | 2D software rendering | Integrated |
| **small3dlib** | 3D software rendering (integer math) | Integrated |

---

## Math Libraries

### linmath.h
- **Description:** Lightweight linear algebra (vectors, matrices, quaternions)
- **Size:** Single header (~800 lines)
- **URL:** https://github.com/datenwolf/linmath.h
- **License:** WTFPL
- **Features:**
  - vec2, vec3, vec4 operations
  - mat4x4 operations
  - Quaternion math
  - Frustum and perspective projection

```c
#include "linmath.h"
vec3 pos = {1.0f, 2.0f, 3.0f};
mat4x4 view;
mat4x4_look_at(view, eye, center, up);
```

### HandmadeMath
- **Description:** Game-oriented math library
- **Size:** Single header (~2500 lines)
- **URL:** https://github.com/HandmadeMath/HandmadeMath
- **License:** Public Domain
- **Features:**
  - SSE optimized (but works without)
  - Vectors, matrices, quaternions
  - Common game math utilities

### cglm
- **Description:** OpenGL-style math for C
- **Size:** Header-only library
- **URL:** https://github.com/recp/cglm
- **License:** MIT
- **Features:**
  - SIMD optimized
  - Full GLM compatibility
  - Pre-built transforms

### Fixed-Point Math (for integer-only)
For TCC without FPU or for deterministic math:
```c
typedef int32_t fixed16;  // 16.16 fixed point
#define FIXED_ONE (1 << 16)
#define FIXED_MUL(a, b) (((int64_t)(a) * (b)) >> 16)
#define FIXED_DIV(a, b) (((int64_t)(a) << 16) / (b))
#define INT_TO_FIXED(x) ((x) << 16)
#define FIXED_TO_INT(x) ((x) >> 16)
```

---

## Physics Engines

### Chipmunk2D
- **Description:** Professional 2D physics engine
- **Size:** ~15 files, ~15K lines total
- **URL:** https://github.com/slembcke/Chipmunk2D
- **License:** MIT
- **Features:**
  - Rigid body dynamics
  - Collision detection (circles, boxes, polygons)
  - Joints and constraints
  - Spatial hashing
- **Used in:** Angry Birds, Night Sky, many others

```c
cpSpace *space = cpSpaceNew();
cpSpaceSetGravity(space, cpv(0, -100));
cpBody *body = cpBodyNew(mass, moment);
cpShape *shape = cpCircleShapeNew(body, radius, cpvzero);
cpSpaceAddBody(space, body);
cpSpaceAddShape(space, shape);
cpSpaceStep(space, dt);
```

### tinyphysicsengine
- **Description:** Minimal 3D physics by drummyfish (small3dlib author)
- **Size:** Single header
- **URL:** https://gitlab.com/drummyfish/tinyphysicsengine
- **License:** CC0
- **Features:**
  - Designed to work with small3dlib
  - Integer math only
  - Basic rigid body dynamics
  - Simple collision shapes

```c
TPE_Body bodies[16];
TPE_World world;
TPE_worldInit(&world, bodies, 16);
TPE_bodyActivate(&bodies[0]);
TPE_worldStep(&world);
```

### Simple 2D Physics (DIY)
Basic physics in ~100 lines:
```c
typedef struct {
    float x, y;       // position
    float vx, vy;     // velocity
    float ax, ay;     // acceleration
    float mass;
    float radius;
} Body;

void physics_step(Body* b, float dt) {
    b->vx += b->ax * dt;
    b->vy += b->ay * dt;
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    b->ay = -9.8f;  // gravity
}

bool circle_collision(Body* a, Body* b) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dist = sqrt(dx*dx + dy*dy);
    return dist < (a->radius + b->radius);
}
```

---

## Collision Detection

### cute_c2.h
- **Description:** 2D collision detection and resolution
- **Size:** Single header (~3000 lines)
- **URL:** https://github.com/RandyGaul/cute_headers
- **License:** Public Domain
- **Features:**
  - Circle, AABB, capsule, polygon collision
  - Ray casting
  - Manifold generation (for physics response)
  - GJK and EPA algorithms

```c
#define CUTE_C2_IMPLEMENTATION
#include "cute_c2.h"

c2Circle player = {{px, py}, 10.0f};
c2AABB wall = {{100, 100}, {150, 200}};

if (c2CircletoAABB(player, wall)) {
    c2Manifold m;
    c2CircletoAABBManifold(player, wall, &m);
    // Resolve collision using m.n and m.depths[0]
}
```

### Simple AABB Collision
```c
typedef struct { float x, y, w, h; } Rect;

bool rect_overlap(Rect a, Rect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

bool point_in_rect(float px, float py, Rect r) {
    return px >= r.x && px < r.x + r.w &&
           py >= r.y && py < r.y + r.h;
}
```

---

## Pathfinding

### A* Algorithm
Simple implementation (~200 lines):
```c
typedef struct Node {
    int x, y;
    int g, h, f;  // costs
    struct Node* parent;
    bool open, closed;
} Node;

Node* astar(int map[W][H], int sx, int sy, int ex, int ey) {
    Node nodes[W][H] = {0};
    Node* open_list[W*H];
    int open_count = 0;

    // Initialize start node
    nodes[sx][sy].g = 0;
    nodes[sx][sy].h = abs(ex-sx) + abs(ey-sy);  // Manhattan
    nodes[sx][sy].f = nodes[sx][sy].h;
    nodes[sx][sy].open = true;
    open_list[open_count++] = &nodes[sx][sy];

    while (open_count > 0) {
        // Find lowest f in open list
        Node* current = pop_lowest_f(open_list, &open_count);
        current->closed = true;

        if (current->x == ex && current->y == ey)
            return current;  // Found path!

        // Check neighbors (4 or 8 directions)
        for (int dir = 0; dir < 4; dir++) {
            int nx = current->x + dx[dir];
            int ny = current->y + dy[dir];

            if (!valid(nx, ny) || map[nx][ny] == WALL)
                continue;
            if (nodes[nx][ny].closed)
                continue;

            int new_g = current->g + 1;
            if (!nodes[nx][ny].open || new_g < nodes[nx][ny].g) {
                nodes[nx][ny].g = new_g;
                nodes[nx][ny].h = abs(ex-nx) + abs(ey-ny);
                nodes[nx][ny].f = new_g + nodes[nx][ny].h;
                nodes[nx][ny].parent = current;

                if (!nodes[nx][ny].open) {
                    nodes[nx][ny].open = true;
                    open_list[open_count++] = &nodes[nx][ny];
                }
            }
        }
    }
    return NULL;  // No path
}
```

### Dijkstra Maps (Roguelike Style)
Great for multiple enemies tracking player:
```c
void build_dijkstra_map(int map[W][H], int dist[W][H], int tx, int ty) {
    // Initialize all to max
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            dist[x][y] = (map[x][y] == WALL) ? -1 : 9999;

    dist[tx][ty] = 0;
    bool changed = true;

    while (changed) {
        changed = false;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (dist[x][y] < 0) continue;

                // Check neighbors
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    if (dist[nx][ny] < 0) continue;

                    if (dist[x][y] + 1 < dist[nx][ny]) {
                        dist[nx][ny] = dist[x][y] + 1;
                        changed = true;
                    }
                }
            }
        }
    }
}

// Enemy just moves toward lower values
void enemy_move(int ex, int ey, int dist[W][H], int* nx, int* ny) {
    int best = dist[ex][ey];
    *nx = ex; *ny = ey;

    for (int d = 0; d < 4; d++) {
        int tx = ex + dx[d], ty = ey + dy[d];
        if (dist[tx][ty] >= 0 && dist[tx][ty] < best) {
            best = dist[tx][ty];
            *nx = tx; *ny = ty;
        }
    }
}
```

### Flow Fields
For many units (RTS games):
```c
typedef struct { int8_t dx, dy; } FlowDir;
FlowDir flow[W][H];

void build_flow_field(int dist[W][H]) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (dist[x][y] < 0) continue;

            int best_d = -1, best_val = dist[x][y];
            for (int d = 0; d < 8; d++) {
                int nx = x + dx8[d], ny = y + dy8[d];
                if (valid(nx, ny) && dist[nx][ny] >= 0 && dist[nx][ny] < best_val) {
                    best_val = dist[nx][ny];
                    best_d = d;
                }
            }

            if (best_d >= 0) {
                flow[x][y].dx = dx8[best_d];
                flow[x][y].dy = dy8[best_d];
            }
        }
    }
}
```

---

## Random Number Generation

### PCG (Recommended)
- **Description:** High-quality, fast PRNG
- **Size:** ~20 lines
- **URL:** https://www.pcg-random.org/
- **License:** Apache 2.0 / MIT

```c
typedef struct { uint64_t state; uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random(pcg32_random_t* rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void pcg32_seed(pcg32_random_t* rng, uint64_t seed, uint64_t seq) {
    rng->state = 0;
    rng->inc = (seq << 1u) | 1u;
    pcg32_random(rng);
    rng->state += seed;
    pcg32_random(rng);
}

// Utilities
int pcg32_range(pcg32_random_t* rng, int min, int max) {
    return min + (pcg32_random(rng) % (max - min + 1));
}

float pcg32_float(pcg32_random_t* rng) {
    return (float)pcg32_random(rng) / (float)UINT32_MAX;
}
```

### Xorshift (Ultra-minimal)
```c
uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *state = x;
}
```

---

## Noise & Procedural Generation

### stb_perlin.h
- **Description:** Perlin noise implementation
- **Size:** Single header (~300 lines)
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// 3D Perlin noise, returns -1.0 to 1.0
float n = stb_perlin_noise3(x * 0.1f, y * 0.1f, z * 0.1f, 0, 0, 0);

// Fractal Brownian Motion (multiple octaves)
float fbm(float x, float y, int octaves) {
    float value = 0, amplitude = 1, frequency = 1;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * stb_perlin_noise3(x * frequency, y * frequency, 0, 0, 0, 0);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}
```

### Simple Value Noise (No Dependencies)
```c
float noise2d(int x, int y, int seed) {
    int n = x + y * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float smooth_noise(float x, float y, int seed) {
    int ix = (int)x, iy = (int)y;
    float fx = x - ix, fy = y - iy;

    float v00 = noise2d(ix, iy, seed);
    float v10 = noise2d(ix+1, iy, seed);
    float v01 = noise2d(ix, iy+1, seed);
    float v11 = noise2d(ix+1, iy+1, seed);

    float i1 = v00 * (1-fx) + v10 * fx;
    float i2 = v01 * (1-fx) + v11 * fx;
    return i1 * (1-fy) + i2 * fy;
}
```

### Cellular Automata (Caves)
```c
void generate_cave(int map[W][H], int seed, int fill_percent, int iterations) {
    uint32_t rng = seed;

    // Random fill
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (x == 0 || x == W-1 || y == 0 || y == H-1)
                map[x][y] = 1;  // Border walls
            else
                map[x][y] = (xorshift32(&rng) % 100) < fill_percent ? 1 : 0;
        }
    }

    // Smooth
    for (int i = 0; i < iterations; i++) {
        int temp[W][H];
        for (int y = 1; y < H-1; y++) {
            for (int x = 1; x < W-1; x++) {
                int walls = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        walls += map[x+dx][y+dy];
                temp[x][y] = (walls > 4) ? 1 : 0;
            }
        }
        memcpy(map, temp, sizeof(temp));
    }
}
```

### BSP Dungeon Generator
```c
typedef struct BSP {
    int x, y, w, h;
    struct BSP *left, *right;
    int room_x, room_y, room_w, room_h;
} BSP;

void bsp_split(BSP* node, int min_size, uint32_t* rng) {
    if (node->w < min_size * 2 && node->h < min_size * 2)
        return;

    bool split_h = (node->w > node->h) ? false :
                   (node->h > node->w) ? true :
                   (xorshift32(rng) % 2);

    if (split_h && node->h >= min_size * 2) {
        int split = min_size + xorshift32(rng) % (node->h - min_size * 2);
        node->left = calloc(1, sizeof(BSP));
        node->right = calloc(1, sizeof(BSP));
        *node->left = (BSP){node->x, node->y, node->w, split, 0, 0};
        *node->right = (BSP){node->x, node->y + split, node->w, node->h - split, 0, 0};
    } else if (node->w >= min_size * 2) {
        int split = min_size + xorshift32(rng) % (node->w - min_size * 2);
        node->left = calloc(1, sizeof(BSP));
        node->right = calloc(1, sizeof(BSP));
        *node->left = (BSP){node->x, node->y, split, node->h, 0, 0};
        *node->right = (BSP){node->x + split, node->y, node->w - split, node->h, 0, 0};
    }

    if (node->left) bsp_split(node->left, min_size, rng);
    if (node->right) bsp_split(node->right, min_size, rng);
}
```

---

## AI & Behavior

### Finite State Machine
```c
typedef enum {
    STATE_IDLE,
    STATE_PATROL,
    STATE_CHASE,
    STATE_ATTACK,
    STATE_FLEE
} AIState;

typedef struct {
    AIState state;
    float x, y;
    float health;
    int patrol_index;
    float state_timer;
} Enemy;

void enemy_update(Enemy* e, float px, float py, float dt) {
    float dist = sqrtf((e->x-px)*(e->x-px) + (e->y-py)*(e->y-py));
    e->state_timer += dt;

    switch (e->state) {
        case STATE_IDLE:
            if (dist < 150) e->state = STATE_CHASE;
            else if (e->state_timer > 3.0f) {
                e->state = STATE_PATROL;
                e->state_timer = 0;
            }
            break;

        case STATE_PATROL:
            move_to_patrol_point(e);
            if (dist < 150) e->state = STATE_CHASE;
            if (reached_patrol_point(e)) {
                e->patrol_index = (e->patrol_index + 1) % NUM_PATROL_POINTS;
            }
            break;

        case STATE_CHASE:
            move_toward(e, px, py, 50.0f * dt);
            if (dist < 30) e->state = STATE_ATTACK;
            if (dist > 200) e->state = STATE_IDLE;
            if (e->health < 20) e->state = STATE_FLEE;
            break;

        case STATE_ATTACK:
            if (e->state_timer > 0.5f) {
                deal_damage_to_player();
                e->state_timer = 0;
            }
            if (dist > 35) e->state = STATE_CHASE;
            break;

        case STATE_FLEE:
            move_away(e, px, py, 60.0f * dt);
            if (dist > 300) e->state = STATE_IDLE;
            break;
    }
}
```

### Behavior Trees (Simple)
```c
typedef enum { BT_SUCCESS, BT_FAILURE, BT_RUNNING } BTStatus;
typedef BTStatus (*BTNode)(Enemy* e, float dt);

BTStatus bt_sequence(Enemy* e, float dt, BTNode* children, int count) {
    for (int i = 0; i < count; i++) {
        BTStatus s = children[i](e, dt);
        if (s != BT_SUCCESS) return s;
    }
    return BT_SUCCESS;
}

BTStatus bt_selector(Enemy* e, float dt, BTNode* children, int count) {
    for (int i = 0; i < count; i++) {
        BTStatus s = children[i](e, dt);
        if (s != BT_FAILURE) return s;
    }
    return BT_FAILURE;
}

// Example nodes
BTStatus bt_is_player_visible(Enemy* e, float dt) {
    return can_see_player(e) ? BT_SUCCESS : BT_FAILURE;
}

BTStatus bt_chase_player(Enemy* e, float dt) {
    move_toward_player(e, dt);
    return BT_RUNNING;
}
```

### Boids Flocking
```c
typedef struct { float x, y, vx, vy; } Boid;

void update_boids(Boid* flock, int count, float dt) {
    for (int i = 0; i < count; i++) {
        float sep_x = 0, sep_y = 0;  // Separation
        float ali_x = 0, ali_y = 0;  // Alignment
        float coh_x = 0, coh_y = 0;  // Cohesion
        int neighbors = 0;

        for (int j = 0; j < count; j++) {
            if (i == j) continue;
            float dx = flock[j].x - flock[i].x;
            float dy = flock[j].y - flock[i].y;
            float dist = sqrtf(dx*dx + dy*dy);

            if (dist < 50) {  // Neighbor radius
                // Separation (avoid crowding)
                if (dist < 20) {
                    sep_x -= dx / dist;
                    sep_y -= dy / dist;
                }
                // Alignment (match velocity)
                ali_x += flock[j].vx;
                ali_y += flock[j].vy;
                // Cohesion (move toward center)
                coh_x += flock[j].x;
                coh_y += flock[j].y;
                neighbors++;
            }
        }

        if (neighbors > 0) {
            ali_x /= neighbors; ali_y /= neighbors;
            coh_x = coh_x / neighbors - flock[i].x;
            coh_y = coh_y / neighbors - flock[i].y;

            flock[i].vx += sep_x * 0.05f + (ali_x - flock[i].vx) * 0.02f + coh_x * 0.01f;
            flock[i].vy += sep_y * 0.05f + (ali_y - flock[i].vy) * 0.02f + coh_y * 0.01f;
        }

        // Limit speed
        float speed = sqrtf(flock[i].vx*flock[i].vx + flock[i].vy*flock[i].vy);
        if (speed > 100) {
            flock[i].vx = flock[i].vx / speed * 100;
            flock[i].vy = flock[i].vy / speed * 100;
        }

        flock[i].x += flock[i].vx * dt;
        flock[i].y += flock[i].vy * dt;
    }
}
```

---

## Data Structures

### stb_ds.h (Highly Recommended)
- **Description:** Dynamic arrays and hash maps
- **Size:** Single header (~2000 lines)
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Dynamic array
int* arr = NULL;
arrput(arr, 10);
arrput(arr, 20);
arrput(arr, 30);
printf("%d\n", arr[1]);  // 20
printf("%d\n", arrlen(arr));  // 3
arrfree(arr);

// Hash map (string keys)
struct { char* key; int value; }* map = NULL;
shput(map, "health", 100);
shput(map, "mana", 50);
int hp = shget(map, "health");  // 100
shfree(map);

// Hash map (int keys)
struct { int key; float value; }* imap = NULL;
hmput(imap, 42, 3.14f);
float v = hmget(imap, 42);
hmfree(imap);
```

---

## Entity Component System (Simple)

```c
#define MAX_ENTITIES 1024
#define COMP_POSITION  (1 << 0)
#define COMP_VELOCITY  (1 << 1)
#define COMP_SPRITE    (1 << 2)
#define COMP_HEALTH    (1 << 3)

typedef struct {
    uint32_t mask[MAX_ENTITIES];
    float x[MAX_ENTITIES], y[MAX_ENTITIES];
    float vx[MAX_ENTITIES], vy[MAX_ENTITIES];
    int sprite_id[MAX_ENTITIES];
    int health[MAX_ENTITIES];
    int entity_count;
} World;

int create_entity(World* w) {
    return w->entity_count++;
}

void add_position(World* w, int e, float x, float y) {
    w->mask[e] |= COMP_POSITION;
    w->x[e] = x; w->y[e] = y;
}

void system_movement(World* w, float dt) {
    uint32_t required = COMP_POSITION | COMP_VELOCITY;
    for (int e = 0; e < w->entity_count; e++) {
        if ((w->mask[e] & required) == required) {
            w->x[e] += w->vx[e] * dt;
            w->y[e] += w->vy[e] * dt;
        }
    }
}
```

---

## Game Loop Structure

```c
#define TARGET_FPS 60
#define FRAME_TIME (1.0f / TARGET_FPS)

typedef struct {
    bool running;
    float dt;
    uint32_t last_time;
    // Game state...
} Game;

void game_init(Game* g);
void game_input(Game* g);
void game_update(Game* g, float dt);
void game_render(Game* g);

int main() {
    Game game = {0};
    game_init(&game);
    game.running = true;
    game.last_time = get_time_ms();

    while (game.running) {
        uint32_t now = get_time_ms();
        game.dt = (now - game.last_time) / 1000.0f;
        game.last_time = now;

        // Cap delta time to avoid spiral of death
        if (game.dt > 0.1f) game.dt = 0.1f;

        game_input(&game);
        game_update(&game, game.dt);
        game_render(&game);

        // Frame limiting
        uint32_t frame_end = get_time_ms();
        uint32_t frame_duration = frame_end - now;
        if (frame_duration < (uint32_t)(FRAME_TIME * 1000)) {
            sleep_ms((uint32_t)(FRAME_TIME * 1000) - frame_duration);
        }
    }

    return 0;
}
```

---

## Recommended Library Bundle for VOS

```
/usr/include/gamedev/
├── linmath.h           # Math
├── cute_c2.h           # Collision
├── stb_ds.h            # Data structures
├── stb_perlin.h        # Noise
├── pcg.h               # Random numbers
├── chipmunk/           # 2D Physics (optional, larger)
│   ├── chipmunk.h
│   └── ...
└── tinyphysicsengine.h # 3D Physics
```

## See Also

- [system_libraries.md](system_libraries.md) - System utilities
- [data_formats.md](data_formats.md) - File format parsers
- [text_processing.md](text_processing.md) - String and text utilities
