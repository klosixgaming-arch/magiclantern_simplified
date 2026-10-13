import struct

rom_path = "/mnt/c/Users/Mega/Downloads/R6 Testing/2026 R6 Rom dumps/1.7.0/boot00.bin"
rom_base = 0xE0100000
addr = 0xE0CCC20C
offset = addr - rom_base

with open(rom_path, "rb") as f:
    f.seek(offset)
    data = f.read(28)

for i in range(7):
    val = struct.unpack("<I", data[i*4:i*4+4])[0]
    print(f"func[{i}] at {hex(addr + i*4)}: {hex(val)}")
