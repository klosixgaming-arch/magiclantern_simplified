with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

target = 0xE029FC58
for off in range(max(0, target - 0xE0000000 - 0x1000), min(len(data), target - 0xE0000000 + 0x1000), 2):
    addr = 0xE0000000 + off
    w = int.from_bytes(data[off:off+2], 'little')
    if (w & 0xF800) == 0xA000:
        imm = (w & 0xFF) * 4
        pc = (addr + 4) & ~3
        if pc + imm == target:
            print(f"16-bit ADR at 0x{addr:X}")
    if off + 4 <= len(data):
        w32 = int.from_bytes(data[off:off+4], 'little')
        if w32 == target:
            print(f"Literal pool at 0x{addr:X}")
