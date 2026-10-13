import struct

with open("/mnt/c/Users/Mega/Downloads/gang100.bin", "rb") as f:
    rom = f.read()

base = 0xE0000000

# The address of UiOpe_GetSwIF_FuncTbl:
# Where is the function UiOpe_GetSwIF_FuncTbl itself?
# Let us find where the string "UiOpe_GetSwIF_FuncTbl" or the table 0xE0109CE4 is used.
tbl_addr = 0xE0109CE4
needle = struct.pack("<I", tbl_addr)
pos = 0
while True:
    pos = rom.find(needle, pos)
    if pos == -1: break
    print(f"Ref to table at 0x{base + pos:08X}")
    pos += 4
