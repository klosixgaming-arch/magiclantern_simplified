with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0xE02A01C0 - 0xE0000000)
    data = g.read(64)
print(data.split(b"\x00")[0].decode('latin1'))
