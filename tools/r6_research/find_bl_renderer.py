with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

def find_bl(target):
    print(f"Finding BL to 0x{target:X}:")
    for off in range(0, len(data) - 4, 2):
        addr = 0xE0000000 + off
        w1 = int.from_bytes(data[off:off+2], 'little')
        w2 = int.from_bytes(data[off+2:off+4], 'little')
        if (w1 & 0xF800) == 0xF000 and (w2 & 0xD000) == 0xD000:
            s = (w1 >> 10) & 1
            j1 = (w2 >> 13) & 1
            j2 = (w2 >> 11) & 1
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm10 = w1 & 0x3FF
            imm11 = w2 & 0x7FF
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 -= (1 << 25)
            dest = (addr + 4 + imm32) & 0xFFFFFFFF
            if dest == target:
                print(f"  BL at 0x{addr:X}")

find_bl(0xE0233156)
