import os, re

ml_src = "/mnt/c/Users/Mega/Downloads/magiclantern_simplified/src"
for root, dirs, files in os.walk(ml_src):
    for fn in files:
        if fn.endswith(".c"):
            p = os.path.join(root, fn)
            with open(p, "r", errors="ignore") as f:
                content = f.read()
            if "PROP_HANDLER" in content and "ASSERT" in content:
                print(f"File {fn}:")
                # find functions with PROP_HANDLER that contain ASSERT
                funcs = re.findall(r'PROP_HANDLER\s*\(\s*([^\)]+)\s*\)\s*\{([^}]+)\}', content, re.DOTALL)
                for prop, body in funcs:
                    if "ASSERT" in body:
                        print(f"   PROP_HANDLER({prop.strip()}):")
                        for line in body.splitlines():
                            if "ASSERT" in line:
                                print(f"      {line.strip()}")
