import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Base of jump table calculation:
# at 0xe059aa4e: add ip, pc -> ip = pc + r4*4 = (0xe059aa4e + 4) + r4*4 = 0xe059aa52 + r4*4
# ldr.w ip, [ip, #6] -> reads from 0xe059aa58 + r4*4!
# add pc, ip -> pc = (0xe059aa54 + 4) + ip = 0xe059aa58 + ip!
table_base = 0xe059aa58
entries = []

for i in range(218):
    off = (table_base + i * 4) - 0xE0000000
    rel_offset = struct.unpack_from("<i", data, off)[0]
    dest = table_base + rel_offset
    entries.append((i, dest))

print("Button code jump targets (first 40):")
for code, dest in entries[:40]:
    print("  Code 0x%02X (%3d) -> Target: 0x%08X" % (code, code, dest))

# Group identical targets
from collections import defaultdict
grouped = defaultdict(list)
for code, dest in entries:
    grouped[dest].append(code)

print("\nGrouped by handler destination:")
for dest, codes in sorted(grouped.items(), key=lambda x: len(x[1])):
    code_str = ", ".join("0x%02X" % c for c in codes)
    print("  Target 0x%08X: %d codes [%s]" % (dest, len(codes), code_str))
