import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for tgt in [0x5474, 0x7F554, 0x5460]:
    needle = struct.pack("<I", tgt)
    pos = 0
    while True:
        pos = rom.find(needle, pos)
        if pos == -1: break
        print(f"Ref to 0x{tgt:X} at 0x{base + pos:08X}")
        pos += 4
