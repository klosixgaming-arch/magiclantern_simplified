with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

target = struct.pack("<I", 0xE023355C)
pos = 0
while True:
    idx = data.find(target, pos)
    if idx == -1: break
    addr = 0xE0000000 + idx
    print(f"Ref to RENDERER_InitializeScreen string at 0x{addr:X}")
    pos = idx + 4
