// maze_gen.c — safe, engine-compatible maze injection for TMC
// Generates a 10x10 DFS backtracker maze and writes tiles+collisions into an in-RAM map layer.
// - Uses GetLayerByIndex, SetTile, MapLayer->mapData, MapLayer->collisionData.
// - Respects the logical room tile dimensions (derived from gRoomControls).
// - Robust autodetection of floor/wall tiles and corresponding collision bytes.
// - Safe to run in LoadRoom() after sub_0801AC98() as instructed.

#include "maze_gen.h"

#include "global.h"    // u8, u16, etc.
#include "map.h"       // MapLayer, GetLayerByIndex()
#include "functions.h" // SetTile(), Random()
#include "room.h"      // gRoomControls, camera_target
#include "asm.h"

#include <string.h>    // memset, memcpy
#include <stddef.h>    // NULL

// ---------------- PRNG (xorshift-style) ----------------
uint32_t g_local_rng_state;
static uint32_t local_rng_next(void) {
    uint32_t x = g_local_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_local_rng_state = x ? x : 0xDEADBEEFu;
    return g_local_rng_state;
}

/* Return a small-range value in [0, n-1] without using division.
 * n must be > 0 and reasonably small (we only use it for MAZE_CELLS_X / neighbor count).
 * Uses subtraction loop to avoid emitting __umodsi3.
 */
static uint32_t local_rng_range(uint32_t n) {
    uint32_t r;
    if (n == 0) return 0;
    r = local_rng_next();
    /* reduce r into range [0, n-1] using subtraction; n is small (<= 32), so this is cheap */
    while (r >= n) r -= n;
    return r;
}

// ---------------- Maze model ----------------
// ---------------- Maze model ----------------
/* MazeCell and CellPos typedefs are declared in maze_gen.h. */

/* Definitions with explicit initializers to avoid COMMON linkage.
 * Uninitialized common symbols sometimes end up in the linker 'COMMON' area
 * which your toolchain may GC aggressively. An explicit initializer forces
 * allocation in .bss/.data and prevents the 'defined in discarded section `COMMON`' error.
 */
uint32_t g_local_rng_state = 0u;

/* Zero-initialize the maze array (use an initializer so the object is not COMMON). */
MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X] = { { {0} } };

/* Zero-initialize stack array and top index similarly. */
CellPos stackArr[MAZE_CELLS_X * MAZE_CELLS_Y] = { {0} };
int stackTop = 0;

static const int DIR_DX[4] = {0, 1, 0, -1};
static const int DIR_DY[4] = {-1, 0, 1, 0};
static const u8 DIR_BIT[4] = {1, 2, 4, 8};
static const u8 OPP_BIT[4] = {4, 8, 1, 2};

static void init_cells(void) {
    int y;
    int x;
    for (y = 0; y < MAZE_CELLS_Y; ++y) {
        for (x = 0; x < MAZE_CELLS_X; ++x) {
            maze[y][x].visited = 0;
            maze[y][x].walls = 1|2|4|8;
        }
    }
    stackTop = 0;
}

static void push_cell(int x,int y){ stackArr[stackTop].x = x; stackArr[stackTop].y = y; ++stackTop; }
static CellPos pop_cell(void){ --stackTop; return stackArr[stackTop]; }

static int count_unvisited_neighbors(int x,int y,int *buf) {
    int n = 0;
    int d;
    int nx, ny;
    for (d = 0; d < 4; ++d) {
        nx = x + DIR_DX[d];
        ny = y + DIR_DY[d];
        if (nx >= 0 && nx < MAZE_CELLS_X && ny >= 0 && ny < MAZE_CELLS_Y) {
            if (!maze[ny][nx].visited) {
                buf[n++] = d;
            }
        }
    }
    return n;
}
static void carve_between(int x,int y,int d) {
    int nx = x + DIR_DX[d], ny = y + DIR_DY[d];
    maze[y][x].walls &= ~DIR_BIT[d];
    maze[ny][nx].walls &= ~OPP_BIT[d];
}

static void generate_cells(uint32_t seed) {
    int sx, sy;
    int dirs[4];
    int n;
    int ri;
    int d;
    int nx, ny;
    CellPos cur;

    g_local_rng_state = seed ? seed : 0xA5A5A5A5u;
    init_cells();

    sx = (int)local_rng_range(MAZE_CELLS_X);
    sy = (int)local_rng_range(MAZE_CELLS_Y);

    maze[sy][sx].visited = 1;
    push_cell(sx, sy);

    while (stackTop > 0) {
        cur = stackArr[stackTop - 1];
        n = count_unvisited_neighbors(cur.x, cur.y, dirs);

        if (n == 0) {
            pop_cell();
            continue;
        }

        ri = (int)local_rng_range((uint32_t)n);
        d = dirs[ri];
        carve_between(cur.x, cur.y, d);

        nx = cur.x + DIR_DX[d];
        ny = cur.y + DIR_DY[d];
        maze[ny][nx].visited = 1;

        push_cell(nx, ny);
    }
}

// ---------------- Helpers: room bounds & safe map writes ----------------

// Convert gRoomControls.width/height (pixels) to tile counts (8-px tiles).
// Use rounding up ( +7 ) then divide by 8 to match engine tile addressing.
static int room_tile_width(void) {
    int w = (gRoomControls.width + 7) >> 3; // divide by 8 (tiles are 8 px)
    if (w < 1) w = 1;
    if (w > 64) w = 64;
    return w;
}
static int room_tile_height(void) {
    int h = (gRoomControls.height + 7) >> 3; // divide by 8 (tiles are 8 px)
    if (h < 1) h = 1;
    if (h > 64) h = 64;
    return h;
}


// Safe write: calls engine SetTile (keeps engine runtime buffers happy) AND updates layer->mapData & collisionData
// posX/posY are tile coords within 0..63; we guard against writing outside logical room tile extents.
static void safe_write_tile_and_collision(
    MapLayer *layer, int layerIndex,
    int posX, int posY,
    u16 tileIndex, u8 collisionValue,
    int roomW, int roomH)
{
    int pos;
    u16 old;

    if (!layer) return;
    if (posX < 0 || posY < 0) return;
    if (posX >= 64 || posY >= 64) return;

    if (posX >= roomW || posY >= roomH) return;

    pos = posY * 64 + posX;

    SetTile(tileIndex, pos, layerIndex);

    if (layer->mapData) {
        old = layer->mapData[pos];
        layer->mapData[pos] = (old & 0xF000) | (tileIndex & 0x0FFF);
    }

    if (layer->collisionData) {
        layer->collisionData[pos] = collisionValue;
    }
}


// ---------------- Tile detection utilities ----------------
// We attempt to find representative floor (walkable) and wall (blocking) tiles and their collision bytes.
// Strategy:
//  1) Scan a safe top-left region (bounded by room tile dims).
//  2) Prefer tiles whose collisionData == 0 for floor, != 0 for wall.
//  3) Fallback to camera target tile or first non-zero tile.
//  4) If collisionData missing, use simple fallbacks.

/* Replacement: C89-compatible, no large static .bss, no memset.
 * Builds a small list of unique tiles seen in the scan window and counts them.
 * Returns the most common tile that matches the collision predicate,
 * or 0 if none found.
 */
static u16 find_most_common_tile_by_collision(MapLayer *layer, int roomW, int roomH, int wantCollision)
{
    int scanW, scanH;
    int y, x, i;
    int pos;
    u16 raw, tile;
    int coll;
    /* worst-case samples = 32*32 = 1024 */
    u16 tiles_seen[1024];
    u16 counts[1024];
    int seen;
    u16 best;
    u16 bestcnt;

    if (!layer || !layer->mapData) return 0;

    scanW = (roomW < 32) ? roomW : 32;
    scanH = (roomH < 32) ? roomH : 32;
    if (scanW < 1) scanW = 1;
    if (scanH < 1) scanH = 1;

    /* init */
    seen = 0;
    for (i = 0; i < 1024; ++i) {
        tiles_seen[i] = 0;
        counts[i] = 0;
    }

    for (y = 0; y < scanH; ++y) {
        for (x = 0; x < scanW; ++x) {
            pos = y * 64 + x;
            raw = layer->mapData[pos];
            tile = raw & 0x0FFF;
            coll = layer->collisionData ? layer->collisionData[pos] : 0;

            if ((wantCollision && coll != 0) || (!wantCollision && coll == 0)) {
                /* find tile in tiles_seen */
                int idx = -1;
                for (i = 0; i < seen; ++i) {
                    if (tiles_seen[i] == tile) { idx = i; break; }
                }
                if (idx >= 0) {
                    counts[idx] = counts[idx] + 1;
                } else {
                    if (seen < 1024) {
                        tiles_seen[seen] = tile;
                        counts[seen] = 1;
                        seen = seen + 1;
                    }
                }
            }
        }
    }

    if (seen == 0) return 0;

    best = tiles_seen[0];
    bestcnt = counts[0];
    for (i = 1; i < seen; ++i) {
        if (counts[i] > bestcnt) {
            bestcnt = counts[i];
            best = tiles_seen[i];
        }
    }

    if (bestcnt == 0) return 0;
    return best;
}


// If detection produced nothing, try camera target tile (if camera_target exists), then linear probe.
static u16 fallback_pick_tile(MapLayer *layer, int roomW, int roomH, int preferCollision) {
    int camX = 0, camY = 0;
    int pos;
    u16 t;
    int coll;
    int scanW, scanH;
    int y, x;

    if (!layer || !layer->mapData) return 0;

    if (gRoomControls.camera_target) {
        camX = ((gRoomControls.camera_target)->x.HALF.HI - gRoomControls.origin_x) >> 3;
        camY = ((gRoomControls.camera_target)->y.HALF.HI - gRoomControls.origin_y) >> 3;

        if (camX >= 0 && camX < roomW && camY >= 0 && camY < roomH) {
            pos = camY * 64 + camX;
            t = layer->mapData[pos] & 0x0FFF;
            coll = layer->collisionData ? layer->collisionData[pos] : 0;

            if ((preferCollision && coll != 0) || (!preferCollision && coll == 0))
                return t;
        }
    }

    scanW = (roomW < 32) ? roomW : 32;
    scanH = (roomH < 32) ? roomH : 32;

    for (y = 0; y < scanH; ++y) {
        for (x = 0; x < scanW; ++x) {
            pos = y * 64 + x;
            t = layer->mapData[pos] & 0x0FFF;
            coll = layer->collisionData ? layer->collisionData[pos] : 0;

            if ((preferCollision && coll != 0) || (!preferCollision && coll == 0))
                return t;
        }
    }

    for (y = 0; y < scanH; ++y) {
        for (x = 0; x < scanW; ++x) {
            t = layer->mapData[y * 64 + x] & 0x0FFF;
            if (t != 0) return t;
        }
    }

    return layer->mapData[0] & 0x0FFF;
}


// Given a tile index, find a representative collision byte in the scanned area, or fallback.
static u8 find_collision_for_tile(MapLayer *layer, u16 tileIndex, int roomW, int roomH, u8 fallback) {
    if (!layer || !layer->mapData || !layer->collisionData) return fallback;
    {
        int scanW = roomW < 32 ? roomW : 32;
        int scanH = roomH < 32 ? roomH : 32;
        int y, x;
        int pos;
        for (y = 0; y < scanH; ++y) {
            for (x = 0; x < scanW; ++x) {
                pos = y*64 + x;
                if ((layer->mapData[pos] & 0x0FFF) == tileIndex) return layer->collisionData[pos];
            }
        }
        /* linear probe further */
        for (y = 0; y < roomH; ++y) {
            for (x = 0; x < roomW; ++x) {
                pos = y*64 + x;
                if ((layer->mapData[pos] & 0x0FFF) == tileIndex) return layer->collisionData[pos];
            }
        }
    }
    return fallback;
}

// ---------------- Maze render mapping & write ----------------
// Mapping: each cell -> 2x2 floor tiles; walls occupy the gaps -> total area = 2*cells+1
static void render_maze_to_layer(MapLayer *layer, int layerIndex, u16 wallTile, u16 floorTile, u8 wallCollision, u8 floorCollision) {
    int roomW = room_tile_width();
    int roomH = room_tile_height();

    int tileAreaW = MAZE_CELLS_X * 2 + 1;
    int tileAreaH = MAZE_CELLS_Y * 2 + 1;

    // clamp to room size (avoid writing into unused map area)
    if (tileAreaW > roomW) tileAreaW = roomW;
    if (tileAreaH > roomH) tileAreaH = roomH;

    // write floor base
    {
        int y, x;
        for (y = 0; y < tileAreaH; ++y) {
            for (x = 0; x < tileAreaW; ++x) {
                safe_write_tile_and_collision(layer, layerIndex, x, y, floorTile, floorCollision, roomW, roomH);
            }
        }
    }

    /* walls between cells */
    {
        int cy, cx;
        int tx, ty;
        for (cy = 0; cy < MAZE_CELLS_Y; ++cy) {
            for (cx = 0; cx < MAZE_CELLS_X; ++cx) {
                tx = cx * 2 + 1;
                ty = cy * 2 + 1;
                /* center floor */
                safe_write_tile_and_collision(layer, layerIndex, tx, ty, floorTile, floorCollision, roomW, roomH);

                if (maze[cy][cx].walls & 1) safe_write_tile_and_collision(layer, layerIndex, tx, ty - 1, wallTile, wallCollision, roomW, roomH); /* N */
                    if (maze[cy][cx].walls & 2) safe_write_tile_and_collision(layer, layerIndex, tx + 1, ty, wallTile, wallCollision, roomW, roomH); /* E */
                        if (maze[cy][cx].walls & 4) safe_write_tile_and_collision(layer, layerIndex, tx, ty + 1, wallTile, wallCollision, roomW, roomH); /* S */
                            if (maze[cy][cx].walls & 8) safe_write_tile_and_collision(layer, layerIndex, tx - 1, ty, wallTile, wallCollision, roomW, roomH); /* W */
            }
        }
    }

    /* outer border (clamped by tileAreaW/tileAreaH) */
    {
        int x, y;
        for (x = 0; x < tileAreaW; ++x) {
            safe_write_tile_and_collision(layer, layerIndex, x, 0, wallTile, wallCollision, roomW, roomH);
            safe_write_tile_and_collision(layer, layerIndex, x, tileAreaH - 1, wallTile, wallCollision, roomW, roomH);
        }
        for (y = 0; y < tileAreaH; ++y) {
            safe_write_tile_and_collision(layer, layerIndex, 0, y, wallTile, wallCollision, roomW, roomH);
            safe_write_tile_and_collision(layer, layerIndex, tileAreaW - 1, y, wallTile, wallCollision, roomW, roomH);
        }
    }
}

// ---------------- Public API ----------------
void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed) {
    MapLayer *layer;
    int roomW, roomH;
    u16 detected_floor, detected_wall;
    u8 floorCollision, wallCollision;

    layer = GetLayerByIndex(layerIndex);
    if (!layer) return;
    if (!layer->mapData) return;

    roomW = room_tile_width();
    roomH = room_tile_height();

    detected_floor = 0;
    detected_wall  = 0;

    if (layer->collisionData) {
        detected_floor = find_most_common_tile_by_collision(layer, roomW, roomH, 0);
        detected_wall  = find_most_common_tile_by_collision(layer, roomW, roomH, 1);
    } else {
        detected_floor = fallback_pick_tile(layer, roomW, roomH, 0);
        detected_wall  = fallback_pick_tile(layer, roomW, roomH, 1);
    }

    if (detected_floor == 0) detected_floor = layer->mapData[0] & 0x0FFF;
    if (detected_wall == 0) {
        if (layer->mapData[1] && ((layer->mapData[1] & 0x0FFF) != detected_floor))
            detected_wall = layer->mapData[1] & 0x0FFF;
        else
            detected_wall = (detected_floor == 0) ? 1 : (detected_floor ^ 1);
    }
    if (detected_wall == detected_floor) detected_wall = (detected_floor ^ 1);

    floorCollision = 0;
    wallCollision  = 1;

    if (layer->collisionData) {
        floorCollision = find_collision_for_tile(layer, detected_floor, roomW, roomH, 0);
        wallCollision  = find_collision_for_tile(layer, detected_wall, roomW, roomH, 1);
        if (wallCollision == floorCollision) {
            wallCollision = (floorCollision == 1) ? 2 : 1;
        }
    }

    if (seed == 0) seed = Random();
    if (seed == 0) seed = 0xA5A5A5A5u;

    generate_cells(seed);

    render_maze_to_layer(layer, layerIndex,
                         detected_wall, detected_floor,
                         wallCollision, floorCollision);
}
