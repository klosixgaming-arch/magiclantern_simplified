import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

def get_str(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data):
        end = data.find(b"\x00", off)
        if end != -1:
            return data[off:end].decode("ascii", errors="replace")
    return ""

for addr in range(0xE059CB80, 0xE059CC20, 4):
    off = addr - 0xE0000000
    val = struct.unpack_from("<I", data, off)[0]
    s = get_str(val) if 0xE0000000 <= val < 0xE4000000 else ""
    print(hex(addr), "->", hex(val), repr(s[:50]))
