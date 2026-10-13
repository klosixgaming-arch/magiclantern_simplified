import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Look for pointers to SW_ERASE_BTN (0xE0681F78) or surrounding addresses
target = 0xE0681F78
packed = struct.pack("<I", target)
print(f"Searching for pointer to SW_ERASE_BTN (0x{target:08X})...")

matches = []
off = 0
while True:
    idx = rom.find(packed, off)
    if idx == -1:
        break
    matches.append(base + idx)
    off = idx + 4

for m in matches:
    print(f"  Pointer at: 0x{m:08X}")
