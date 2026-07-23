"""
write_build_ts.py — PlatformIO post-build script for dpx_tc002
Writes the firmware's compile timestamp to build_output/dpx_build.ts so
dpx_test.sh can compare it against the device's reported build string.
The test compares at minute precision to tolerate the few-second gap between
this post-build timestamp and __DATE__/__TIME__ in the compiled binary.
"""
Import("env")
import datetime
import os

def write_build_ts(source, target, env):
    ts = datetime.datetime.now().strftime("%b %d %Y %H:%M:%S")
    out_dir = os.path.join(env.subst("$PROJECT_DIR"), "build_output")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "dpx_build.ts"), "w") as f:
        f.write(ts)
    print(f"[dpx] Build timestamp: {ts}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", write_build_ts)
