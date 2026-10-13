import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    rom = g.read()

# Let's find ADDW rd, pc, #imm or LDR rd, [pc, #imm] pointing to the string names
strings = {
    "Open": 0xE07BC8A8,
    "Create": 0xE07BC8F4,
    "Remove": 0xE07BC910,
    "Read": 0xE07BC918,
    "Write": 0xE07BCE34,
    "Close": 0xE07BD7C8,
    "AcqSize": 0xE07BD808,
}

for name, s_addr in strings.items():
    print(f"\n--- Function for '{name}' (str at 0x{s_addr:X}) ---")
    refs = []
    # Search for references in range 0x7BC000 - 0x7BE000
    for i in range(0x7BC000, 0x7BE000, 2):
        w1, w2 = struct.unpack('<HH', rom[i:i+4])
        # ADDW rd, pc, #imm
        if (w1 & 0xFBFF) == 0xF20F:
            imm12 = ((w1 & 0x0400) << 1) | ((w2 & 0x7000) >> 4) | (w2 & 0xFF)
            pc = 0xE0000000 + i + 4
            if pc + imm12 == s_addr:
                refs.append(0xE0000000 + i)
        # ADR rd, #imm
        elif (w1 & 0xFF00) == 0xA000:
            imm8 = (w1 & 0xFF) << 2
            pc = (0xE0000000 + i + 4) & ~3
            if pc + imm8 == s_addr:
                refs.append(0xE0000000 + i)
    for r in refs:
        print(f"  Ref at 0x{r:X}")
        # Search backwards for push / stmdb
        for b in range(r, r - 128, -2):
            w = struct.unpack('<H', rom[b-0xE0000000:b-0xE0000000+2])[0]
            # push {..., lr} or stmdb sp!, {..., lr}
            if (w & 0xFF00) == 0xB500: # push {..., lr}
                print(f"    Possible entry (push): 0x{b:X}")
                break
            elif w == 0xE92D: # stmdb sp!, {..., lr}
                print(f"    Possible entry (stmdb): 0x{b:X}")
                break
