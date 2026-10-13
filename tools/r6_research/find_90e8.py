import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
needle = struct.pack("<I", 0x90E8)
pos = 0
while True:
    pos = rom.find(needle, pos)
    if pos == -1: break
    print(f"Ref to 0x90E8 at 0x{base + pos:08X}")
    pos += 4
