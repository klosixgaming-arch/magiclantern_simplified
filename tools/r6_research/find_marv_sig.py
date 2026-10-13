with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re
matches = [m.start() for m in re.finditer(b"VRAM", data)]
print(f"Found {len(matches)} matches for VRAM (MARV):")
for m in matches:
    addr = 0xE0000000 + m
    if 0xE04B5E00 <= addr <= 0xE04B6200:
        print(f"  MARV sig at 0x{addr:X}")
