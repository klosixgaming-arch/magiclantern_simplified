with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re

for s in [b"VMIX_TransferRectangleToVram", b"TransferRectangleToVram", b"XCM_GetSourceSurface", b"XCM_MakeContext", b"JediWrapperClearSurface"]:
    m = [x.start() for x in re.finditer(s, data)]
    print(f"{s.decode()}: {len(m)} matches")
    for idx in m:
        print(f"  0x{0xE0000000 + idx:X}")
