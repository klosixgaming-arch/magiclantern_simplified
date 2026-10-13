import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for addr in [0xE09EA62C, 0xE09EA71C, 0xE09EA744]:
    val = struct.unpack_from("<I", rom, addr - base)[0]
    print(f"0x{addr:08X}: 0x{val:08X}")
    if 0xE0000000 <= val < 0xE4000000:
        s_end = rom.find(b"\x00", val - base)
        if s_end != -1 and s_end - (val - base) < 80:
            print(f"  String: {rom[val - base : s_end].decode('latin1', errors='replace')}")
