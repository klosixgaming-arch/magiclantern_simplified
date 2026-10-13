import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
target = struct.pack("<I", 0x14B000)
pos = 0
while True:
    idx = rom.find(target, pos)
    if idx == -1:
        break
    print("Found 0x14B000 at ROM addr: 0x{:08X}".format(base + idx))
    pos = idx + 4

target2 = struct.pack("<I", 0x18384)
pos = 0
while True:
    idx = rom.find(target2, pos)
    if idx == -1:
        break
    print("Found 0x18384 at ROM addr: 0x{:08X}".format(base + idx))
    pos = idx + 4
