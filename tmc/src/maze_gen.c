#include "global.h"
#include "room.h"

#define MAZE_CELLS_X 6
#define MAZE_CELLS_Y 6
extern u8 gUpdateVisibleTiles;

extern u16 GetTileTypeAtRoomTile(int x, int y, int layerIndex);
typedef struct {
    u8 visited;
    u8 walls;
} MazeCell;

typedef struct {
    int x;
    int y;
} CellPos;

typedef struct {
    u32 rng_state;
    MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X];
    CellPos stack[MAZE_CELLS_X * MAZE_CELLS_Y];
    int stackTop;
} MazeGenState;

static const int DIR_DX[4] = {0, 1, 0, -1};
static const int DIR_DY[4] = {-1, 0, 1, 0};
static const u8 DIR_BIT[4] = {1, 2, 4, 8};
static const u8 OPP_BIT[4] = {4, 8, 1, 2};

static u32 local_rng_next(MazeGenState* s) {
    u32 x = s->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng_state = x ? x : 0xDEADBEEFu;
    return s->rng_state;
}
static u32 local_rng_range(MazeGenState* s, u32 n) {
    u32 r;

    if (n == 0)
        return 0;

    r = local_rng_next(s) >> 28; // 0..15

    if (r >= n)
        r -= n;
    if (r >= n)
        r -= n;

    return r;
}



static void init_cells(MazeGenState* s) {
    int y, x;
    for (y = 0; y < MAZE_CELLS_Y; ++y) {
        for (x = 0; x < MAZE_CELLS_X; ++x) {
            s->maze[y][x].visited = 0;
            s->maze[y][x].walls = 15;
        }
    }
    s->stackTop = 0;
}

static int push_cell(MazeGenState* s, int x, int y) {
    if (s->stackTop >= MAZE_CELLS_X * MAZE_CELLS_Y)
        return 0;

    s->stack[s->stackTop].x = x;
    s->stack[s->stackTop].y = y;
    s->stackTop++;
    return 1;
}


static CellPos pop_cell(MazeGenState* s) {
    if (s->stackTop <= 0) {
        CellPos z = {0,0};
        return z;
    }
    s->stackTop--;
    return s->stack[s->stackTop];
}

static int count_unvisited_neighbors(MazeGenState* s, int x, int y, int* buf) {
    int n = 0, d, nx, ny;
    for (d = 0; d < 4; ++d) {
        nx = x + DIR_DX[d];
        ny = y + DIR_DY[d];
        if (nx >= 0 && nx < MAZE_CELLS_X && ny >= 0 && ny < MAZE_CELLS_Y) {
            if (!s->maze[ny][nx].visited) buf[n++] = d;
        }
    }
    return n;
}

static void carve_between(MazeGenState* s, int x, int y, int d) {
    int nx = x + DIR_DX[d];
    int ny = y + DIR_DY[d];
    s->maze[y][x].walls &= ~DIR_BIT[d];
    s->maze[ny][nx].walls &= ~OPP_BIT[d];
}
/*
static void generate_cells(MazeGenState* s, u32 seed) {
    int sx, sy, dirs[4], n, ri, d, nx, ny;
    CellPos cur;

    s->rng_state = seed ? seed : 0xA5A5A5A5u;
    init_cells(s);

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
        ri = (int)local_rng_range(s, (u32)n);
        d = dirs[ri];
        carve_between(s, cur.x, cur.y, d);
        nx = cur.x + DIR_DX[d];
        ny = cur.y + DIR_DY[d];
        s->maze[ny][nx].visited = 1;
        push_cell(s, nx, ny);
    }
}*/
static void DrawWallBlock(u16 tile, int x, int y, int layer) {
    SetTileType(tile, TILE_POS(x, y), layer);
    SetTileType(tile, TILE_POS((x+1), y), layer);
    SetTileType(tile, TILE_POS(x, (y+1)), layer);
    SetTileType(tile, TILE_POS((x+1), (y+1)), layer);
}

static void generate_cells(MazeGenState* s, u32 seed) {
    int sx, sy, dirs[4], n, ri, d, nx, ny;
    CellPos cur;
    int iter = 0;

    s->rng_state = seed ? seed : 0xA5A5A5A5u;
    init_cells(s);

    sx = local_rng_range(s, MAZE_CELLS_X);
    sy = local_rng_range(s, MAZE_CELLS_Y);

    s->maze[sy][sx].visited = 1;
    push_cell(s, sx, sy);

    while (s->stackTop > 0) {

        if (++iter > MAZE_DEBUG_LIMIT)
            break;

        cur = s->stack[s->stackTop - 1];

        n = count_unvisited_neighbors(s, cur.x, cur.y, dirs);
        if (n <= 0) {
            pop_cell(s);
            continue;
        }

        d = dirs[local_rng_range(s, n)];

        nx = cur.x + DIR_DX[d];
        ny = cur.y + DIR_DY[d];

        carve_between(s, cur.x, cur.y, d);
        s->maze[ny][nx].visited = 1;

        if (!push_cell(s, nx, ny)) {
            pop_cell(s);
        }
    }

}

#include "common.h"
#include "tiles.h"
static u16 DeriveWallTile(u16 floorTile) {
    return floorTile + 1;
}

static u16 SampleWallTile(int layerIndex, int roomW, int roomH) {
    u16 tile;

    /* Sample near top edge (likely wall) */
    tile = GetTileTypeAtRoomTile(1, 1, layerIndex);
    if (tile && tile < 0x400)
        return tile;

    /* Sample left edge */
    tile = GetTileTypeAtRoomTile(1, roomH - 2, layerIndex);
    if (tile && tile < 0x400)
        return tile;

    /* Fallback: known dungeon wall */
    return 0x175;
}

static void ClearRoomWithTile(int layerIndex, u16 tile) {
    int x, y;
    int w = gRoomControls.width >> 4;
    int h = gRoomControls.height >> 4;

    if (w <= 0 || h <= 0 || w > 64 || h > 64)
        return;

    for (y = 0; y < (h -1); y++) {
        for (x = 0; x < (w-1); x++) {
            SetTileType(tile, TILE_POS(x, y), layerIndex);
        }
    }
}
static u16 SampleCenterFloorTile(int layerIndex, int roomW, int roomH) {
    int cx = roomW >> 1;
    int cy = roomH >> 1;
    u16 tile;

    tile = GetTileTypeAtRoomTile(cx, cy, layerIndex);

    /* Reject invalid tiles */
    if (tile == 0 || tile >= 0x400) {
        /* Try nearby tiles */
        tile = GetTileTypeAtRoomTile(cx + 1, cy, layerIndex);
    }
    if (tile == 0 || tile >= 0x400) {
        tile = GetTileTypeAtRoomTile(cx, cy + 1, layerIndex);
    }
    if (tile == 0 || tile >= 0x400) {
        tile = GetTileTypeAtRoomTile(cx + 1, cy + 1, layerIndex);
    }
    if (tile == 0 || tile >= 0x400)
        tile = GetTileTypeAtRoomTile(cx - 1, cy - 1, layerIndex);


    /* Final hard fallback */
    if (tile == 0 || tile >= 0x400)
        tile = 0x174; /* known safe dungeon floor */

        return tile;
}

__attribute__((section(".ewram")))
MazeGenState gMazeState;
void GenerateAndApplyMaze(int layerIndex, u32 seed) {
    MazeGenState* state = &gMazeState;
    MapLayer* layer;
    int roomW, roomH;
    int startX, startY;
    int cx, cy, tx, ty, x,y,mazeW,mazeH;
    u16 floorTile, wallTile;

    layer = GetLayerByIndex(layerIndex);
    if (!layer)
        return;


    roomW = gRoomControls.width >> 4;
    roomH = gRoomControls.height >> 4;

    if (roomW > 64) roomW = 64;
    if (roomH > 64) roomH = 64;

    /* ---- 1. Sample center tiles ---- */
    floorTile = SampleCenterFloorTile(layerIndex, roomW, roomH);
    wallTile = SampleWallTile(layerIndex, roomW, roomH);


    /* ---- 2. Clear entire room ---- */
    ClearRoomWithTile(layerIndex, floorTile);

    /* ---- 3. Generate maze ---- */
    generate_cells(state, seed);

    startX = (roomW - ((MAZE_CELLS_X * 2) + 1)) >> 1;
    startY = (roomH - ((MAZE_CELLS_Y * 2) + 1)) >> 1;

    if (startX < 1) startX = 1;
    if (startY < 1) startY = 1;
     mazeW = (MAZE_CELLS_X * 2) + 1;
     mazeH = (MAZE_CELLS_Y * 2) + 1;

    for (x = 0; x < mazeW; x += 2) {
        DrawWallBlock(wallTile, (startX + x), (startY - 2), layerIndex);
        DrawWallBlock(wallTile, (startX + x), (startY + mazeH), layerIndex);
    }

    /* Left & right */
    for (y = 0; y < mazeH; y += 2) {
        DrawWallBlock(wallTile, (startX - 2), (startY + y), layerIndex);
        DrawWallBlock(wallTile, (startX + mazeW), (startY + y), layerIndex);
    }

    /* ---- 4. Draw maze ---- */
    for (cy = 0; cy < MAZE_CELLS_Y; cy++) {
        for (cx = 0; cx < MAZE_CELLS_X; cx++) {
            tx = startX + ((cx * 2) + 1);
            ty = startY + ((cy * 2) + 1);

            if (tx <= 0 || tx >= (roomW - 1) ||
                ty <= 0 || ty >= (roomH - 1))
                continue;

            /* Cell center (floor already exists, but safe to enforce) */
            SetTileType(floorTile, TILE_POS(tx, ty), layerIndex);

            /* Walls */
            if (state->maze[cy][cx].walls & 1) { // north
                DrawWallBlock(wallTile, tx, ty - 2, layerIndex);
            }
            if (state->maze[cy][cx].walls & 2) { // east
                DrawWallBlock(wallTile, tx + 1, ty, layerIndex);
            }
            if (state->maze[cy][cx].walls & 4) { // south
                DrawWallBlock(wallTile, tx, ty + 1, layerIndex);
            }
            if (state->maze[cy][cx].walls & 8) { // west
                DrawWallBlock(wallTile, tx - 2, ty, layerIndex);
            }

        }
    }

    gUpdateVisibleTiles = 1;
}

