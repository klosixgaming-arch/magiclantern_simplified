import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# read literal at 0xE07A22FC
val = struct.unpack_from("<I", rom, 0xE07A22FC - base)[0]
print(f"Literal at 0xE07A22FC: 0x{val:08X}")

# find all references to this literal
needle = struct.pack("<I", val)
pos = 0
while True:
    pos = rom.find(needle, pos)
    if pos == -1: break
    print(f"Ref to 0x{val:08X} at 0x{base + pos:08X}")
    pos += 4
