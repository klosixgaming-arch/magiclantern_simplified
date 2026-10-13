import struct

path = "/mnt/c/Users/Mega/Downloads/R6 Testing/2026 R6 Rom dumps/1.7.0/gang200.bin"
with open(path, "rb") as f:
    rom1 = f.read()

base = 0xE1000000
addr = 0xE10223E8
off = addr - base

print("Reading gang200.bin at offset 0x{:08X} (ROM addr: 0x{:08X}):".format(off, addr))
for i in range(8):
    entry = off + i * 12
    if entry + 12 <= len(rom1):
        vals = struct.unpack("<III", rom1[entry:entry+12])
        print("Entry {}: 0x{:08X} ({}), 0x{:08X} ({}), 0x{:08X} ({})".format(i, vals[0], vals[0], vals[1], vals[1], vals[2], vals[2]))
