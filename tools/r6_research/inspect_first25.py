import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

def get_str(addr):
    off = addr - 0xE0000000
    if 0 <= off < len(data):
        end = data.find(b"\x00", off)
        if end != -1:
            s = data[off:end].decode("ascii", errors="replace")
            if all(32 <= ord(c) < 127 for c in s) and len(s) > 2:
                return s
    return ""

table_base = 0xe059aa58
for code in range(0x26):
    off = (table_base + code * 4) - 0xE0000000
    rel = struct.unpack_from("<i", data, off)[0]
    handler = table_base + rel
    h_off = handler - 0xE0000000
    # look at 64 bytes in handler
    chunk = data[h_off:h_off+64]
    strs = []
    for i in range(0, len(chunk)-4, 2):
        hw = struct.unpack_from("<H", chunk, i)[0]
        if (hw & 0xF800) == 0x4800:
            imm = (hw & 0xFF) * 4
            pc = handler + i + 4
            p_off = ((pc & ~3) + imm) - 0xE0000000
            if 0 <= p_off < len(data) - 4:
                val = struct.unpack_from("<I", data, p_off)[0]
                s = get_str(val)
                if s: strs.append(s)
        elif (hw & 0xF800) == 0xA000:
            imm = (hw & 0xFF) * 4
            pc = handler + i + 4
            s = get_str(pc + imm)
            if s: strs.append(s)
    print("Code 0x%02X (%2d) at 0x%08X: %s" % (code, code, handler, " | ".join(strs)))
