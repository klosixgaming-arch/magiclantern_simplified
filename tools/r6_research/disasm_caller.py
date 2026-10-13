import subprocess

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(0x10B88E - 40)
    data = g.read(100)

with open("/tmp/caller1.bin", "wb") as f:
    f.write(data)

res = subprocess.run([
    "arm-none-eabi-objdump", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "-D", f"--adjust-vma={0xE010B88E - 40}", "/tmp/caller1.bin"
], capture_output=True, text=True)

print(res.stdout)
