with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

# Strings are at 0xE023355C and 0xE02335AC
# In Thumb-2:
# ADR Rd, label:
#   16-bit: ADD Rd, PC, #imm (imm <= 1020, PC = addr + 4)
#   32-bit: ADR.W Rd, label (offset up to 4KB)
# LDR Rd, =label (via literal pool within 4KB)
target1 = 0xE023355C
target2 = 0xE02335AC

for off in range(max(0, target1 - 0xE0000000 - 0x2000), min(len(data), target1 - 0xE0000000 + 0x2000), 2):
    addr = 0xE0000000 + off
    # check 16-bit ADD Rd, PC, #imm: 1010 0ddd iiiiiiii
    w = int.from_bytes(data[off:off+2], 'little')
    if (w & 0xF800) == 0xA000:
        imm = (w & 0xFF) * 4
        pc = (addr + 4) & ~3
        if pc + imm == target1 or pc + imm == target2:
            print(f"Found 16-bit ADR at 0x{addr:X}: target 0x{pc+imm:X}")
    # check 32-bit ADR.W: 1111 0i10 1010 1111 0iii dddd iiiiiiii (ADR) or SUB
    if off + 4 <= len(data):
        w32 = int.from_bytes(data[off:off+4], 'little')
        # ADR.W Rd, label: 1111 0x10 10x0 1111 ...
        # Or let's just check if literal pool has the address
        if w32 == target1 or w32 == target2:
            print(f"Found literal pool at 0x{addr:X}: target 0x{w32:X}")
