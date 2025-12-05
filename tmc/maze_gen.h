#ifndef MAZE_GEN_H
#define MAZE_GEN_H

#include <stdint.h>

// Maze size in cells
#define MAZE_CELLS_X 10
#define MAZE_CELLS_Y 10

// Public API:
// layerIndex: pass to GetLayerByIndex (0 = typical bottom layer).
// seed: 0 to use game's Random() as the seed; otherwise use the provided seed.
void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed);

typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    u8 visited;
    u8 walls; /* N=1,E=2,S=4,W=8 */
} MazeCell;

typedef struct { int x, y; } CellPos;

extern uint32_t g_local_rng_state;
extern MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X];
extern CellPos stackArr[MAZE_CELLS_X * MAZE_CELLS_Y];
extern int stackTop;

#endif // MAZE_GEN_H
