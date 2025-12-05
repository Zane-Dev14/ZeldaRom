/* src/maze_gen_data.c
 * Separate translation unit that holds the maze generator's global data.
 * This prevents the linker from discarding those objects when GC is active.
 */

#include "maze_gen.h"
#include <stdint.h>

/* MazeCell and CellPos are declared in maze_gen.h (use uint8_t there). */

/* Definitions (zero-initialized). Put them in a separate TU so the linker keeps them. */
uint32_t g_local_rng_state = 0u;

/* Zero-initialize maze and stack so they are emitted into .bss/.data of this TU. */
MazeCell maze[MAZE_CELLS_Y][MAZE_CELLS_X] = { { {0} } };
CellPos stackArr[MAZE_CELLS_X * MAZE_CELLS_Y] = { {0} };
int stackTop = 0;