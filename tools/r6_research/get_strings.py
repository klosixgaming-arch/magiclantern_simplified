import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

def get_str(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data):
        end = data.find(b"\x00", off)
        if end != -1:
            return data[off:end].decode("ascii", errors="replace")
    return "<out of bounds>"

def get_ptr(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data) - 4:
        return struct.unpack_from("<I", data, off)[0]
    return 0

for addr in range(0xE0A7F360, 0xE0A7F400, 4):
    p = get_ptr(addr)
    s = get_str(p)
    print(hex(addr), "val:", hex(p), "str:", repr(s[:60]))
