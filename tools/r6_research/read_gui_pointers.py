import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    f.seek(0xE0477B3C - 0xE0000000)
    print("0xE0477B3C:", f.read(64))








