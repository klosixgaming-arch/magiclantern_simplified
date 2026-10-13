import subprocess

def disasm(addr, length):
    off = addr - 0xE0000000
    with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
        g.seek(off)
        data = g.read(length)
    with open(f"/tmp/func_{addr:x}.bin", "wb") as f:
        f.write(data)
    res = subprocess.run([
        "arm-none-eabi-objdump", "-b", "binary", "-m", "arm", "-M", "force-thumb",
        "-D", f"--adjust-vma={addr}", f"/tmp/func_{addr:x}.bin"
    ], capture_output=True, text=True)
    print(res.stdout)

print("=== 0xE0260000 to 0xE0260200 ===")
disasm(0xE0260000, 0x200)
