-- Corrected mGBA Script to Teleport to Simon's Simulation with a specific inventory.
--
-- HOW TO USE:
-- 1. In mGBA, go to Tools > Scripting.
-- 2. Load and run this script while the game is running and PAUSED.
-- 3. The script will make the changes to RAM instantly.
-- 4. UNPAUSE the game and immediately walk through a door or screen transition.
-- 5. You will be warped to the Simon's Simulation room.
-- 6. Once there, SAVE your game to make the changes permanent in your .sav file.

-- [!! IMPORTANT !!] MEMORY ADDRESS VERIFICATION
-- The addresses below are based on common builds, but may be WRONG for your specific compiled ROM.
-- If this script fails, you MUST verify the addresses.
-- 1. Find the `tmc.sym` file in your `tmc/build/` directory.
-- 2. Open it and search for `gSave` and `gRoomTransition`.
-- 3. Note their starting addresses. For example, you might see `02022418 gSave`.
-- 4. The inventory is at a fixed offset from the start of `gSave`. If the base address changes,
--    you will need to update INV_START_ADDR and the item addresses below accordingly.
-- 5. Update the addresses in this script to match your `.sym` file.

local INV_START_ADDR = 0x02022442
local INV_LEN = 34
local FOURSWORD_ADDR = 0x0202244A
local BOW_ADDR = 0x0202244B

local AREA_ADDR = 0x03000DE8
local ROOM_ADDR = 0x03000DE9
local POS_X_ADDR = 0x03000E00
local POS_Y_ADDR = 0x03000E02

-- Target Values
local TARGET_AREA = 0x44
local TARGET_ROOM = 0x00
local TARGET_X = 0x0098
local TARGET_Y = 0x0088
local FOURSWORD_ID = 0x04
local BOW_ID = 0x05

-- --- SAFE MEMORY WRITING FUNCTIONS ---
-- These functions will catch errors and provide helpful feedback.

local function safe_write_u8(addr, val)
local ok, err = pcall(emu.memory.write_u8, addr, val)
if not ok then
    console.log(string.format("[FATAL ERROR] Failed to write to address %#x.", addr))
    console.log("           The address is invalid or protected. Check your tmc.sym file!")
    error("Halting script due to invalid memory address.")
    end
    end

    local function safe_write_u16_le(addr, val)
    local ok, err = pcall(emu.memory.write_u16_le, addr, val)
    if not ok then
        console.log(string.format("[FATAL ERROR] Failed to write to address %#x.", addr))
        console.log("           The address is invalid or protected. Check your tmc.sym file!")
        error("Halting script due to invalid memory address.")
        end
        end

        -- --- SCRIPT EXECUTION ---

        -- 1. Wipe the entire inventory region
        console.log("Wiping inventory...")
        for i = 0, INV_LEN - 1 do
            safe_write_u8(INV_START_ADDR + i, 0x00)
            end

            -- 2. Add the specific items
            console.log("Adding Four Sword and Bow...")
            safe_write_u8(FOURSWORD_ADDR, FOURSWORD_ID)
            safe_write_u8(BOW_ADDR, BOW_ID)

            -- 3. Set the next room transition data
            console.log("Setting room transition data...")
            safe_write_u8(AREA_ADDR, TARGET_AREA)
            safe_write_u8(ROOM_ADDR, TARGET_ROOM)
            safe_write_u16_le(POS_X_ADDR, TARGET_X)
            safe_write_u16_le(POS_Y_ADDR, TARGET_Y)

            console.log("--- MEMORY PATCH APPLIED SUCCESSFULLY ---")
            console.log(string.format("Warp set for Area: %#x, Room: %#x at (%#x, %#x)", TARGET_AREA, TARGET_ROOM, TARGET_X, TARGET_Y))
            console.log("IMMEDIATELY unpause and walk through a door to trigger the warp.")

