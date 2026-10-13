import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# String "UiOpe_GetSwIF_FuncTbl" is at 0xE0109DD4
# Find references to string 0xE0109DD4:
needle = struct.pack("<I", 0xE0109DD4)
pos = 0
while True:
    pos = rom.find(needle, pos)
    if pos == -1: break
    print(f"Ref to string at 0x{base + pos:08X}")
    pos += 4

# Or check pc-relative add to 0xE0109DD4
for addr in range(0xE0109000, 0xE010A500, 2):
    inst = struct.unpack_from("<H", rom, addr - base)[0]
    if (inst & 0xFF00) == 0xA200 or (inst & 0xFF00) == 0xA100 or (inst & 0xFF00) == 0xA000:
        imm = (inst & 0xFF) * 4
        pc = (addr & ~3) + 4
        if pc + imm == 0xE0109DD4:
            print(f"add rx, pc, #{imm} at 0x{addr:08X}")
