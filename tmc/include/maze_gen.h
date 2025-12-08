#ifndef MAZE_GEN_H
#define MAZE_GEN_H

#include <stdint.h> /* uint8_t, uint16_t, uint32_t */

/* Maze size in cells */
#define MAZE_CELLS_X 5
#define MAZE_CELLS_Y 5

typedef struct {
    uint8_t visited;
    uint8_t walls; /* N=1,E=2,S=4,W=8 */
} MazeCell;

typedef struct { int x, y; } CellPos;

/*
 * extern uint32_t g_local_rng_state;
 * extern MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X];
 * extern CellPos stackArr[MAZE_CELLS_X * MAZE_CELLS_Y];
 * extern int stackTop;*/

/* Public API:
 * layerIndex: pass to GetLayerByIndex (0 = typical bottom layer).
 * seed: 0 to use game's Random() as the seed; otherwise use the provided seed.
 */
void GenerateAndApplyMazeToLayer(int layerIndex, uint32_t seed);

/* Ensure the TU with maze data is referenced so the linker doesn't GC it.
 *   Implemented in maze_gen.c (empty function is fine). */
void MazeGen_KeepData(void);

#endif /* MAZE_GEN_H */
