with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

# BL to 0xE0233156
# In Thumb-2:
# target = 0xE0233156
target = 0xE0233156

for off in range(0, len(data) - 4, 2):
    pc = 0xE0000000 + off + 4
    w1 = int.from_bytes(data[off:off+2], 'little')
    w2 = int.from_bytes(data[off+2:off+4], 'little')
    if (w1 & 0xF800) == 0xF000 and (w2 & 0xD800) == 0xD000:
        s = (w1 >> 10) & 1
        j1 = (w2 >> 13) & 1
        j2 = (w2 >> 11) & 1
        imm10 = w1 & 0x3FF
        imm11 = w2 & 0x7FF
        i1 = ~(j1 ^ s) & 1
        i2 = ~(j2 ^ s) & 1
        imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if imm32 & 0x1000000:
            imm32 -= 0x2000000
        dest = pc + imm32
        if dest == target:
            print(f"BL to RENDERER_InitializeScreen at 0x{0xE0000000 + off:X}")
