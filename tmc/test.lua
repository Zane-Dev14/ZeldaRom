-- Player entity pointer
local PLAYER_ENTITY = 0x03001160 - 0x03000000

-- Offsets inside player entity (verified TMC)
local TILE_OFFSET = 0x2A  -- tile type under player (u16)

local tile = emu.memory.iwram:read16(PLAYER_ENTITY + TILE_OFFSET)

print(string.format("Tile under player = 0x%03X", tile))

local COLLISION_TILE_OFFSET = 0x2C
local tile = emu.memory.iwram:read16(PLAYER_ENTITY + COLLISION_TILE_OFFSET)

print(string.format("Collision tile = 0x%03X", tile))
