with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re
matches = [m.start() for m in re.finditer(b"DryEFat", data)]
print(f"Found {len(matches)} DryEFat references:")
for m in matches[:20]:
    s = data[m:m+64].split(b"\x00")[0]
    print(f"0x{0xE0000000 + m:X}: {s.decode('latin1', 'ignore')}")
