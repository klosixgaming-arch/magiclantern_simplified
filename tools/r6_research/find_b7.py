import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Search for any movw r1, #183 or movs r1, #183 or ldr r1, =0xB7
# In Thumb:
# movs r1, #0xB7 -> 21B7
# movw r1, #0x00B7 -> F240 01B7
for i in range(0, len(data)-4, 2):
    hw1 = struct.unpack_from("<H", data, i)[0]
    if hw1 == 0x21B7:
        print("movs r1, #0xB7 at", hex(0xE0000000 + i))
    elif hw1 == 0xF240:
        hw2 = struct.unpack_from("<H", data, i+2)[0]
        if hw2 == 0x01B7:
            print("movw r1, #0xB7 at", hex(0xE0000000 + i))

# Also search for movs r0, #0xB7
for i in range(0, len(data)-4, 2):
    hw1 = struct.unpack_from("<H", data, i)[0]
    if hw1 == 0x20B7:
        print("movs r0, #0xB7 at", hex(0xE0000000 + i))
    elif hw1 == 0xF240:
        hw2 = struct.unpack_from("<H", data, i+2)[0]
        if hw2 == 0x00B7:
            print("movw r0, #0xB7 at", hex(0xE0000000 + i))
