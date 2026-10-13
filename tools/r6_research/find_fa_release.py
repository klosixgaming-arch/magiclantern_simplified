with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

# Search for "FA_Release\0"
target = b"FA_Release\x00"
pos = data.find(target)
if pos != -1:
    addr = 0xE0000000 + pos
    print(f"String 'FA_Release' at 0x{addr:X}")
    # Search for reference to string address
    str_packed = struct.pack("<I", addr)
    p2 = 0
    while True:
        idx = data.find(str_packed, p2)
        if idx == -1: break
        print(f"  Ref to 'FA_Release' at 0x{0xE0000000 + idx:X}")
        p2 = idx + 4
