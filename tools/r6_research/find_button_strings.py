import re

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# Search for strings containing Button, Erace, Trash, KEY, etc.
keywords = [b"Erace", b"Trash", b"Button", b"button", b"KEY", b"PressKey", b"gui_main_task"]
for kw in keywords:
    print(f"=== Keyword: {kw.decode()} ===")
    matches = [m.start() for m in re.finditer(kw, rom)]
    for off in matches[:10]:
        # print surrounding string
        s_start = max(0, off - 20)
        s_end = min(len(rom), off + 40)
        print(f"  0x{base + off:08X}: {rom[s_start:s_end]}")
