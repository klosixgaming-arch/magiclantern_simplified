with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re
matches = [m.start() for m in re.finditer(b"RENDERER_InitializeScreen", data)]
print(f"Found {len(matches)} string matches for RENDERER_InitializeScreen:")
for m in matches:
    addr = 0xE0000000 + m
    print(f"  String at 0x{addr:X}")

# Let's find callers / references to these strings
import struct
for m in matches:
    str_addr = 0xE0000000 + m
    target = struct.pack("<I", str_addr)
    p = 0
    while True:
        idx = data.find(target, p)
        if idx == -1: break
        print(f"  Pointer ref to 0x{str_addr:X} at 0x{0xE0000000 + idx:X}")
        p = idx + 1
