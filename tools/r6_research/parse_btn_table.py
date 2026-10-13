with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0681A00 - base
end = 0xE0682140 - base

import struct

# The table has entries: string followed by function pointer
pos = start
while pos < end:
    # Read null-terminated string
    str_end = rom.find(b"\x00", pos)
    if str_end == -1 or str_end > end:
        break
    name = rom[pos:str_end].decode("latin1", errors="ignore")
    # align to 4
    ptr_pos = (str_end + 1 + 3) & ~3
    if ptr_pos + 4 <= end:
        ptr = struct.unpack("<I", rom[ptr_pos:ptr_pos+4])[0]
        if 0xE0000000 <= ptr < 0xE1000000:
            print(f"Button: {name:<28} Handler: 0x{ptr:08X} at table 0x{base + ptr_pos:08X}")
            pos = ptr_pos + 4
            continue
    pos = str_end + 1
