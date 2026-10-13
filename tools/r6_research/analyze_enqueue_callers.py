import subprocess
import struct

target = 0xE04E4666
callers = []

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

for i in range(0, len(data)-4, 2):
    hw1, hw2 = struct.unpack_from("<HH", data, i)
    if (hw1 & 0xF800) == 0xF000 and (hw2 & 0xD000) == 0xD000:
        s = (hw1 >> 10) & 1
        j1 = (hw2 >> 13) & 1
        j2 = (hw2 >> 11) & 1
        imm10 = hw1 & 0x3FF
        imm11 = hw2 & 0x7FF
        i1 = (~(j1 ^ s)) & 1
        i2 = (~(j2 ^ s)) & 1
        offset = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if s:
            offset = offset - (1 << 25)
        pc = 0xE0000000 + i + 4
        dest = pc + offset
        if dest == (target & ~1):
            callers.append(0xE0000000 + i)

print("Analyzing %d callers of gui_enqueue_message:" % len(callers))

# For each caller, look at preceding instructions (up to 30 bytes before)
for c in callers:
    # check instructions before c
    off = c - 0xE0000000
    chunk = data[max(0, off - 40):off+4]
    with open("/tmp/chunk.bin", "wb") as f:
        f.write(chunk)
    cmd = ["arm-none-eabi-objdump", "-b", "binary", "-marm", "-Mforce-thumb", f"--adjust-vma={hex(c - len(chunk) + 4)}", "-D", "/tmp/chunk.bin"]
    out = subprocess.check_output(cmd).decode()
    lines = [l.strip() for l in out.splitlines() if "\t" in l]
    # Check if any line sets r0
    print("--- Caller at %s ---" % hex(c))
    for l in lines[-8:]:
        print("  " + l)
