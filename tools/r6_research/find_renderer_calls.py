import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

for target_addr in [0xE0233157, 0xE0233156]:
    target = struct.pack("<I", target_addr)
    pos = 0
    while True:
        idx = rom.find(target, pos)
        if idx == -1: break
        print("Literal ref to 0x{:08X} at 0x{:08X}".format(target_addr, base + idx))
        pos = idx + 4

# Also search for BL instructions to 0xE0233156
# In Thumb: BL encoding
pc = 0
while True:
    idx = rom.find(b"\x56\x31\x23\xe0", pc) # or similar
    if idx == -1: break
    print("Found pattern at 0x{:08X}".format(base + idx))
    pc = idx + 4
