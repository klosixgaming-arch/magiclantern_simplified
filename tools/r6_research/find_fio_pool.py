import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    rom = g.read()

# Let's search all 4-byte words in ROM matching the string addresses
strings = {
    "Open": 0xE07BC8A8,
    "Create": 0xE07BC8F4,
    "Remove": 0xE07BC910,
    "Close": 0xE07BD7C8,
}

for name, s_addr in strings.items():
    print(f"\n--- References to '{name}' (0x{s_addr:X}) ---")
    for i in range(0x7BC000, 0x7BE000, 4):
        val = struct.unpack('<I', rom[i:i+4])[0]
        if val == s_addr:
            ref = 0xE0000000 + i
            print(f"  LDR pool ref at 0x{ref:X}")
            # Search for LDR rx, [pc, #imm] pointing to ref
            for j in range(max(0x7BC000, i - 1000), i, 2):
                w = struct.unpack('<H', rom[j:j+2])[0]
                if (w & 0xF800) == 0x4800: # LDR rt, [pc, #imm8]
                    target = ((0xE0000000 + j + 4) & ~3) + ((w & 0xFF) << 2)
                    if target == ref:
                        code_addr = 0xE0000000 + j
                        print(f"    LDR instruction at 0x{code_addr:X}")
                        # Find entry point
                        for b in range(j, max(0x7BC000, j - 128), -2):
                            w_e = struct.unpack('<H', rom[b:b+2])[0]
                            if (w_e & 0xFF00) == 0xB500:
                                print(f"      Entry (push): 0x{0xE0000000 + b:X}")
                                break
                            elif w_e == 0xE92D:
                                print(f"      Entry (stmdb): 0x{0xE0000000 + b:X}")
                                break
