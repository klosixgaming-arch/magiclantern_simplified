with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

off = 0xE0233540 - 0xE0000000
chunk = data[off:off+0x400]

s = ""
for b in chunk:
    if 32 <= b <= 126:
        s += chr(b)
    elif b == 0:
        if len(s) > 3:
            print("String:", s)
        s = ""
    else:
        s = ""
