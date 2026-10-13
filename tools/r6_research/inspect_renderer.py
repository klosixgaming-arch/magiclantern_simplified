import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    data = f.read()

# Disassemble around 0xE0233500 - 0xE0233700 to see all stores to 0x18374..0x18390
import subprocess
addr = 0xE0233500
off = addr - 0xE0000000
chunk = data[off:off+0x200]
with open("/tmp/code.bin", "wb") as f:
    f.write(chunk)
cmd = ["arm-none-eabi-objdump", "-b", "binary", "-marm", "-Mforce-thumb", f"--adjust-vma={hex(addr)}", "-D", "/tmp/code.bin"]
out = subprocess.check_output(cmd).decode()
print(out[:4000])
