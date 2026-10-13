import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

for addr in range(0xE059C500, 0xE059C560, 4):
    off = addr - 0xE0000000
    val = struct.unpack_from("<I", data, off)[0]
    print(hex(addr), "->", hex(val))
