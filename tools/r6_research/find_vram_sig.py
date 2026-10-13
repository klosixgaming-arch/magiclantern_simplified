import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Search for "VRAM" signature (0x5652414D) stored into MARV struct
target = b"MARV" # or "VRAM"
pos = 0
while True:
    pos = data.find(b"VRAM", pos)
    if pos == -1: break
    print("Found 'VRAM' at", hex(0xE0000000 + pos))
    pos += 4

pos = 0
while True:
    pos = data.find(b"MARV", pos)
    if pos == -1: break
    print("Found 'MARV' at", hex(0xE0000000 + pos))
    pos += 4
