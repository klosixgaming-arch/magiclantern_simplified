import subprocess

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0x2A3E6C)
    data = g.read(400)

with open("/tmp/dump_file.bin", "wb") as f:
    f.write(data)

res = subprocess.run([
    "arm-none-eabi-objdump", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "-D", "--adjust-vma=0xE02A3E6C", "/tmp/dump_file.bin"
], capture_output=True, text=True)

print(res.stdout[:2000])
