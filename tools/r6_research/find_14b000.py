with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

def find_pool(val):
    target = struct.pack("<I", val)
    p = 0
    while True:
        idx = data.find(target, p)
        if idx == -1: break
        print(f"Ref to 0x{val:X} at 0x{0xE0000000 + idx:X}")
        p = idx + 1

find_pool(0x14B000)
