with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
target = 0xE0233156

for i in range(0, len(rom) - 4, 2):
    b0, b1, b2, b3 = rom[i], rom[i+1], rom[i+2], rom[i+3]
    h1 = b0 | (b1 << 8)
    h2 = b2 | (b3 << 8)
    if (h1 & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
        s = (h1 >> 10) & 1
        j1 = (h2 >> 13) & 1
        j2 = (h2 >> 11) & 1
        imm10 = h1 & 0x3FF
        imm11 = h2 & 0x7FF
        i1 = (~(j1 ^ s)) & 1
        i2 = (~(j2 ^ s)) & 1
        imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if s:
            imm32 -= (1 << 25)
        pc = base + i + 4
        dest = pc + imm32
        if dest == target:
            print("Found BL to 0x{:08X} at 0x{:08X}".format(target, base + i))
