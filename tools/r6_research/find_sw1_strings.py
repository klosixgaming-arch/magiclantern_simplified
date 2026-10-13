with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re

for kw in [b"SW1", b"SW2", b"Release_SW"]:
    matches = [m.start() for m in re.finditer(kw, data)]
    print(f"Matches for {kw.decode()}: {len(matches)}")
    for m in matches[:10]:
        s_start = max(0, m - 10)
        s_end = min(len(data), m + 30)
        chunk = data[s_start:s_end]
        parts = chunk.split(b"\x00")
        for p in parts:
            if kw in p:
                print("  Candidate:", p)
