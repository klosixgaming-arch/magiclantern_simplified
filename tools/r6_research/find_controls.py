import re

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

patterns = [
    b"MultiControl",
    b"DirectionKey",
    b"SubDial",
    b"QuickControl",
    b"SwIF",
    b"SwSubDial",
    b"SwDirection",
    b"JoyStick",
    b"Joystick",
    b"MultiController",
    b"CrossKey",
    b"KeyCBR",
    b"DialCBR",
]

for p in patterns:
    matches = list(re.finditer(re.escape(p), rom, re.IGNORECASE))
    print(f"=== Pattern '{p.decode()}' found {len(matches)} times ===")
    for m in matches[:10]:
        offset = m.start()
        # find string boundaries
        s_start = rom.rfind(b"\x00", 0, offset) + 1
        s_end = rom.find(b"\x00", offset)
        s = rom[s_start:s_end]
        print(f"  0x{base + s_start:08X}: {s.decode('latin1', errors='replace')}")
