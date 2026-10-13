import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

pos = 0
s = b"RefreshVrmsSurface"
while True:
    pos = data.find(s, pos)
    if pos == -1: break
    print("Found 'RefreshVrmsSurface' at", hex(0xE0000000 + pos))
    pos += len(s)
