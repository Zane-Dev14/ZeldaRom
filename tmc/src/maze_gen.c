#include "global.h"
#include "map.h"
#include "functions.h"
#include "room.h"
#include "asm.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

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
} MazeGenState;

static const int DIR_DX[4] = { 0,  1,  0, -1 };
static const int DIR_DY[4] = { -1,  0,  1,  0 };
static const u8  DIR_BIT[4] = { 1, 2, 4, 8 };
static const u8  OPP_BIT[4] = { 4, 8, 1, 2 };

static uint32_t local_rng_next(MazeGenState* s)
{
    uint32_t x = s->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng_state = x ? x : 0xDEADBEEFu;
    return s->rng_state;
}

static uint32_t local_rng_range(MazeGenState* s, uint32_t n)
{
    uint32_t r;
    if (n == 0)
        return 0;
    r = local_rng_next(s);
    while (r >= n)
        r -= n;
    return r;
}

static void init_cells(MazeGenState* s)
{
    int y, x;
    for (y = 0; y < MAZE_CELLS_Y; ++y) {
        for (x = 0; x < MAZE_CELLS_X; ++x) {
            s->maze[y][x].visited = 0;
            s->maze[y][x].walls = 15;
        }
    }
    s->stackTop = 0;
}

static void push_cell(MazeGenState* s, int x, int y)
{
    s->stack[s->stackTop].x = x;
    s->stack[s->stackTop].y = y;
    s->stackTop++;
}

static CellPos pop_cell(MazeGenState* s)
{
    s->stackTop--;
    return s->stack[s->stackTop];
}

static int count_unvisited_neighbors(
    MazeGenState* s, int x, int y, int* buf)
{
    int n = 0;
    int d, nx, ny;

    for (d = 0; d < 4; ++d) {
        nx = x + DIR_DX[d];
        ny = y + DIR_DY[d];
        if (nx >= 0 && nx < MAZE_CELLS_X &&
            ny >= 0 && ny < MAZE_CELLS_Y) {
            if (!s->maze[ny][nx].visited)
                buf[n++] = d;
            }
    }
    return n;
}

static void carve_between(MazeGenState* s, int x, int y, int d)
{
    int nx = x + DIR_DX[d];
    int ny = y + DIR_DY[d];
    s->maze[y][x].walls &= ~DIR_BIT[d];
    s->maze[ny][nx].walls &= ~OPP_BIT[d];
}

static void generate_cells(MazeGenState* s, uint32_t seed)
{
    int sx, sy;
    int dirs[4];
    int n, ri, d;
    int nx, ny;
    CellPos cur;


    sx = (int)local_rng_range(s, MAZE_CELLS_X);
    sy = (int)local_rng_range(s, MAZE_CELLS_Y);

    s->maze[sy][sx].visited = 1;
    push_cell(s, sx, sy);

    while (s->stackTop > 0) {
        cur = s->stack[s->stackTop - 1];
        n = count_unvisited_neighbors(s, cur.x, cur.y, dirs);
        if (n == 0) {
            pop_cell(s);
            continue;
        }
        ri = (int)local_rng_range(s, (uint32_t)n);
        d = dirs[ri];
        carve_between(s, cur.x, cur.y, d);
        nx = cur.x + DIR_DX[d];
        ny = cur.y + DIR_DY[d];
        s->maze[ny][nx].visited = 1;
        push_cell(s, nx, ny);
    }
}

static int room_tile_width(void)
{
    int w = (gRoomControls.width + 7) >> 3;
    if (w < 1)  w = 1;
    if (w > 64) w = 64;
    return w;
}

static int room_tile_height(void)
{
    int h = (gRoomControls.height + 7) >> 3;
    if (h < 1)  h = 1;
    if (h > 64) h = 64;
    return h;
}
__attribute__((section(".maze_text")))
void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed)
{
    MazeGenState state;
    MazeGenState* s = &state;

    MapLayer* layer;
    int roomW, roomH;
    int center_tile, wall_tile;
    int tileAreaW, tileAreaH;
    int startX, startY;
    int tx, ty, px, py;
    int cx, cy;

    layer = GetLayerByIndex(layerIndex);
    if (!layer || !layer->mapData)
        return;

    s->rng_state = seed ? seed : 0xA5A5A5A5u;
    init_cells(s);

    roomW = room_tile_width();
    roomH = room_tile_height();

    {
        int centerX = roomW >> 1;
        int centerY = roomH >> 1;
        center_tile = layer->mapData[centerY * 64 + centerX] & 0x0FFF;
        if (center_tile == 0)
            center_tile = 0x3001;
    }

    wall_tile = center_tile + 0x10;

    generate_cells(s, seed);

    tileAreaW = MAZE_CELLS_X * 2 + 1;
    tileAreaH = MAZE_CELLS_Y * 2 + 1;

    if (tileAreaW > roomW) tileAreaW = roomW;
    if (tileAreaH > roomH) tileAreaH = roomH;

    startX = (roomW > tileAreaW) ? ((roomW - tileAreaW) >> 1) : 0;
    startY = (roomH > tileAreaH) ? ((roomH - tileAreaH) >> 1) : 0;

    for (ty = 0; ty < tileAreaH; ++ty) {
        for (tx = 0; tx < tileAreaW; ++tx) {
            px = startX + tx;
            py = startY + ty;
            if (px < roomW && py < roomH)
                layer->mapData[py * 64 + px] = center_tile;
        }
    }

    for (cy = 0; cy < MAZE_CELLS_Y; ++cy) {
        for (cx = 0; cx < MAZE_CELLS_X; ++cx) {
            tx = startX + (cx * 2 + 1);
            ty = startY + (cy * 2 + 1);

            if (tx >= 0 && tx < roomW && ty >= 0 && ty < roomH) {
                if ((s->maze[cy][cx].walls & 1) && ty > 0)
                    layer->mapData[(ty - 1) * 64 + tx] = wall_tile;
                if ((s->maze[cy][cx].walls & 2) && tx < roomW - 1)
                    layer->mapData[ty * 64 + (tx + 1)] = wall_tile;
                if ((s->maze[cy][cx].walls & 4) && ty < roomH - 1)
                    layer->mapData[(ty + 1) * 64 + tx] = wall_tile;
                if ((s->maze[cy][cx].walls & 8) && tx > 0)
                    layer->mapData[ty * 64 + (tx - 1)] = wall_tile;
            }
        }
    }

    gUpdateVisibleTiles = 1;
}
