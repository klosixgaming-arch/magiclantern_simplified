import capstone

with open('/mnt/c/Users/Mega/Downloads/gang100.bin', 'rb') as f:
    rom = f.read()

base = 0xE0000000
addr = 0xE07E15F8
offset = addr - base
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

for ins in md.disasm(rom[offset:offset+0x60], addr):
    print(f"0x{ins.address:08X}:  {ins.mnemonic:<8} {ins.op_str}")
