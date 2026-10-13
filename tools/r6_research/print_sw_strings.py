import re

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0108D00 - base
end = 0xE0109000 - base

matches = re.finditer(b"[A-Za-z0-9_/%().\\[\\] -]{3,}", rom[start:end])
for m in matches:
    addr = base + start + m.start()
    print(f"0x{addr:08X}: {m.group().decode('latin1')}")
