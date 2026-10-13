with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    data = g.read()

# Let's search around 0xE010743A in ROM
print("Disassembling around 0xE010743A:")
import subprocess

def disasm_range(start, length):
    off = start - 0xE0000000
    sub_data = data[off:off+length]
    with open("/tmp/sub.bin", "wb") as f:
        f.write(sub_data)
    res = subprocess.run([
        "arm-none-eabi-objdump", "-b", "binary", "-m", "arm", "-M", "force-thumb",
        "-D", f"--adjust-vma={start}", "/tmp/sub.bin"
    ], capture_output=True, text=True)
    for line in res.stdout.splitlines()[:50]:
        print(line)

disasm_range(0xE0107200, 300)
