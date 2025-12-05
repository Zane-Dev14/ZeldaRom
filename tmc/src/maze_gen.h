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

#endif // MAZE_GEN_H
