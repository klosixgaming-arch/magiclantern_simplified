import struct
import subprocess

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0109CE4 - base
end = 0xE0109DD4 - base

funcs = []
idx = 0
for i in range(start, end, 4):
    fn_addr = struct.unpack_from("<I", rom, i)[0]
    funcs.append((idx, fn_addr))
    idx += 1

print(f"Total switch functions: {len(funcs)}")

# For each function, let us see if it references any strings in ROM
for idx, fn in funcs:
    fn_clean = fn & ~1
    code = rom[fn_clean - base : fn_clean - base + 64]
    # search for pc-relative ldr or literals
    found_str = []
    for offset in range(0, 64, 4):
        w = struct.unpack_from("<I", code, offset)[0]
        if 0xE0000000 <= w < 0xE4000000:
            # check if it points to a string
            s_off = w - base
            if 0 <= s_off < len(rom):
                s_end = rom.find(b"\x00", s_off)
                if s_end != -1 and s_end - s_off < 50:
                    cand = rom[s_off:s_end]
                    if all(32 <= b < 127 for b in cand) and len(cand) > 2:
                        found_str.append(cand.decode())
    str_summary = ", ".join(found_str) if found_str else "no string"
    print(f"[{idx:02d}] 0x{fn:08X}: {str_summary}")
