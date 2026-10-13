import struct
import sys

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

def get_str(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data):
        end = data.find(b"\x00", off)
        if end != -1:
            return data[off:end].decode("ascii", errors="replace")
    return ""

start = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xE059B1A0
count = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x40

for addr in range(start, start + count, 4):
    off = addr - 0xE0000000
    val = struct.unpack_from("<I", data, off)[0]
    s = get_str(val) if 0xE0000000 <= val < 0xE4000000 else ""
    print(hex(addr), "->", hex(val), repr(s[:50]))
