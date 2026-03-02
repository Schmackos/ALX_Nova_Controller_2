"""
Pre-build script: Adds #ifndef guards around WEBSOCKETS_MAX_DATA_SIZE in the
WebSockets library header so that -D build flags can override the default 15KB.
Without this, the library's unconditional #define silently overrides our flag.
"""
import os
import re

Import("env")  # noqa: F821 - PlatformIO SCons global

def patch_websockets_header(*args, **kwargs):
    ws_h = os.path.join(
        env.subst("$PROJECT_LIBDEPS_DIR"),
        env.subst("$PIOENV"),
        "WebSockets", "src", "WebSockets.h"
    )
    if not os.path.exists(ws_h):
        return

    with open(ws_h, "r") as f:
        txt = f.read()

    if "#ifndef WEBSOCKETS_MAX_DATA_SIZE" in txt:
        return  # already patched

    # Wrap each: #define WEBSOCKETS_MAX_DATA_SIZE <value>
    # With:      #ifndef / #define / #endif
    patched = re.sub(
        r"#define WEBSOCKETS_MAX_DATA_SIZE (.+)",
        r"#ifndef WEBSOCKETS_MAX_DATA_SIZE\n#define WEBSOCKETS_MAX_DATA_SIZE \1\n#endif",
        txt,
    )

    if patched != txt:
        with open(ws_h, "w") as f:
            f.write(patched)
        print("  [patch] WebSockets.h: added #ifndef guards for WEBSOCKETS_MAX_DATA_SIZE")

env.AddPreAction("buildprog", patch_websockets_header)
# Also run for lib building (the library compiles before buildprog)
env.AddPreAction("$BUILD_DIR/lib", patch_websockets_header)
