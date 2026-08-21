# ================================================================================
# dpx_web_flash_bins.py — collect ESP Web Tools flashing artifacts
# ================================================================================
# Original work — dubpixel / dpx_tc002 (EUPL v1.2)
# ================================================================================
#
# Copies the extra binaries (bootloader, partition table, boot_app0) that
# output_bins.py doesn't collect, and writes a manifest.json for ESP Web Tools
# (https://github.com/esphome/esp-web-tools) — the same browser-based USB flash
# mechanism install.wled.me uses. Scoped to the TC001 (Ulanzi) release only, so
# this stays out of the way of every other WLED board env in the build matrix.
#
# ================================================================================

Import('env')
import os
import json
import shutil

OUTPUT_DIR = "build_output{}release{}".format(os.path.sep, os.path.sep)


def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]
    return define_list[-1] if define_list else None


def collect_web_flash_bins(source, target, env):
    release_name = _get_cpp_define_value(env, "WLED_RELEASE_NAME")
    if release_name != '\\"TC001\\"' and release_name != '"TC001"':
        return  # not our board — leave every other WLED env untouched

    with open("package.json", "r") as package:
        version = json.load(package)["version"]
    base = f"dpx_tc002_{version}_TC001"

    variant = env["PIOENV"]
    builddir = os.path.join(env["PROJECT_BUILD_DIR"], variant)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    parts = {
        "bootloader": os.path.join(builddir, "bootloader.bin"),
        "partitions": os.path.join(builddir, "partitions.bin"),
    }
    boot_app0 = os.path.join(
        env.PioPlatform().get_package_dir("framework-arduinoespressif32"),
        "tools", "partitions", "boot_app0.bin",
    )
    if os.path.isfile(boot_app0):
        parts["boot_app0"] = boot_app0

    manifest_parts = []
    fw_name = f"{base}.bin"
    fw_path = os.path.join(OUTPUT_DIR, fw_name)
    if os.path.isfile(fw_path):
        manifest_parts.append({"path": fw_name, "offset": 65536})  # 0x10000

    offsets = {"bootloader": 4096, "partitions": 32768, "boot_app0": 57344}
    for name, src in parts.items():
        if not os.path.isfile(src):
            print(f"dpx_web_flash_bins: missing {src}, skipping")
            continue
        dest_name = f"{base}_{name}.bin"
        dest_path = os.path.join(OUTPUT_DIR, dest_name)
        print(f"dpx_web_flash_bins: copying {src} to {dest_path}")
        shutil.copy(src, dest_path)
        manifest_parts.append({"path": dest_name, "offset": offsets[name]})

    # Order parts by flash offset — ESP Web Tools doesn't require it, but it
    # makes the manifest readable and matches how esptool.py lists them.
    manifest_parts.sort(key=lambda p: p["offset"])

    manifest = {
        # ESP Web Tools' install dialog reliably shows manifest.name but not
        # manifest.version (confirmed live) — bake the version into the name
        # so it's visible wherever the install button is embedded, including
        # on friendster's own page where we can't add separate version text.
        "name": f"dpx_tc002 v{version} (Ulanzi TC001)",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": manifest_parts,
            }
        ],
    }
    manifest_path = os.path.join(OUTPUT_DIR, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"dpx_web_flash_bins: wrote {manifest_path}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", collect_web_flash_bins)
