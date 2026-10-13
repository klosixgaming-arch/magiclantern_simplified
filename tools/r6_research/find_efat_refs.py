with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

def find_refs(addr):
    # Search for 32-bit little endian pointer
    target = struct.pack("<I", addr)
    refs = []
    p = 0
    while True:
        idx = data.find(target, p)
        if idx == -1: break
        refs.append(0xE0000000 + idx)
        p = idx + 1
    return refs

print("Refs to string 0xE010743A:")
for r in find_refs(0xE010743A):
    print(f"  0x{r:X}")

print("Refs to string 0xE01074BA:")
for r in find_refs(0xE01074BA):
    print(f"  0x{r:X}")
