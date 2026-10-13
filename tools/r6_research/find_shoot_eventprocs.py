with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re

# Look for eventprocs related to shooting / release / shutter
for kw in [b"Release", b"Shutter", b"Shoot", b"FA_"]:
    matches = [m.start() for m in re.finditer(kw, data)]
    print(f"Matches for {kw.decode()}: {len(matches)}")
    for m in matches[:10]:
        # print surrounding string
        s_start = max(0, m - 10)
        s_end = min(len(data), m + 30)
        chunk = data[s_start:s_end]
        # find nulls
        parts = chunk.split(b"\x00")
        for p in parts:
            if kw in p:
                print("  Candidate:", p)
