with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# find where "UiOpe_GetSwIF_FuncTbl" occurs
pos = rom.find(b"UiOpe_GetSwIF_FuncTbl")
print("String at:", hex(base + pos))

# let us search for references to this string address
import struct
needle = struct.pack("<I", base + pos)
p = 0
while True:
    p = rom.find(needle, p)
    if p == -1: break
    print("Ref to string at:", hex(base + p))
    p += 4
