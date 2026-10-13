import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Search for LDR rx, =0x41xx in gang100.bin
# In thumb, literal pool loading 0x000041xx or 0x000040xx
for target in range(0x4100, 0x4160, 4):
    packed = struct.pack("<I", target)
    off = 0
    while True:
        idx = rom.find(packed, off)
        if idx == -1:
            break
        print(f"Literal 0x{target:04X} found at 0x{base + idx:08X}")
        off = idx + 4
