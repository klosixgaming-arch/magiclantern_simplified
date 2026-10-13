import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

for addr in range(0xE025FF00, 0xE025FF60, 4):
    off = addr - 0xE0000000
    val = struct.unpack("<I", data[off:off+4])[0]
    print(f"0x{addr:X}: 0x{val:08X}")
