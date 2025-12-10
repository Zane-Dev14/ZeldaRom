#ifndef MAZE_GEN_H
#define MAZE_GEN_H

#include "global.h"

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

extern unsigned char _mazeState_start[];
extern unsigned char _mazeState_end[];

#define g_mazeState ((MazeGenState*)_mazeState_start)

void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed);

#endif
