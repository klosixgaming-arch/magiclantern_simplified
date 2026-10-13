import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
target = 0xE04E4666

callers = []
for addr in range(0xE0100000, 0xE2000000, 2):
    h1 = struct.unpack_from("<H", rom, addr - base)[0]
    if (h1 & 0xF800) == 0xF000:
        h2 = struct.unpack_from("<H", rom, addr + 2 - base)[0]
        if (h2 & 0xD000) in (0x9000, 0xD000):
            s = (h1 >> 10) & 1
            j1 = (h2 >> 13) & 1
            j2 = (h2 >> 11) & 1
            imm10 = h1 & 0x3FF
            imm11 = h2 & 0x7FF
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm25 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if imm25 & (1 << 24):
                imm25 -= (1 << 25)
            dest = addr + 4 + imm25
            if dest == target:
                callers.append(addr)

print("Callers with r0 == 0 (ev_type 0):")
for c in callers:
    for back in [2, 4, 6, 8, 10]:
        inst = struct.unpack_from("<H", rom, c - back - base)[0]
        if (inst & 0xFF00) == 0x2000: # movs r0, #imm8
            val = inst & 0xFF
            if val == 0:
                print(f"  0x{c:08X}")
            break
