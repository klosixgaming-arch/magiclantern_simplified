import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Find "MARV%s" or "Width:%d"
idx = rom.find(b"MARV%s")
if idx != -1:
    print(f"MARV%s string at: 0x{base + idx:08X}")
    print(repr(rom[idx:idx+100]))
    
    # Now find where this string pointer is referenced in ROM
    str_addr = base + idx
    target = struct.pack("<I", str_addr)
    pos = 0
    while True:
        ref = rom.find(target, pos)
        if ref == -1: break
        print(f"Referenced at: 0x{base + ref:08X}")
        pos = ref + 4
