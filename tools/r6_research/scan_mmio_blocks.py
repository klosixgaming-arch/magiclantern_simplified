import struct
from collections import Counter

with open('/mnt/c/Users/Mega/Downloads/gang100.bin', 'rb') as f:
    data = f.read()

ranges = [
    (0xC0000000, 0xC1000000, 'DIGIC Core/ASIC'),
    (0xD0000000, 0xD5000000, 'High-Speed/IO'),
    (0xF8000000, 0xF9000000, 'Cortex MPCore')
]

block_counts = Counter()
addr_counts = Counter()

for i in range(0, len(data) - 4, 4):
    val = struct.unpack('<I', data[i:i+4])[0]
    for low, high, name in ranges:
        if low <= val < high and (val & 3) == 0:
            blk = val & 0xFFFFF000
            block_counts[blk] += 1
            addr_counts[val] += 1

print('Top 50 Most Frequently Accessed MMIO Blocks in Canon 1.7.0 ROM:')
for blk, count in block_counts.most_common(50):
    sub = 'Core' if blk < 0xD0000000 else ('IO' if blk < 0xF0000000 else 'MPCore')
    samples = [f'0x{a:08X}' for a, c in addr_counts.items() if (a & 0xFFFFF000) == blk][:4]
    print(f'  [0x{blk:08X}] ({sub:6s}) refs={count:4d} | Samples: {", ".join(samples)}')
