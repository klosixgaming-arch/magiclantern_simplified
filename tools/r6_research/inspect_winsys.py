import os

rom_path = "/mnt/c/Users/Mega/Downloads/R6 Testing/2026 R6 Rom dumps/1.7.0/boot00.bin"
rom_base = 0xE0100000

with open(rom_path, "rb") as f:
    rom = f.read()

for target in [0x18384, 0x183B4, 0x183B8, 0x14B000, 0x14B00C]:
    needle = target.to_bytes(4, "little")
    pos = 0
    found = []
    while True:
        pos = rom.find(needle, pos)
        if pos == -1:
            break
        found.append(rom_base + pos)
        pos += 4
    print(f"Target {hex(target)}: {len(found)} refs: {[hex(x) for x in found]}")
