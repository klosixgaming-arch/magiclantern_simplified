with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    rom = g.read()

# Let's search strings in 0x7BC000 - 0x7BE000
import re

for m in re.finditer(b"[A-Za-z0-9_]{3,30}\x00", rom[0x7BC000:0x7BE000]):
    name = m.group(0)[:-1].decode('latin1')
    off = 0x7BC000 + m.start()
    addr = 0xE0000000 + off
    print(f"0x{addr:X} (off 0x{off:X}): {name}")
