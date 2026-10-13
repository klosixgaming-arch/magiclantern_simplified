import subprocess
import sys

addr = int(sys.argv[1], 16)
size = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x200
offset = addr - 0xE0000000

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    f.seek(offset)
    data = f.read(size)

with open("/tmp/code.bin", "wb") as f:
    f.write(data)

cmd = ["arm-none-eabi-objdump", "-b", "binary", "-marm", "-Mforce-thumb", f"--adjust-vma={hex(addr)}", "-D", "/tmp/code.bin"]
out = subprocess.check_output(cmd).decode()
print(out)
