with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import struct

def find_str_refs(s):
    s_bytes = s.encode()
    str_off = data.find(s_bytes)
    if str_off == -1:
        print(f"String '{s}' not found")
        return
    str_addr = 0xE0000000 + str_off
    print(f"String '{s}' at 0x{str_addr:X}")
    target = struct.pack("<I", str_addr)
    p = 0
    while True:
        idx = data.find(target, p)
        if idx == -1: break
        print(f"  Ref to '{s}' at 0x{0xE0000000 + idx:X}")
        p = idx + 1

find_str_refs("JediWrapperClearSurface")
find_str_refs("RENDERER_InitializeScreen")
find_str_refs("SaveVRAM")
