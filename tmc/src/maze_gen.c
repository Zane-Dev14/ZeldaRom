#include "global.h"
#include "room.h"

#define MAZE_CELLS_X 10
#define MAZE_CELLS_Y 10
extern u8 gUpdateVisibleTiles;

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
    if (n == 0) return 0;
    r = local_rng_next(s);
    while (r >= n) r -= n;
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

static void push_cell(MazeGenState* s, int x, int y) {
    s->stack[s->stackTop].x = x;
    s->stack[s->stackTop].y = y;
    s->stackTop++;
}

static CellPos pop_cell(MazeGenState* s) {
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
}
void GenerateAndApplyMaze(int layerIndex, u32 seed) {
    MazeGenState state;
    MapLayer* layer;
    int roomW, roomH, centerTile, wallTile;
    int startX, startY, cx, cy, tx, ty;

    layer = GetLayerByIndex(layerIndex);
    if (!layer || !layer->mapData)
        return;

    roomW = (gRoomControls.width + 7) >> 3;
    roomH = (gRoomControls.height + 7) >> 3;
    if (roomW < 1) roomW = 1;
    if (roomW > 64) roomW = 64;
    if (roomH < 1) roomH = 1;
    if (roomH > 64) roomH = 64;

    centerTile = layer->mapData[(roomH >> 1) * 64 + (roomW >> 1)] & 0x0FFF;
    if (centerTile == 0)
        centerTile = 0x3001;

    wallTile = centerTile + 0x10;

    generate_cells(&state, seed);

    startX = (roomW > 21) ? ((roomW - 21) >> 1) : 0;
    startY = (roomH > 21) ? ((roomH - 21) >> 1) : 0;

    for (cy = 0; cy < MAZE_CELLS_Y; ++cy) {
        for (cx = 0; cx < MAZE_CELLS_X; ++cx) {
            tx = startX + (cx * 2 + 1);
            ty = startY + (cy * 2 + 1);

            if (tx >= 0 && tx < roomW && ty >= 0 && ty < roomH) {
                if ((state.maze[cy][cx].walls & 1) && ty > 0)
                    layer->mapData[(ty - 1) * 64 + tx] = wallTile;
                if ((state.maze[cy][cx].walls & 2) && tx < roomW - 1)
                    layer->mapData[ty * 64 + (tx + 1)] = wallTile;
                if ((state.maze[cy][cx].walls & 4) && ty < roomH - 1)
                    layer->mapData[(ty + 1) * 64 + tx] = wallTile;
                if ((state.maze[cy][cx].walls & 8) && tx > 0)
                    layer->mapData[ty * 64 + (tx - 1)] = wallTile;
            }
        }
    }

    /* REQUIRED in TMC: force tilemap refresh */
    gUpdateVisibleTiles = 1;
}

void ClearRoomMapDataOriginal(void) {
    int i;
    int w = gRoomControls.width >> 4;
    int h = gRoomControls.height >> 4;
    int count = w * h;

    for (i = 0; i < count; i++) {
        gMapBottom.mapDataOriginal[i] = 0;
        gMapTop.mapDataOriginal[i] = 0;
    }
}
