with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re

# Look for strings like "720", "960", "480", "540" in VRAM / Display context
for s in [b"DispVram", b"Vram", b"vram", b"RefreshVrms", b"VMIX"]:
    matches = [m.start() for m in re.finditer(s, data)]
    print(f"Matches for {s.decode()}: {len(matches)}")
