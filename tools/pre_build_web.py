"""
PlatformIO pre-build script: auto-rebuild web_pages_gz.cpp when sources change.

Runs before each ESP32 firmware build. Checks if web_pages.cpp or login_page.h
is newer than web_pages_gz.cpp, and if so, re-runs build_web_assets.js.

Added to platformio.ini via:
    extra_scripts = pre:tools/pre_build_web.py
"""

Import("env")

import os
import subprocess
import sys

project_dir = env.subst("$PROJECT_DIR")

src_file   = os.path.join(project_dir, "src", "web_pages.cpp")
login_file = os.path.join(project_dir, "src", "login_page.h")
out_file   = os.path.join(project_dir, "src", "web_pages_gz.cpp")
script     = os.path.join(project_dir, "tools", "build_web_assets.js")

TAG = "pre_build_web"

if not os.path.exists(out_file):
    needs_rebuild = True
    reason = "web_pages_gz.cpp is missing"
else:
    out_mtime = os.path.getmtime(out_file)
    src_mtime = os.path.getmtime(src_file)
    needs_rebuild = src_mtime > out_mtime
    if not needs_rebuild and os.path.exists(login_file):
        needs_rebuild = os.path.getmtime(login_file) > out_mtime
    reason = "source newer than web_pages_gz.cpp" if needs_rebuild else None

if needs_rebuild:
    print(f"\n[{TAG}] {reason} — running build_web_assets.js...")
    result = subprocess.run(
        ["node", script],
        cwd=project_dir,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"[{TAG}] ERROR: build_web_assets.js failed with exit code {result.returncode}")
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
        sys.exit(1)
    # Print the script's output summary (last few lines are most useful)
    lines = [l for l in result.stdout.strip().splitlines() if l.strip()]
    for line in lines:
        print(f"[{TAG}] {line}")
    print(f"[{TAG}] web_pages_gz.cpp rebuilt successfully.\n")
else:
    print(f"[{TAG}] web_pages_gz.cpp is up to date.")
