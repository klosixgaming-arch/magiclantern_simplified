import subprocess

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

def dis(addr, length):
    off = addr - 0xE0000000
    chunk = data[off:off+length]
    with open("/tmp/chunk.bin", "wb") as f:
        f.write(chunk)
    res = subprocess.run(["arm-none-eabi-objdump", "-b", "binary", "-m", "arm", "-M", "force-thumb", "-D", f"--adjust-vma={addr}", "/tmp/chunk.bin"], capture_output=True, text=True)
    print(res.stdout)

print("=== 0xE0233170 to 0xE0233300 ===")
dis(0xE0233170, 0x180)
