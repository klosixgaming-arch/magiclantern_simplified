import capstone

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
addr = 0xE1BC8380
size = 0x100
code = rom[addr - base : addr - base + size]

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
for i in md.disasm(code, addr):
    print(f"0x{i.address:08X}:  {i.mnemonic:8s} {i.op_str}")
