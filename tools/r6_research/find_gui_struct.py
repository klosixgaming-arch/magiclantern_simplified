import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
target = struct.pack("<I", 0x5460)

pos = 0
while True:
    idx = rom.find(target, pos)
    if idx == -1: break
    print("Found 0x5460 at 0x{:08X}".format(base + idx))
    pos = idx + 4
