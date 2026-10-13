import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0xE025FF00 - 0xE0000000)
    data = g.read(128)

for i in range(0, 128, 4):
    v = struct.unpack("<I", data[i:i+4])[0]
    print(f"0x{0xE025FF00 + i:X} = 0x{v:08X}")
