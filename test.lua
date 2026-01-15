-- Test memory reading in mGBA Lua Script
local area = memory.readbyte(0x02002F7C)
local room = memory.readbyte(0x02002F7D)
print(string.format("Area=%02X Room=%02X", area, room))
