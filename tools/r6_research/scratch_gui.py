import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for target in [0x8D6C, 0x8D70, 0x8D74]:
    needle = struct.pack("<I", target)
    pos = 0
    while True:
        pos = rom.find(needle, pos)
        if pos == -1: break
        print(f"Ref to 0x{target:X} at 0x{base + pos:08X}")
        pos += 4
