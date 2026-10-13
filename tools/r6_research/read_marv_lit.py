import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0xE04B61D8 - 0xE0000000)
    data = g.read(16)

v = struct.unpack("<I", data[:4])[0]
print(f"0xE04B61D8 = 0x{v:08X} ({data[:4]})")
