with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0681A00 - base
end = 0xE0682200 - base

import re
matches = re.finditer(b"[A-Za-z0-9_]{3,}", rom[start:end])
for m in matches:
    print(f"0x{base + start + m.start():08X}: {m.group().decode('latin1')}")
