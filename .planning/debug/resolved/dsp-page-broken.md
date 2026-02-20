---
status: resolved
trigger: "DSP settings page is completely non-functional — can't add presets, frequency response graph doesn't render, can't add pipeline stages. WebSocket may not be connecting. No browser console errors visible."
created: 2026-02-20T00:00:00Z
updated: 2026-02-20T12:00:00Z
---

## Current Focus

hypothesis: CONFIRMED - WS_BUF_SIZE (4096 bytes) too small for DSP state JSON (~5202 bytes minimum)
test: Calculated JSON size for default DSP state and confirmed overflow
expecting: Fix increases WS_BUF_SIZE to 16384 + adds truncation guard with String fallback
next_action: COMPLETE

## Symptoms

expected: DSP page loads with working preset management, frequency response graph, and ability to add/remove pipeline stages
actual: All DSP page features broken — add preset does nothing, freq response graph blank, stage management non-functional
errors: No browser console errors visible (user may have missed the JSON SyntaxError)
reproduction: Navigate to DSP page in web UI
started: Since commit 87bce1e (heap memory optimization — introduced WS_BUF_SIZE = 4096)

## Eliminated

- hypothesis: JavaScript syntax error in web_pages.cpp
  evidence: Node.js --check passes cleanly on extracted script; braces/parens balanced
  timestamp: 2026-02-20

- hypothesis: Missing fields removed from app_state.h causing compile failure
  evidence: Build succeeds; all USB routing field removals are internally consistent
  timestamp: 2026-02-20

- hypothesis: DSP API changes breaking server-side state
  evidence: 159 DSP tests pass; sendDspState() function is identical in structure to before
  timestamp: 2026-02-20

- hypothesis: web_pages_gz.cpp stale/out of sync
  evidence: build_web_assets.js ran clean; JS syntax check passed
  timestamp: 2026-02-20

## Evidence

- timestamp: 2026-02-20
  checked: web_pages.cpp diff
  found: Changes only removed Emergency Limiter and USB auto-priority UI; DSP page HTML/JS unchanged
  implication: DSP page breakage is not from JS changes

- timestamp: 2026-02-20
  checked: build output
  found: Firmware compiles successfully; 992 native tests pass
  implication: No compile-time errors; backend logic is correct

- timestamp: 2026-02-20
  checked: WS_BUF_SIZE in websocket_handler.cpp
  found: WS_BUF_SIZE = 4096, introduced in commit 87bce1e (heap optimization)
  implication: Any JSON > 4096 bytes gets truncated silently

- timestamp: 2026-02-20
  checked: DSP state JSON size (calculated via Node.js simulation)
  found: Default DSP state (6 channels × 10 disabled PEQ bands + 32 preset slots) = 5202 bytes
  implication: ALWAYS overflows 4096-byte buffer; JSON is truncated by 1106 bytes every time

- timestamp: 2026-02-20
  checked: JSON truncation effect
  found: serializeJson() with too-small buffer silently truncates; broadcastTXT sends len bytes (truncated); JSON.parse() throws SyntaxError on client; ws.onmessage has no try/catch around JSON.parse()
  implication: dspState stays null → graph blank, "add preset" fires but ws.send silently skips, no features work

- timestamp: 2026-02-20
  checked: Old sendDspState() in commit 87bce1e^
  found: Used "String json; serializeJson(doc, json); broadcastTXT(json)" — no size limit
  implication: DSP page worked before heap optimization commit; broke when 4096-byte buffer was introduced

## Resolution

root_cause: Commit 87bce1e introduced WS_BUF_SIZE = 4096 bytes for the PSRAM-backed WebSocket serialization buffer. The DSP state JSON (6 channels × 10 PEQ bands + 32 preset slots) is ~5202 bytes in the default all-disabled configuration, overflowing by 1106 bytes. When stages are enabled with biquad coefficients, the JSON grows to ~16 KB. The truncated JSON causes JSON.parse() to throw SyntaxError in ws.onmessage, leaving dspState = null permanently, so the entire DSP page is non-functional (blank graph, silent no-ops on add/save/remove).

fix: Increased WS_BUF_SIZE from 4096 to 16384 bytes in websocket_handler.cpp (PSRAM allocation, so memory cost is negligible). Added truncation guard with LOG_W + String fallback in both wsBroadcastJson() and wsSendJson() so that any future overflow never sends truncated JSON.

verification: |
  - Firmware builds successfully (pio run: SUCCESS)
  - All 992 native tests pass (0 failures)
  - JSON size of default DSP state (5202 bytes) now fits within 16384-byte buffer
  - Fallback guard prevents silent truncation for any future oversized messages

files_changed:
  - src/websocket_handler.cpp: WS_BUF_SIZE 4096→16384; added truncation guard with LOG_W+String fallback in wsBroadcastJson() and wsSendJson()
