import re

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0C0B000 - base
end = 0xE0C0E000 - base

matches = re.finditer(b"[A-Za-z0-9_]{4,}", rom[start:end])
for m in matches:
    print(f"0x{base + start + m.start():08X}: {m.group().decode('latin1')}")
