-- === Auto-detect Room ID + Coordinates ===
local snapshot = {}
local changes = {}

-- We'll check 1-byte and 2-byte values
local startAddr = 0x02000000
local endAddr   = 0x02040000

print("Step 1: Snapshot taken. Change room, then press D to detect changed addresses.")

-- Take a snapshot of all 1- and 2-byte values
for addr = startAddr, endAddr, 1 do
    snapshot[addr] = memory.readbyte(addr)
end

-- Detect changes after room transition
local function detect()
    print("Detecting changed addresses...")
    changes = {}
    for addr = startAddr, endAddr, 1 do
        local val = memory.readbyte(addr)
        if val ~= snapshot[addr] then
            table.insert(changes, {addr=addr, value=val})
        end
    end

    print("Changed addresses:")
    for _, c in ipairs(changes) do
        print(string.format("0x%08X = 0x%02X", c.addr, c.value))
    end
    print("Look for:")
    print("  1) A single 1-byte change = Room ID")
    print("  2) Two smooth 2-byte values changing together = X/Y coords")
end

emu.addKeyCallback(detect, "D")
