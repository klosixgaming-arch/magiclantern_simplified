with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Search for where 0x14B004 or 0x14B000 is written to
import subprocess
res = subprocess.check_output([
    "arm-none-eabi-objdump", "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0xE0000000",
    "--start-address=0xE0233000",
    "--stop-address=0xE0233180",
    "/mnt/c/Users/Mega/Downloads/gang100.bin"
]).decode("latin1")

print(res)
