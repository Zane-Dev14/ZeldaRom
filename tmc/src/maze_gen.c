#include "global.h"
#include "map.h"
#include "functions.h"
#include "room.h"
#include "asm.h"
#include <string.h>
#include <stddef.h>

extern u8 gUpdateVisibleTiles;

#define MAZE_CELLS_X 10
#define MAZE_CELLS_Y 10

typedef struct {
    u8 visited;
    u8 walls;
} MazeCell;

typedef struct {
    int x;
    int y;
} CellPos;

typedef struct {
    uint32_t rng_state;
    MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X];
    CellPos stack[MAZE_CELLS_X * MAZE_CELLS_Y];
    int stackTop;
    int DIR_DX[4];
    int DIR_DY[4];
    u8  DIR_BIT[4];
    u8  OPP_BIT[4];
} MazeGenState;

extern unsigned char _mazeState_start[];
extern unsigned char _mazeState_end[];
#define g_mazeState ((MazeGenState*)_mazeState_start)

/* NOT const - goes to .data in EW-RAM, safe from ROM layout issues */
/* remove any initializers — these must be uninitialised so they go to .bss */
/* ROM-resident direction tables (no EW-RAM footprint) */

/* Rest of your code unchanged */
static uint32_t local_rng_next(void) {
    uint32_t x = g_mazeState->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_mazeState->rng_state = x ? x : 0xDEADBEEFu;
    return g_mazeState->rng_state;
}

static uint32_t local_rng_range(uint32_t n) {
    uint32_t r;
    if (n == 0) return 0;
    r = local_rng_next();
    while (r >= n) r -= n;
    return r;
}

static void init_cells(void) {
    int y, x;
    for (y = 0; y < MAZE_CELLS_Y; ++y) {
        for (x = 0; x < MAZE_CELLS_X; ++x) {
            g_mazeState->maze[y][x].visited = 0;
            g_mazeState->maze[y][x].walls = 15;
        }
    }
    g_mazeState->stackTop = 0;
}

static void push_cell(int x, int y) {
    g_mazeState->stack[g_mazeState->stackTop].x = x;
    g_mazeState->stack[g_mazeState->stackTop].y = y;
    g_mazeState->stackTop++;
}

static CellPos pop_cell(void) {
    g_mazeState->stackTop--;
    return g_mazeState->stack[g_mazeState->stackTop];
}

static int count_unvisited_neighbors(int x, int y, int *buf) {
    int n, d, nx, ny;
    n = 0;
    for (d = 0; d < 4; ++d) {
        nx = x +  g_mazeState->DIR_DX[d];
        ny = y +  g_mazeState->DIR_DY[d];
        if (nx >= 0 && nx < MAZE_CELLS_X && ny >= 0 && ny < MAZE_CELLS_Y) {
            if (!g_mazeState->maze[ny][nx].visited) buf[n++] = d;
        }
    }
    return n;
}

static void carve_between(int x, int y, int d) {
    int nx, ny;

    nx = x +  g_mazeState->DIR_DX[d];
    ny = y +  g_mazeState->DIR_DY[d];
    g_mazeState->maze[y][x].walls &= ~ g_mazeState->DIR_BIT[d];
    g_mazeState->maze[ny][nx].walls &= ~ g_mazeState->OPP_BIT[d];
}

static void generate_cells(uint32_t seed) {
    int sx, sy, dirs[4], n, ri, d, nx, ny;
    CellPos cur;

    g_mazeState->rng_state = seed ? seed : 0xA5A5A5A5u;
    init_cells();

    /* at top of generate_cells() after seed set */
    g_mazeState->DIR_DX[0] = 0; g_mazeState->DIR_DX[1] = 1; g_mazeState->DIR_DX[2] = 0; g_mazeState->DIR_DX[3] = -1;
    g_mazeState->DIR_DY[0] = -1; g_mazeState->DIR_DY[1] = 0; g_mazeState->DIR_DY[2] = 1; g_mazeState->DIR_DY[3] = 0;
    g_mazeState->DIR_BIT[0] = 1; g_mazeState->DIR_BIT[1] = 2; g_mazeState->DIR_BIT[2] = 4; g_mazeState->DIR_BIT[3] = 8;
    g_mazeState->OPP_BIT[0] = 4; g_mazeState->OPP_BIT[1] = 8; g_mazeState->OPP_BIT[2] = 1; g_mazeState->OPP_BIT[3] = 2;

    sx = (int)local_rng_range(MAZE_CELLS_X);
    sy = (int)local_rng_range(MAZE_CELLS_Y);

    g_mazeState->maze[sy][sx].visited = 1;
    push_cell(sx, sy);

    while (g_mazeState->stackTop > 0) {
        cur = g_mazeState->stack[g_mazeState->stackTop - 1];
        n = count_unvisited_neighbors(cur.x, cur.y, dirs);
        if (n == 0) {
            pop_cell();
            continue;
        }
        ri = (int)local_rng_range((uint32_t)n);
        d = dirs[ri];
        carve_between(cur.x, cur.y, d);
        nx = cur.x +  g_mazeState->DIR_DX[d];
        ny = cur.y +  g_mazeState->DIR_DY[d];
        g_mazeState->maze[ny][nx].visited = 1;
        push_cell(nx, ny);
    }
}

static int room_tile_width(void) {
    int w = (gRoomControls.width + 7) >> 3;
    if (w < 1) w = 1;
    if (w > 64) w = 64;
    return w;
}

static int room_tile_height(void) {
    int h = (gRoomControls.height + 7) >> 3;
    if (h < 1) h = 1;
    if (h > 64) h = 64;
    return h;
}

void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed) {
    MapLayer *layer;
    int roomW, roomH;
    u16 center_tile, wall_tile;
    int cy, cx, tx, ty, tileAreaW, tileAreaH, startX, startY, px, py;

    g_mazeState->stackTop = 0;
    g_mazeState->rng_state = seed ? seed : 0xA5A5A5A5u;

    layer = GetLayerByIndex(layerIndex);
    if (!layer || !layer->mapData)
        return;

    roomW = room_tile_width();
    roomH = room_tile_height();

    {
        int centerX = roomW / 2;
        int centerY = roomH / 2;
        center_tile = layer->mapData[centerY * 64 + centerX] & 0x0FFF;
        if (center_tile == 0)
            center_tile = 0x3001;
    }

    wall_tile = center_tile + 0x10;

    generate_cells(seed);

    tileAreaW = MAZE_CELLS_X * 2 + 1;
    tileAreaH = MAZE_CELLS_Y * 2 + 1;

    if (tileAreaW > roomW)
        tileAreaW = roomW;
    if (tileAreaH > roomH)
        tileAreaH = roomH;

    startX = (roomW > tileAreaW) ? ((roomW - tileAreaW) >> 1) : 0;
    startY = (roomH > tileAreaH) ? ((roomH - tileAreaH) >> 1) : 0;

    for (ty = 0; ty < tileAreaH; ++ty) {
        for (tx = 0; tx < tileAreaW; ++tx) {
            px = startX + tx;
            py = startY + ty;
            if (px < roomW && py < roomH) {
                layer->mapData[py * 64 + px] = center_tile;
            }
        }
    }

    for (cy = 0; cy < MAZE_CELLS_Y; ++cy) {
        for (cx = 0; cx < MAZE_CELLS_X; ++cx) {
            tx = startX + (cx * 2 + 1);
            ty = startY + (cy * 2 + 1);

            if (tx >= 0 && tx < roomW && ty >= 0 && ty < roomH) {
                if (g_mazeState->maze[cy][cx].walls & 1 && ty > 0)
                    layer->mapData[(ty - 1) * 64 + tx] = wall_tile;
                if (g_mazeState->maze[cy][cx].walls & 2 && tx < roomW - 1)
                    layer->mapData[ty * 64 + (tx + 1)] = wall_tile;
                if (g_mazeState->maze[cy][cx].walls & 4 && ty < roomH - 1)
                    layer->mapData[(ty + 1) * 64 + tx] = wall_tile;
                if (g_mazeState->maze[cy][cx].walls & 8 && tx > 0)
                    layer->mapData[ty * 64 + (tx - 1)] = wall_tile;
            }
        }
    }

    gUpdateVisibleTiles = 1;
}
