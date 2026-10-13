import subprocess

addr = 0xE04E4666
off = (addr & ~1) - 0xE0000000
with open("C:/Users/Mega/Downloads/gang100.bin", "rb") as g:
    g.seek(off)
    data = g.read(400)
with open("C:/Users/Mega/Downloads/magiclantern_simplified/f_enqueue.bin", "wb") as f:
    f.write(data)
cmd = f"wsl arm-none-eabi-objdump -b binary -m arm -M force-thumb -D --adjust-vma={hex(addr & ~1)} /mnt/c/Users/Mega/Downloads/magiclantern_simplified/f_enqueue.bin"
out = subprocess.check_output(cmd, shell=True).decode()
print(out)
