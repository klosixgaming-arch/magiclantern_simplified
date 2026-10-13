import capstone

with open('/mnt/c/Users/Mega/Downloads/gang100.bin', 'rb') as f:
    rom = f.read()

base = 0xE0000000
target = 0xE07DCB1C
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

for offset in range(0, len(rom) - 4, 2):
    hw1 = rom[offset+1]
    if (hw1 & 0xF8) == 0xF0:
        addr = base + offset
        dis = list(md.disasm(rom[offset:offset+4], addr))
        if dis and dis[0].mnemonic == 'bl' and hex(target) in dis[0].op_str:
            print(f"Caller of _AllocateMemory at 0x{addr:08X}:")
            prev_off = offset - 20
            for p in md.disasm(rom[prev_off:offset+20], addr - 20):
                print(f"   0x{p.address:08X}: {p.mnemonic} {p.op_str}")
            print("-" * 40)
