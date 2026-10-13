import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# UiOpe_GetSwIF_FuncTbl table is at 0xE0109CE4
start = 0xE0109CE4 - base

print("=== ALL SWITCH / DIAL / BUTTON HANDLERS IN EOS R6 1.7.0 ===")
for idx in range(46):
    fn_addr = struct.unpack_from("<I", rom, start + idx * 4)[0]
    fn_clean = fn_addr & ~1
    code = rom[fn_clean - base : fn_clean - base + 120]
    
    # find string name from DryosDebugMsg call
    # DryosDebugMsg is called with add r2, pc, #imm
    name = "unknown"
    for o in range(0, 40, 2):
        inst = struct.unpack_from("<H", code, o)[0]
        if (inst & 0xFF00) == 0xA200:
            imm = (inst & 0xFF) * 4
            pc = ((fn_clean + o) & ~3) + 4
            s_off = pc + imm - base
            if 0 <= s_off < len(rom):
                s_end = rom.find(b"\x00", s_off)
                if s_end != -1 and s_end - s_off < 60:
                    cand = rom[s_off:s_end].decode('latin1', errors='replace')
                    name = cand
            break
            
    # find what event codes (movs r0, #imm / movs r4, #imm) it sends to 0xE09EA578
    codes = []
    for o in range(0, min(len(code)-2, 100), 2):
        inst = struct.unpack_from("<H", code, o)[0]
        if (inst & 0xFF00) == 0x2000: # movs r0, #imm
            codes.append(inst & 0xFF)
        elif (inst & 0xFF00) == 0x2400: # movs r4, #imm
            codes.append(inst & 0xFF)
            
    uniq_codes = sorted(list(set(codes)))
    codes_str = ", ".join(f"0x{c:02X}" for c in uniq_codes)
    print(f"[{idx:02d}] 0x{fn_addr:08X} | {name:<35} | Codes: {codes_str}")
