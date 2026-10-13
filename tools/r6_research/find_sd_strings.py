with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

import re
matches = [m.start() for m in re.finditer(b"SD::|Sd::|sd::|SDCard|SdCard|sdcard|SDIO|sdio|DryEFatWrite|efat_write", data, re.IGNORECASE)]
print(f"Found {len(matches)} SD matches:")
for m in matches[:30]:
    s = data[m:m+64].split(b"\x00")[0]
    print(f"0x{0xE0000000 + m:X}: {s.decode('latin1', 'ignore')}")
