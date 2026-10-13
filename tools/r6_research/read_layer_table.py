import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0xE09A7468 - 0xE0000000)
    data = g.read(4)
table_addr = struct.unpack("<I", data)[0] - 24
print(f"Table addr: 0x{table_addr:X}")

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(table_addr - 0xE0000000)
    tdata = g.read(64)

for i in range(0, len(tdata), 4):
    v = struct.unpack("<I", tdata[i:i+4])[0]
    print(f"  +0x{i:02X}: 0x{v:08X} ({v})")
