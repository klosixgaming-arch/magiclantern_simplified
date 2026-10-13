import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

targets = {
    0xE0108E8C: "SwSubDialMultiControlerCBR[%d]",
    0xE0108EAC: "SwSubDialMultiControlerCBR Unknown[%d]",
    0xE0108ED4: "SwDirectionKeyCBR[%d]",
    0xE0108EEC: "SwDirectionKeyCBR Unknown[%d]",
}

for tgt_addr, name in targets.items():
    print(f"\nSearching for references to '{name}' at 0x{tgt_addr:08X}:")
    # check every 2-byte instruction in 0xE0108000 to 0xE0109000
    for addr in range(0xE0108500, 0xE0109000, 2):
        inst = struct.unpack_from("<H", rom, addr - base)[0]
        # ADD Rd, PC, #imm (A2xx for Rd=r2)
        if (inst & 0xFF00) == 0xA200:
            imm = (inst & 0xFF) * 4
            pc = (addr & ~3) + 4
            if pc + imm == tgt_addr:
                print(f"  Found 'add r2, pc, #{imm}' at 0x{addr:08X}")
        # or LDR Rd, [PC, #imm]
        elif (inst & 0xF800) == 0x4800:
            imm = (inst & 0xFF) * 4
            pc = (addr & ~3) + 4
            lit_val = struct.unpack_from("<I", rom, pc + imm - base)[0]
            if lit_val == tgt_addr:
                print(f"  Found 'ldr r?, [pc, #{imm}]' at 0x{addr:08X}")
