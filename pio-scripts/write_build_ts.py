"""
write_build_ts.py — PlatformIO pre-build script for dpx_tc002
Writes a build timestamp to build_output/dpx_build.ts before compilation
so dpx_test.sh can compare it against the device's __DATE__ __TIME__ string.
Runs at script import time (before source compilation) so the timestamp is
close to the __DATE__/__TIME__ macros the compiler will stamp into the binary.
"""
import datetime
import os

# Write timestamp immediately at import time — this runs before compilation
# so it will be within a few seconds of what __DATE__ __TIME__ captures.
_ts = datetime.datetime.now().strftime("%b %d %Y %H:%M:%S")
_out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build_output")
os.makedirs(_out_dir, exist_ok=True)
with open(os.path.join(_out_dir, "dpx_build.ts"), "w") as _f:
    _f.write(_ts)
print(f"[dpx] Build timestamp: {_ts}")

# PlatformIO requires this file to import 'env' — keep a no-op to satisfy it
Import("env")
