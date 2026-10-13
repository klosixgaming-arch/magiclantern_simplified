import struct
import sys

target = int(sys.argv[1], 16)
callers = []

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Search for BL instructions to target in Thumb mode
for i in range(0, len(data)-4, 2):
    hw1, hw2 = struct.unpack_from("<HH", data, i)
    if (hw1 & 0xF800) == 0xF000 and (hw2 & 0xD000) == 0xD000:
        s = (hw1 >> 10) & 1
        j1 = (hw2 >> 13) & 1
        j2 = (hw2 >> 11) & 1
        imm10 = hw1 & 0x3FF
        imm11 = hw2 & 0x7FF
        i1 = (~(j1 ^ s)) & 1
        i2 = (~(j2 ^ s)) & 1
        offset = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if s:
            offset = offset - (1 << 25)
        pc = 0xE0000000 + i + 4
        dest = pc + offset
        if dest == (target & ~1):
            callers.append(0xE0000000 + i)

print("Found %d direct BL callers to %s:" % (len(callers), hex(target)))
for c in callers[:40]:
    print("  %s" % hex(c))

# Also search for literal 32-bit pointers to target or target | 1
ptrs = []
target_bytes = struct.pack("<I", target | 1)
pos = 0
while True:
    pos = data.find(target_bytes, pos)
    if pos == -1:
        break
    ptrs.append(0xE0000000 + pos)
    pos += 4

target_bytes0 = struct.pack("<I", target & ~1)
pos = 0
while True:
    pos = data.find(target_bytes0, pos)
    if pos == -1:
        break
    ptrs.append(0xE0000000 + pos)
    pos += 4

print("Found %d pointer references to %s:" % (len(ptrs), hex(target)))
for p in ptrs[:40]:
    print("  %s" % hex(p))
