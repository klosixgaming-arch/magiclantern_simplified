import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for off in range(0xE09A7460 - base, 0xE09A7480 - base, 4):
    val = struct.unpack("<I", rom[off:off+4])[0]
    print("0x{:08X}: 0x{:08X}".format(base + off, val))
