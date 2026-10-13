import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Search for references to 0x90E8 or 0x90EC
for target in [0x90E8, 0x90EC]:
    needle = struct.pack("<I", target)
    pos = 0
    while True:
        pos = rom.find(needle, pos)
        if pos == -1: break
        print(f"Ref to 0x{target:X} at 0x{base + pos:08X}")
        pos += 4
