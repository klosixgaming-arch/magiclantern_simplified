import struct
from collections import Counter

with open('/mnt/c/Users/Mega/Downloads/gang100.bin', 'rb') as f:
    data = f.read()

ranges = [
    (0xF8000000, 0xF9000000, 'MPCore / Cortex-A53'),
    (0xD0000000, 0xD3000000, 'DIGIC X ASIC / IO'),
    (0xC0000000, 0xC1000000, 'DIGIC Core / Legacy')
]

for low, high, tag in ranges:
    c = Counter()
    for i in range(0, len(data) - 4, 4):
        v = struct.unpack('<I', data[i:i+4])[0]
        if low <= v < high and (v & 3) == 0:
            c[v & 0xFFFFF000] += 1
    print(f'=== Top 20 {tag} Blocks ===')
    for blk, cnt in c.most_common(20):
        print(f'    {{ 0x{blk:08X}, "{tag}" }}, /* {cnt} refs in ROM */')
