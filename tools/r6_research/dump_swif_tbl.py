import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0109BE0 - base
end = 0xE0109DF0 - base

ptrs = []
for i in range(start, end, 4):
    val = struct.unpack_from("<I", rom, i)[0]
    ptrs.append((base + i, val))

for addr, val in ptrs:
    print(f"0x{addr:08X}: 0x{val:08X}")
