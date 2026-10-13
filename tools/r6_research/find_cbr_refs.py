import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for str_addr in [0xE0108E8C, 0xE0108ED4, 0xE010941C]:
    needle = struct.pack("<I", str_addr)
    pos = 0
    while True:
        pos = rom.find(needle, pos)
        if pos == -1: break
        print(f"Ref to 0x{str_addr:08X} at 0x{base + pos:08X}")
        pos += 4
