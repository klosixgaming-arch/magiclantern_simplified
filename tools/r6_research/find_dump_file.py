with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re
matches = [m.start() for m in re.finditer(b"A:/%s", data)]
print(f"Found {len(matches)} matches for A:/%s:")
for m in matches:
    print(f"  0x{0xE0000000 + m:X}")
