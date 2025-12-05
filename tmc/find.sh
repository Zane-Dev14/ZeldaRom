#!/usr/bin/env bash
set -e

echo "=== Zelda TMC — Targeted Scan ==="
echo

################################################################################
echo "=== 1) Candidate room/map loader functions (first 200 matches) ==="
################################################################################
git grep -nE "LoadRoom|LoadMap|Map_Load|room_load|roomLoad|map_load|LoadRoomTiles|LoadScreen" src \
    | sed -n '1,200p' || true
echo

################################################################################
echo "=== 2) Candidate tile-buffer / tilemap variable names (first 200 matches) ==="
################################################################################
git grep -nE "tilebuf|tile_buf|tileBuffer|tile_map|tilemap|roomTile|room_tile|roomTileBuf|mapBuffer|map_buf" src \
    | sed -n '1,200p' || true
echo

################################################################################
echo "=== 3) uint16/u8 tilemap arrays and likely buffer declarations (first 200 matches) ==="
################################################################################
git grep -nE "uint16_t .*(map|tile|screen)|u16 .*(map|tile)|uint8_t .*(tilemap|map|tile)|u8 .*(tilemap|map|tile)" src \
    | sed -n '1,200p' || true
echo

################################################################################
echo "=== 4) RNG / seed functions and globals (first 200 matches) ==="
################################################################################
git grep -nE "GetRandom|Rand|Random32|Random16|rng|rand|gRng|gRand|seed" src \
    | sed -n '1,200p' || true
echo

################################################################################
echo "=== 5) Collision / metatile / blocking matches (first 200 matches) ==="
################################################################################
git grep -nE "collision|collisions|blockmap|blocking|block_map|tileCollision|tile_collision|metatile|metatiles|meta_tile" src \
    | sed -n '1,200p' || true
echo

################################################################################
echo "=== 6) Room dimensions / tilemap size constants (first 200 matches) ==="
################################################################################
git grep -nE "ROOM_WIDTH|ROOM_HEIGHT|MAP_WIDTH|MAP_HEIGHT|TILEMAP_WIDTH|TILEMAP_HEIGHT|SCREEN_WIDTH|SCREEN_HEIGHT|MAP_W|MAP_H" \
    src include | sed -n '1,200p' || true
echo

################################################################################
echo "=== 7) assets/map.json (first 200 lines) ==="
################################################################################
sed -n '1,200p' assets/map.json || true
echo

################################################################################
echo "=== 8) File headers: room.c / roomInit.c / screenTileMap.c ==="
################################################################################
echo "--- src/room.c ---"
sed -n '1,240p' src/room.c || true
echo

echo "--- src/roomInit.c ---"
sed -n '1,240p' src/roomInit.c || true
echo

echo "--- src/screenTileMap.c ---"
sed -n '1,240p' src/screenTileMap.c || true
echo

echo "=== DONE ==="
