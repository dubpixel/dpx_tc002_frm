Import('env')
import os
import re
import shutil
import gzip
import json
import subprocess

OUTPUT_DIR = "build_output{}".format(os.path.sep)
#OUTPUT_DIR = os.path.join("build_output")

def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]

    if define_list:
        return define_list[-1]  # last definition wins (override takes precedence over base env)

    return None

def _create_dirs(dirs=["map", "release", "firmware"]):
    for d in dirs:
        os.makedirs(os.path.join(OUTPUT_DIR, d), exist_ok=True)

def _branch_slug():
    """Current branch name, slugified for filenames. Prefers CI env vars
    (GITHUB_HEAD_REF for PRs, GITHUB_REF_NAME for branch/tag pushes) over
    `git rev-parse --abbrev-ref HEAD`, since a CI checkout is often a
    detached HEAD where that git command would just return "HEAD"."""
    branch = os.environ.get("GITHUB_HEAD_REF") or os.environ.get("GITHUB_REF_NAME")
    if not branch:
        try:
            result = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                                     capture_output=True, text=True, check=True)
            branch = result.stdout.strip()
        except Exception:
            branch = None
    if not branch or branch == "HEAD":
        return None
    return re.sub(r'[^a-zA-Z0-9._-]', '-', branch)

def _short_sha():
    """7-char commit SHA. Prefers GITHUB_SHA (set in every GH Actions run)
    over a local git call for the same detached-HEAD reason as _branch_slug."""
    sha = os.environ.get("GITHUB_SHA")
    if sha:
        return sha[:7]
    try:
        result = subprocess.run(['git', 'rev-parse', '--short=7', 'HEAD'],
                                 capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except Exception:
        return None

def create_release(source):
    release_name_def = _get_cpp_define_value(env, "WLED_RELEASE_NAME")
    if release_name_def:
        release_name = release_name_def.replace("\\\"", "")
        with open("package.json", "r") as package:
            version = json.load(package)["version"]
        base_name = f"dpx_tc002_{version}_{release_name}"
        # Non-main builds get a branch-slug + short-SHA suffix so multiple
        # dev-branch builds at the same version stay distinguishable, and
        # so a branch build never collides with (or is mistaken for) a
        # main/release build of the same version. See AGENTS.md's "Build
        # Artifact Naming Convention".
        branch = _branch_slug()
        if branch and branch != "main":
            sha = _short_sha()
            suffix = f"-{branch}" + (f"-{sha}" if sha else "")
            base_name += suffix
        release_file = os.path.join(OUTPUT_DIR, "release", f"{base_name}.bin")
        release_gz_file = release_file + ".gz"
        print(f"Copying {source} to {release_file}")
        shutil.copy(source, release_file)
        bin_gzip(release_file, release_gz_file)
    else:
        variant = env["PIOENV"]
        bin_file = "{}firmware{}{}.bin".format(OUTPUT_DIR, os.path.sep, variant)
        print(f"Copying {source} to {bin_file}")
        shutil.copy(source, bin_file)

def bin_rename_copy(source, target, env):
    _create_dirs()
    variant = env["PIOENV"]
    builddir = os.path.join(env["PROJECT_BUILD_DIR"],  variant)
    source_map = os.path.join(builddir, env["PROGNAME"] + ".map")

    # create string with location and file names based on variant
    map_file = "{}map{}{}.map".format(OUTPUT_DIR, os.path.sep, variant)

    create_release(str(target[0]))

    # copy firmware.map to map/<variant>.map
    if os.path.isfile("firmware.map"):
        print("Found linker mapfile firmware.map")
        shutil.copy("firmware.map", map_file)
    if os.path.isfile(source_map):
        print(f"Found linker mapfile {source_map}")
        shutil.copy(source_map, map_file)

def bin_gzip(source, target):
    # only create gzip for esp8266
    if not env["PIOPLATFORM"] == "espressif8266":
        return
    
    print(f"Creating gzip file {target} from {source}")
    with open(source,"rb") as fp:
        with gzip.open(target, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", bin_rename_copy)
