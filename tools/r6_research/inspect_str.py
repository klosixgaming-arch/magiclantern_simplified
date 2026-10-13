import sys

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    for arg in sys.argv[1:]:
        addr = int(arg, 16)
        off = addr - 0xE0000000
        f.seek(off)
        data = f.read(128)
        end = data.find(b"\x00")
        if end != -1:
            data = data[:end]
        print(hex(addr), ":", repr(data.decode("ascii", errors="replace")))
