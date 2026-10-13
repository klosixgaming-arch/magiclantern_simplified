with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
start = 0xE0681F50 - base
end = 0xE0681FA0 - base

chunk = rom[start:end]
for i in range(0, len(chunk), 16):
    line_bytes = chunk[i:i+16]
    hex_str = " ".join(f"{b:02X}" for b in line_bytes)
    ascii_str = "".join(chr(b) if 32 <= b < 127 else "." for b in line_bytes)
    print(f"0x{base + start + i:08X}: {hex_str:<48}  {ascii_str}")
