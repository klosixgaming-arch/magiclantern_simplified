import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Search for instructions that load literal 0x18374 or 0x18384
for addr in [0x18374, 0x18384, 0x14B000]:
    b = struct.pack("<I", addr)
    pos = 0
    print("=== Searching for literal %s ===" % hex(addr))
    while True:
        pos = data.find(b, pos)
        if pos == -1: break
        lit_addr = 0xE0000000 + pos
        print("Literal at", hex(lit_addr))
        # search backwards for LDR using this literal
        for i in range(max(0, pos - 0x200), pos, 2):
            hw = struct.unpack_from("<H", data, i)[0]
            if (hw & 0xF800) == 0x4800:
                imm = (hw & 0xFF) * 4
                pc = 0xE0000000 + i + 4
                target = (pc & ~3) + imm
                if target == lit_addr:
                    print("  Used by LDR at", hex(0xE0000000 + i))
        pos += 4
