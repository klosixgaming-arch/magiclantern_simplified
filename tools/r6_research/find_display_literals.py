with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

def find_writes(target_val):
    target = struct.pack("<I", target_val)
    pos = 0
    while True:
        idx = data.find(target, pos)
        if idx == -1: break
        addr = 0xE0000000 + idx
        print(f"Literal 0x{target_val:X} found at 0x{addr:X}")
        pos = idx + 4

find_writes(0x14B000)
find_writes(0x14B1A8)
find_writes(0x18374)
find_writes(0x18384)
