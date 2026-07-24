"""
write_build_ts.py — pre-build script for dpx_tc002
Generates a random 8-char hex build ID, injects it as DPX_BUILD_ID build flag,
and writes it to build_output/dpx_build.ts.
The test reads dpx_build.ts and compares to the device's reported build_id.
"""
Import("env")
import os, random, string

def gen_build_id():
    return ''.join(random.choices(string.hexdigits[:16], k=8)).lower()

build_id = gen_build_id()
out_dir = os.path.join(env.subst("$PROJECT_DIR"), "build_output")
os.makedirs(out_dir, exist_ok=True)
with open(os.path.join(out_dir, "dpx_build.ts"), "w") as f:
    f.write(build_id)

env.Append(CPPDEFINES=[("DPX_BUILD_ID", env.StringifyMacro(build_id))])
print(f"[dpx] Build ID: {build_id}")
