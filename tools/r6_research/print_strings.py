with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

off = 0xE0107400 - 0xE0000000
for s in data[off:off+0x150].split(b"\x00"):
    if s:
        print(s.decode('latin1', 'ignore'))
