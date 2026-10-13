with open("/mnt/c/Users/Mega/.gemini/antigravity-cli/brain/759bad5c-10e5-4f69-bbb3-5fa08fa41971/.system_generated/tasks/task-4208.log") as f:
    text = f.read()

chunks = text.split("--- Caller at ")
for c in chunks[1:]:
    if "183" in c or "0xb7" in c.lower():
        print(c[:400])
