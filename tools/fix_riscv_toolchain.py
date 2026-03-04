"""
Pre-build script: Fixes the pioarduino RISC-V toolchain on Windows/MSYS2.

The pioarduino registry ZIP for toolchain-riscv32-esp only contains metadata
(package.json + tools.json), not the actual compiler binaries. This script
detects the broken state and downloads the real toolchain from Espressif.

Installation strategy (priority order):
  1. ~/.platformio/tools/toolchain-riscv32-esp/  ← flat layout, PREFERRED
     Extracted with `cp -r riscv32-esp-elf/* .` so the sysroot is directly
     under this directory.  Complete installation, not subject to Windows
     MAX_PATH truncation during Python zipfile extraction.

  2. <pkg_dir>/bin/                               ← flat layout in packages/
  3. <pkg_dir>/riscv32-esp-elf/bin/              ← nested layout in packages/
     Both packages/ layouts may have an incomplete sysroot on Windows if
     zipfile.extractall() silently dropped files due to MAX_PATH limits.

If no usable compiler is found anywhere, the real toolchain is downloaded
from Espressif and extracted via 7zip (if available) or Python zipfile.
The download target is always ~/.platformio/tools/ to avoid MAX_PATH issues
with deeply-nested paths inside ~/.platformio/packages/.

Only runs when building for RISC-V targets (ESP32-P4, ESP32-C3, etc).
"""
import os
import sys
import subprocess
import zipfile

Import("env")  # noqa: F821 - PlatformIO SCons global

TOOLCHAIN_PKG = "toolchain-riscv32-esp"
DOWNLOAD_URL = (
    "https://github.com/espressif/crosstool-NG/releases/download/"
    "esp-14.2.0_20251107/"
    "riscv32-esp-elf-14.2.0_20251107-x86_64-w64-mingw32.zip"
)

compiler_name = "riscv32-esp-elf-g++.exe"
pio_home = os.path.expanduser("~/.platformio")

# ---------------------------------------------------------------------------
# Locate candidate bin directories (checked in priority order)
# ---------------------------------------------------------------------------
candidates = []

# 1. tools/ flat layout — PREFERRED (complete sysroot, no MAX_PATH issues)
tools_dir = os.path.join(pio_home, "tools", TOOLCHAIN_PKG)
if os.path.isdir(tools_dir):
    candidates.append(os.path.join(tools_dir, "bin"))  # flat
    candidates.append(os.path.join(tools_dir, "riscv32-esp-elf", "bin"))  # nested

# 2. packages/ layout — may be truncated on Windows but try anyway
pkg_dir = env.PioPlatform().get_package_dir(TOOLCHAIN_PKG)
if pkg_dir and os.path.isdir(pkg_dir):
    candidates.append(os.path.join(pkg_dir, "bin"))
    candidates.append(os.path.join(pkg_dir, "riscv32-esp-elf", "bin"))

# Sysroot completeness check: a valid installation must have machine headers
# (these are missing when Python zipfile truncated the extraction on Windows).
def _sysroot_ok(bin_dir):
    """Return True if the sysroot under bin_dir/../ has the machine headers."""
    root = os.path.dirname(bin_dir)  # one level up from bin/
    # Flat layout: machine headers at root/riscv32-esp-elf/include/machine/
    # Nested layout: machine headers at root/riscv32-esp-elf/include/machine/
    # Both map to the same relative path from the toolchain root.
    for subpath in [
        os.path.join(root, "riscv32-esp-elf", "include", "machine"),
        os.path.join(root, "picolibc", "include", "machine"),
    ]:
        if os.path.isdir(subpath):
            return True
    return False

# ---------------------------------------------------------------------------
# Use the first candidate that has the compiler AND a complete sysroot
# ---------------------------------------------------------------------------
chosen_bin = None
for cand in candidates:
    compiler = os.path.join(cand, compiler_name)
    if os.path.isfile(compiler):
        if _sysroot_ok(cand):
            chosen_bin = cand
            break
        # Compiler exists but sysroot is incomplete — keep looking

if chosen_bin:
    env.PrependENVPath("PATH", chosen_bin)
    print("  [fix_riscv] Using toolchain from: %s" % chosen_bin)
else:
    # No usable installation found — download to tools/ (avoids MAX_PATH)
    print("  [fix_riscv] No complete toolchain found — downloading real toolchain...")
    install_dir = os.path.join(pio_home, "tools", TOOLCHAIN_PKG)
    os.makedirs(install_dir, exist_ok=True)
    zip_path = os.path.join(pio_home, "tools", "_riscv32_download.zip")

    try:
        subprocess.check_call(
            ["curl", "-L", "-o", zip_path, DOWNLOAD_URL],
            timeout=600,
        )

        # Try 7zip first (handles long paths correctly on Windows)
        _7z = None
        for path_7z in [r"C:\Program Files\7-Zip\7z.exe",
                        r"C:\Program Files (x86)\7-Zip\7z.exe"]:
            if os.path.isfile(path_7z):
                _7z = path_7z
                break

        if _7z:
            print("  [fix_riscv] Extracting with 7-Zip (long-path safe)...")
            # Extract to a temp dir then move up
            tmp_dir = install_dir + "_tmp"
            subprocess.check_call([_7z, "x", "-y", "-o" + tmp_dir, zip_path])
            # ZIP top-level is riscv32-esp-elf/; flatten one level
            top = os.path.join(tmp_dir, "riscv32-esp-elf")
            if os.path.isdir(top):
                import shutil
                if os.path.isdir(install_dir):
                    shutil.rmtree(install_dir)
                shutil.move(top, install_dir)
                shutil.rmtree(tmp_dir, ignore_errors=True)
            else:
                if os.path.isdir(install_dir):
                    import shutil
                    shutil.rmtree(install_dir)
                import shutil
                shutil.move(tmp_dir, install_dir)
        else:
            print("  [fix_riscv] Extracting with Python zipfile...")
            with zipfile.ZipFile(zip_path, "r") as zf:
                # Extract to tools/ directly; top-level riscv32-esp-elf/ becomes
                # tools/toolchain-riscv32-esp/riscv32-esp-elf/ (nested layout)
                zf.extractall(install_dir)

        # Detect layout and locate bin
        flat_bin   = os.path.join(install_dir, "bin")
        nested_bin = os.path.join(install_dir, "riscv32-esp-elf", "bin")

        if os.path.isfile(os.path.join(flat_bin, compiler_name)) and _sysroot_ok(flat_bin):
            env.PrependENVPath("PATH", flat_bin)
            print("  [fix_riscv] Toolchain installed (flat layout) at %s" % flat_bin)
        elif os.path.isfile(os.path.join(nested_bin, compiler_name)):
            env.PrependENVPath("PATH", nested_bin)
            print("  [fix_riscv] Toolchain installed (nested layout) at %s" % nested_bin)
        else:
            sys.exit("ERROR: Toolchain download succeeded but compiler still missing")

    finally:
        if os.path.exists(zip_path):
            os.remove(zip_path)
