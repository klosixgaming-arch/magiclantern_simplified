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

# Check handlers for 0x00 to 0x20 and 0xA0 to 0xC0
table_base = 0xe059aa58
for code in range(218):
    off = (table_base + code * 4) - 0xE0000000
    rel = struct.unpack_from("<i", data, off)[0]
    handler = table_base + rel
    # scan up to 30 bytes from handler for debug messages or strings
    h_off = handler - 0xE0000000
    chunk = data[h_off:h_off+32]
    # see if any string literal
    strs = []
    for i in range(0, len(chunk)-4, 2):
        hw = struct.unpack_from("<H", chunk, i)[0]
        # check ldr r2, [pc, #imm]
        if (hw & 0xF800) == 0x4800: # ldr rx, [pc, imm]
            imm = (hw & 0xFF) * 4
            pc = handler + i + 4
            ptr_addr = (pc & ~3) + imm
            p_off = ptr_addr - 0xE0000000
            if 0 <= p_off < len(data) - 4:
                val = struct.unpack_from("<I", data, p_off)[0]
                s = get_str(val)
                if s:
                    strs.append(s)
        # check add r2, pc, #imm
        elif (hw & 0xF800) == 0xA000: # adr rx, pc, imm
            imm = (hw & 0xFF) * 4
            pc = handler + i + 4
            s = get_str(pc + imm)
            if s:
                strs.append(s)
    if strs or handler != 0xE059C0E8:
        print("Code 0x%02X (%3d) at 0x%08X: %s" % (code, code, handler, " | ".join(strs[:2])))
