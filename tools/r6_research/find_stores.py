import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for i in range(0, len(rom)-4, 2):
    w = struct.unpack_from("<H", rom, i)[0]
    if (w & 0xFFC0) == 0x66C0: # str rx, [ry, #0x6c]
        ry = (w >> 3) & 7
        rx = w & 7
        surr = rom[max(0, i-40):i+40]
        if b"\x00\x8d\x00\x00" in surr:
            print(f"Match str r{rx}, [r{ry}, #0x6c] at 0x{base+i:08X}")
