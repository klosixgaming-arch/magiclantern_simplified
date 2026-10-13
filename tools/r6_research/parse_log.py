with open("/mnt/c/Users/Mega/.gemini/antigravity-cli/brain/759bad5c-10e5-4f69-bbb3-5fa08fa41971/.system_generated/tasks/task-4208.log") as f:
    text = f.read()

chunks = text.split("--- Caller at ")
print("Total callers in log:", len(chunks)-1)
for c in chunks[1:]:
    lines = [l.strip() for l in c.splitlines() if l.strip()]
    if not lines:
        continue
    header = lines[0]
    # find instruction setting r0 before bl
    bl_idx = -1
    for i, l in enumerate(lines):
        if "bl\t0xe04e4666" in l or "b.w\t0xe04e4666" in l:
            bl_idx = i
            break
    if bl_idx != -1:
        prev = lines[max(1, bl_idx-5):bl_idx]
        for p in prev:
            if "movs\tr0," in p:
                print(header, ":", p)
