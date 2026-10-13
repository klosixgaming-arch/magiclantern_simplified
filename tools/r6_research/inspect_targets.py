import subprocess
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

def get_ptr(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data) - 4:
        return struct.unpack_from("<I", data, off)[0]
    return 0

targets = [0xe042e734, 0xe04782ce, 0xe0599d16, 0xe09ea4e6]

for t in targets:
    print("=== Target %s ===" % hex(t))
    # search backwards for function start (push {..., lr})
    off = t - 0xE0000000
    fn_start = off
    for i in range(off, max(0, off - 0x400), -2):
        hw = struct.unpack_from("<H", data, i)[0]
        if (hw & 0xFF00) == 0xB500: # push {..., lr}
            fn_start = i
            break
    print("Function start:", hex(0xE0000000 + fn_start))
    # look for any strings referenced within 0x200 bytes
    for i in range(fn_start, min(len(data)-4, fn_start + 0x300), 4):
        val = struct.unpack_from("<I", data, i)[0]
        if 0xE0000000 <= val < 0xE4000000:
            s = get_str(val)
            if len(s) > 3 and all(32 <= ord(c) < 127 for c in s):
                print("  Ref string at %s -> %s: %s" % (hex(0xE0000000 + i), hex(val), repr(s)))
