with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000
str_addr = 0xE02335F4
str_off = str_addr - base

# Disassemble backwards from str_addr to find where it is referenced
# Let's search instructions in range [str_off - 0x500, str_off]
import subprocess
res = subprocess.check_output([
    "arm-none-eabi-objdump", "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0xE0000000",
    f"--start-address={hex(str_addr - 0x300)}",
    f"--stop-address={hex(str_addr + 0x100)}",
    "/mnt/c/Users/Mega/Downloads/gang100.bin"
]).decode("latin1")

# Find lines containing 0xe02335f4
for line in res.splitlines():
    if "e02335f4" in line.lower() or "renderer.c" in line.lower() or "savevram" in line.lower():
        print(line)
