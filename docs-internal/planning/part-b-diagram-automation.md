# Part B: Architecture Diagram Drift Prevention

**Status**: PLAN READY
**Date**: 2026-03-21
**Depends on**: Part A (doc-mapping 100% coverage + `check_mapping_coverage.js` CI gate) — COMPLETE

---

## Research Review & Architectural Decisions

### Challenging the Agent Findings

Three research agents investigated this problem. Their findings were largely accurate, but several critical issues need correction before implementation.

**Issue 1: The B+C hybrid has a token budget problem the agents understated.**

The `generate_docs.js` pipeline sends all source files mapped to a section in a single API call with `max_tokens: 8192` output. The `developer/architecture` section already maps to 17 source files (all 15 state headers + app_state.h + app_state.cpp + app_events.h/cpp + enums.h + eth_manager + CLAUDE.md + ARCHITECTURE.md). Adding diagram generation instructions to the prompt for sections that already produce long prose risks hitting the 8192 token ceiling and getting truncated mid-diagram. The existing architecture.md already has 6 inline Mermaid diagrams plus substantial prose — it is likely close to the limit.

**Decision**: Do NOT attempt to make the AI "co-generate" diagrams and prose in a single call. Instead, treat diagrams as a **separate validation concern** (Phase 1) and fix the 4 missing diagrams by running targeted one-shot regenerations of specific sections (Phase 3). The existing pipeline already produces inline Mermaid when the prompt template asks for it — the problem is not generation capability but regeneration triggers.

**Issue 2: The test count drift (1,579 vs 1,669+) is NOT a diagram problem — it is a regeneration trigger gap.**

The `testing.md` page maps to `docs-internal/testing-architecture.md` as its sole input. When new test modules are added (e.g., `test_hal_coord`, `test_sink_write_utils`), no source file in `doc-mapping.json` triggers regeneration of `developer/testing`. The testing architecture doc at `docs-internal/testing-architecture.md` does not get updated either — it is a static working doc. The fix is to add `platformio.ini` (which lists test environments) and `test/` glob patterns to `doc-mapping.json` targeting `developer/testing`.

**Decision**: Phase 2 adds `platformio.ini` and `.github/workflows/tests.yml` as inputs to `developer/testing` and `developer/contributing` sections.

**Issue 3: The `.mmd` files are OUTPUT, not INPUT — mapping them as triggers is wrong.**

Agent 2 suggested treating `.mmd` files as inputs to `doc-mapping.json`. This is backwards. The `.mmd` files are developer working documents — they are manually updated by developers as they work on features. They are often MORE detailed than what belongs in public docs. The correct approach is:

- The `.mmd` files remain as developer reference (untouched)
- The source of truth for public diagrams is the inline Mermaid in Docusaurus `.md` files
- Source code changes (the actual INPUT) trigger section regeneration, which produces inline Mermaid
- A static validator (Phase 1) catches cases where source identifiers referenced in `.mmd` files no longer exist

**Decision**: Do NOT add `.mmd` files to `doc-mapping.json`. The validator reads `.mmd` files to find identifiers to check, but the regeneration trigger path remains: source file change -> `detect_doc_changes.js` -> `generate_docs.js`.

**Issue 4: The `doc-coverage` CI gate exists but diagrams in testing.md and contributing.md do not mention it.**

The `tests.yml` workflow now has 5 gates (cpp-tests, cpp-lint, js-lint, e2e-tests, doc-coverage) but the inline Mermaid diagrams in `testing.md` and `contributing.md` show only 4. The `ci-quality-gates.mmd` in docs-internal also shows only 4 (though it has the correct test count of 1,669). This is exactly the drift this plan prevents.

**Issue 5: The `UNKNOWN -> DETECTED` transition is missing from the inline device-lifecycle.md diagram.**

The inline Mermaid in `device-lifecycle.md` jumps from `UNKNOWN` directly to `CONFIGURING`. The `.mmd` file and the source code both include `DETECTED` as an intermediate state. The prose section below the diagram correctly describes all 8 states including DETECTED. This is a diagram-specific gap.

---

## Phase 1: Static Diagram Validator

**Goal**: Detect when source code symbols referenced in architecture diagrams no longer exist.
**Effort**: ~2 hours
**Files to create**: `tools/diagram-validation.js`
**Files to modify**: `.github/workflows/tests.yml`, `.githooks/pre-commit`

### Design

The validator reads each `.mmd` file, extracts identifiers to check, and greps for them in the mapped source files. It does NOT parse Mermaid syntax — it uses a simple declaration-based approach where each `.mmd` file has a comment block listing the identifiers to validate.

**Annotation format** (add to top of each `.mmd` file):

```
%% @validate-symbols
%% src/state/enums.h: AppFSMState, STATE_IDLE, STATE_SIGNAL_DETECTED, STATE_AUTO_OFF_TIMER, STATE_WEB_CONFIG, STATE_OTA_UPDATE, STATE_ERROR
%% src/app_events.h: EVT_OTA, EVT_DISPLAY, EVT_BUZZER, EVT_SIGGEN, EVT_ANY
%% @end-validate
```

The tool parses `@validate-symbols` blocks, reads each referenced source file, and checks that every listed identifier appears as a substring. Exit 1 on any miss.

### Identifiers to Validate Per Diagram

**`system-architecture.mmd`** — Core task names, HAL constants, pipeline structure:
```
%% @validate-symbols
%% src/config.h: TASK_CORE_AUDIO, TASK_CORE_GUI, HAL_MAX_DEVICES
%% src/hal/hal_device_manager.h: HalDeviceManager, HAL_MAX_PINS, HAL_GPIO_MAX
%% src/hal/hal_pipeline_bridge.h: hal_pipeline_sync, hal_pipeline_state_change
%% src/hal/hal_discovery.h: HalDiscovery
%% src/hal/hal_device_db.h: HalDeviceDB
%% src/hal/hal_settings.h: hal_apply_config
%% src/audio_pipeline.h: audio_pipeline_set_sink, audio_pipeline_remove_sink, audio_pipeline_set_source
%% src/app_events.h: EVT_ANY
%% src/app_state.h: AppState
%% @end-validate
```

**`event-architecture.mmd`** — Event bits and dirty flag pattern:
```
%% @validate-symbols
%% src/app_events.h: EVT_OTA, EVT_DISPLAY, EVT_BUZZER, EVT_SIGGEN, EVT_DSP_CONFIG, EVT_DAC, EVT_EEPROM, EVT_USB_AUDIO, EVT_USB_VU, EVT_SETTINGS, EVT_ADC_ENABLED, EVT_DIAG, EVT_ETHERNET, EVT_DAC_SETTINGS, EVT_HAL_DEVICE, EVT_CHANNEL_MAP, EVT_ANY, app_events_signal, app_events_wait
%% src/app_state.h: markHalDeviceDirty, clearHalDeviceDirty, isHalDeviceDirty
%% @end-validate
```

**`hal-lifecycle.mmd`** — Device state enum values:
```
%% @validate-symbols
%% src/hal/hal_types.h: HAL_STATE_UNKNOWN, HAL_STATE_DETECTED, HAL_STATE_CONFIGURING, HAL_STATE_AVAILABLE, HAL_STATE_UNAVAILABLE, HAL_STATE_DISABLED, HAL_STATE_REMOVED
%% src/hal/hal_pipeline_bridge.h: hal_pipeline_state_change
%% src/state/hal_coord_state.h: requestDeviceToggle
%% @end-validate
```

**`hal-pipeline-bridge.mmd`** — Bridge API functions and capability constants:
```
%% @validate-symbols
%% src/hal/hal_pipeline_bridge.h: hal_pipeline_state_change, hal_pipeline_sync
%% src/hal/hal_types.h: HAL_CAP_DAC_PATH, HAL_CAP_ADC_PATH
%% src/audio_pipeline.h: audio_pipeline_set_sink, audio_pipeline_remove_sink, audio_pipeline_set_source, audio_pipeline_remove_source
%% src/app_state.h: markHalDeviceDirty, markChannelMapDirty
%% @end-validate
```

**`boot-sequence.mmd`** — Init function names called from setup():
```
%% @validate-symbols
%% src/hal/hal_builtin_devices.h: hal_register_builtins
%% src/hal/hal_device_db.h: hal_db_init
%% src/hal/hal_settings.h: hal_load_device_configs
%% src/hal/hal_pipeline_bridge.h: hal_pipeline_sync
%% src/i2s_audio.h: i2s_audio_init
%% src/dac_hal.h: dac_secondary_init, dac_output_init
%% @end-validate
```

**`sink-dispatch.mmd`** — Sink API and pipeline types:
```
%% @validate-symbols
%% src/audio_output_sink.h: AudioOutputSink
%% src/audio_pipeline.h: audio_pipeline_set_sink, audio_pipeline_remove_sink
%% src/hal/hal_types.h: HAL_CAP_DAC_PATH
%% @end-validate
```

**`ci-quality-gates.mmd`** — Job names (validate against workflow file):
```
%% @validate-symbols
%% .github/workflows/tests.yml: cpp-tests, cpp-lint, js-lint, e2e-tests, doc-coverage, build
%% @end-validate
```

**`test-coverage-map.mmd`** — Test infrastructure existence:
```
%% @validate-symbols
%% e2e/helpers/fixtures.js: connectedPage
%% e2e/helpers/selectors.js: selectors
%% e2e/mock-server/ws-state.js: ws-state
%% @end-validate
```

**`test-infrastructure.mmd`** — Same as test-coverage-map plus assembler:
```
%% @validate-symbols
%% e2e/mock-server/assembler.js: assembler
%% e2e/mock-server/server.js: server
%% e2e/helpers/fixtures.js: connectedPage
%% @end-validate
```

**`e2e-test-flow.mmd`** — WS auth flow identifiers:
```
%% @validate-symbols
%% e2e/helpers/fixtures.js: connectedPage
%% e2e/helpers/ws-helpers.js: buildInitialState
%% @end-validate
```

### Algorithm (`tools/diagram-validation.js`)

```
1. Glob docs-internal/architecture/*.mmd
2. For each .mmd file:
   a. Parse @validate-symbols block (if absent, skip with warning)
   b. For each line: extract source_file: id1, id2, id3
   c. Read source_file (relative to repo root)
   d. For each identifier, check content.includes(identifier)
   e. Collect failures: { diagram, sourceFile, missingIdentifier }
3. Report failures and exit 1 if any
```

### CI Integration

Add to `.github/workflows/tests.yml` in the `js-lint` job (reuse its Node.js setup):

```yaml
    - name: Validate architecture diagrams
      run: node tools/diagram-validation.js
```

Add to `.githooks/pre-commit` as step 4 (after ESLint):

```bash
node tools/diagram-validation.js
```

### Acceptance Criteria

- [ ] `node tools/diagram-validation.js` exits 0 on current codebase
- [ ] Removing a symbol from source (e.g., renaming `EVT_BUZZER`) causes exit 1 with clear error message naming the diagram and missing symbol
- [ ] `.mmd` files without `@validate-symbols` block produce a warning but do not fail
- [ ] CI `js-lint` job includes the validation step
- [ ] Pre-commit hook runs the validation

---

## Phase 2: Fix Regeneration Triggers in doc-mapping.json

**Goal**: Ensure source code changes that affect diagrams trigger the correct section regeneration.
**Effort**: ~30 minutes
**Files to modify**: `tools/doc-mapping.json`

### Mappings to Add

The following source files are currently unmapped but affect documentation sections that contain inline diagrams:

```json
"platformio.ini": ["developer/testing", "developer/build-setup"],
".github/workflows/tests.yml": ["developer/testing", "developer/contributing"],
".github/workflows/docs.yml": ["developer/contributing"]
```

**Rationale**:
- `platformio.ini` lists test environments and their configuration. Adding/removing test modules changes the native test count. This triggers `developer/testing` regeneration so test counts stay current.
- `tests.yml` defines the CI quality gates. Adding/removing a gate (like the new `doc-coverage` job) should trigger regeneration of `developer/testing` (CI gates diagram) and `developer/contributing` (CI gates diagram).
- `docs.yml` defines the docs CI pipeline. Changes should trigger `developer/contributing` regeneration.

### What NOT to Add

- `.mmd` files are NOT added as inputs. They are developer working docs, not source-of-truth inputs.
- `test/` directories are NOT added (too broad — every test change would trigger regeneration of testing docs, which is wasteful). The test count comes from `platformio.ini` environment configuration, which changes less frequently.

### Acceptance Criteria

- [ ] `node tools/check_mapping_coverage.js` still passes (new entries do not break existing coverage)
- [ ] `node tools/detect_doc_changes.js` correctly outputs `["developer/testing", "developer/contributing"]` when `tests.yml` changes
- [ ] `node tools/detect_doc_changes.js` correctly outputs `["developer/testing", "developer/build-setup"]` when `platformio.ini` changes

---

## Phase 3: Add Missing Inline Diagrams to Docusaurus Pages

**Goal**: The 4 diagrams that exist only in `.mmd` files become visible in public docs.
**Effort**: ~1 hour (4 section regenerations)
**Prerequisite**: Phase 2 complete (so regeneration triggers are correct)

### 3A: Event Architecture -> `developer/architecture`

The `architecture.md` page already has 6 diagrams but is missing the event-driven dirty-flag/event-group sequence diagram from `event-architecture.mmd`.

**Trigger**: Run targeted regeneration. The source files already mapped to `developer/architecture` include `app_events.h`, `app_events.cpp`, and all state headers — sufficient context for the AI to produce the event architecture diagram.

```bash
node tools/generate_docs.js --sections developer/architecture
```

**Verification**: After regeneration, confirm `architecture.md` contains a `sequenceDiagram` or equivalent showing the dirty-flag -> event-group -> main-loop -> WS dispatch flow. If the AI omits it (8192 token limit risk), manually add the diagram from `event-architecture.mmd` adapted to inline format.

**Fallback plan**: If the section is too large for a single 8192-token call, split `developer/architecture` into two sections in `doc-mapping.json`: `developer/architecture` (system overview, FSM, AppState) and `developer/architecture-events` (event-driven architecture, cross-core communication). This requires adding a new sidebar entry in `docs-site/sidebars.js`.

### 3B: Boot Sequence -> `developer/audio-pipeline`

The `audio-pipeline.md` page has 1 pipeline flow diagram but no boot sequence. The `boot-sequence.mmd` details the critical init order (HAL builtins -> DB -> configs -> I2S -> bridge sync).

**Trigger**:

```bash
node tools/generate_docs.js --sections developer/audio-pipeline
```

**Source files** already mapped include `audio_pipeline.h/.cpp`, `i2s_audio.h/.cpp`, `hal_pipeline_bridge.h/.cpp`, `hal_i2s_bridge.h/.cpp` — sufficient context for the boot sequence.

**Verification**: Confirm output includes a sequence diagram or ordered list showing the init order. The key elements: `hal_register_builtins()` before `hal_db_init()`, I2S ADC2 before ADC1, bridge sync after all registration.

### 3C: Test Coverage Map -> `developer/testing`

This is the biggest gap: no inline equivalent exists anywhere in public docs. The `test-coverage-map.mmd` shows which test layer (unit, E2E, static analysis, contract) covers which system boundary.

**Trigger**:

```bash
node tools/generate_docs.js --sections developer/testing
```

**Source files** mapped: `docs-internal/testing-architecture.md`. This single input may not provide enough context for the AI to generate a coverage map. If the output lacks a coverage diagram, manually add an adapted version of `test-coverage-map.mmd` to the generated `testing.md`.

**Alternative**: Add `e2e/playwright.config.js` and `e2e/package.json` to `doc-mapping.json` targeting `developer/testing` to give the AI more context about the E2E test structure.

### 3D: Sink Dispatch Decision Tree -> `developer/audio-pipeline`

The `audio-pipeline.md` has the pipeline flow diagram but not the slot-indexed dispatch decision tree from `sink-dispatch.mmd`. This is handled by the same regeneration as 3B.

**Verification**: Confirm the regenerated `audio-pipeline.md` includes the sink dispatch loop logic (check NULL -> check isReady -> check muted -> apply gain -> convert -> write).

### Regeneration Commands Summary

```bash
# Phase 3: Regenerate the 3 affected sections (4 diagrams across 3 pages)
node tools/generate_docs.js --sections developer/architecture
node tools/generate_docs.js --sections developer/audio-pipeline
node tools/generate_docs.js --sections developer/testing
```

### Acceptance Criteria

- [ ] `developer/architecture.md` contains event architecture diagram (dirty flags + event group flow)
- [ ] `developer/audio-pipeline.md` contains boot sequence diagram showing init order
- [ ] `developer/audio-pipeline.md` contains sink dispatch decision tree
- [ ] `developer/testing.md` contains test coverage map showing unit/E2E/static boundaries
- [ ] `npm run build` in `docs-site/` succeeds with all new inline Mermaid rendering

---

## Phase 4: Fix Stale Content in Existing Inline Diagrams

**Goal**: Correct known stale data in currently-published diagrams.
**Effort**: ~1 hour
**Approach**: Regeneration via `generate_docs.js` with corrected triggers, plus one manual fixup.

### 4A: Test Count Drift

**Current stale values**:
- `testing.md` line 15: "1,579 tests / 66 modules" (actual: ~1,669 tests / 72 modules)
- `testing.md` line 24: "Run all 1,579 tests" (actual: ~1,669)
- `testing.md` line 301: CI diagram node "1,579 Unity tests" (actual: ~1,669)
- `contributing.md` line 80: "1,614 Unity tests / 70 modules" (actual: ~1,669 / 72)

**Fix**: Phase 2 adds `platformio.ini` and `tests.yml` as triggers for `developer/testing` and `developer/contributing`. After Phase 2, regenerate:

```bash
node tools/generate_docs.js --sections developer/testing,developer/contributing
```

The AI reads CLAUDE.md (mapped to `developer/architecture` but available as context via the system prompt) which states "~1620 tests across 70 modules". This number is also stale in CLAUDE.md itself. To ensure accuracy:

1. Run `pio test -e native -v 2>&1 | tail -5` to get the actual current test count
2. Update CLAUDE.md with the correct number
3. Then regenerate the sections

### 4B: Missing `doc-coverage` Gate in CI Diagrams

The `tests.yml` workflow now has 5 gates but all inline CI diagrams show only 4. After Phase 2 adds `tests.yml` as a trigger, regeneration will produce updated diagrams. Also update `ci-quality-gates.mmd`:

Add `doc-coverage` node:
```mermaid
DOC["doc-coverage\nnode tools/check_mapping_coverage.js"]
```

And update the `@validate-symbols` block to include `doc-coverage`.

### 4C: Missing DETECTED State in device-lifecycle.md

The inline Mermaid in `device-lifecycle.md` (line 13-31) jumps from UNKNOWN directly to CONFIGURING, skipping DETECTED. The `.mmd` file and the source code both include DETECTED. The prose table below the diagram correctly lists all 8 states.

**Fix**: This page maps to `developer/hal/device-lifecycle` and is triggered by changes to `src/hal/hal_device.h`. Regenerate:

```bash
node tools/generate_docs.js --sections developer/hal/device-lifecycle
```

If the AI still omits DETECTED (it is described as informational and "most builtin devices skip this state"), manually add the transition:

```
UNKNOWN --> DETECTED : probe() ACK / I2C scan
DETECTED --> CONFIGURING : driver matched
```

### Acceptance Criteria

- [ ] `testing.md` shows current test count (matching `pio test -e native -v` output)
- [ ] `testing.md` CI diagram shows 5 gates including `doc-coverage`
- [ ] `contributing.md` CI diagram shows 5 gates including `doc-coverage`
- [ ] `ci-quality-gates.mmd` shows 5 gates including `doc-coverage`
- [ ] `device-lifecycle.md` inline diagram includes UNKNOWN -> DETECTED -> CONFIGURING path
- [ ] All pages build successfully with `cd docs-site && npm run build`

---

## Future-Proofing: How This Prevents Future Drift

### When a developer adds a new source file:

1. `check_mapping_coverage.js` (CI gate) fails if the file is not in `doc-mapping.json` or `excludes` — forces the developer to map it to a section
2. On next push to `main`, `detect_doc_changes.js` detects the new file and triggers regeneration of the mapped section(s)
3. The AI regenerates the section with current source code context, producing updated inline Mermaid

### When a developer renames/removes a symbol:

1. `diagram-validation.js` (CI gate) fails with a clear message: "boot-sequence.mmd references `hal_register_builtins` but it was not found in `src/hal/hal_builtin_devices.h`"
2. The developer updates the `.mmd` file's `@validate-symbols` block (and optionally the diagram itself)
3. The inline Docusaurus diagram is regenerated on next push via the normal pipeline

### When a developer adds a new CI gate:

1. `tests.yml` is mapped to `developer/testing` and `developer/contributing` (Phase 2)
2. Change triggers regeneration of both sections
3. `diagram-validation.js` catches if `ci-quality-gates.mmd` still references old gate names

### What is NOT automated (and why):

- `.mmd` file content is not auto-generated. These are developer working docs with detailed notes, color coding, and layout decisions that require human judgment. The validator catches staleness; the human fixes it.
- Diagram layout/style is not enforced. Mermaid diagrams are a communication tool; auto-generated layouts often look worse than hand-crafted ones.

---

## Implementation Notes

### Git Worktree

All phases should be implemented on a feature branch `feature/diagram-drift-prevention`. The implementation will happen in a git worktree to avoid disrupting ongoing work.

### Phase Order

Phases 1 and 2 are independent and can be implemented in parallel. Phase 3 depends on Phase 2 (correct triggers). Phase 4 depends on Phase 2 and can run in parallel with Phase 3.

Recommended order: Phase 1 + Phase 2 (parallel) -> Phase 3 + Phase 4 (parallel) -> Final verification.

### Testing the Validator

Before committing Phase 1, test the validator by temporarily renaming a symbol in source and confirming it catches the break. Then revert.

### Token Budget Risk Mitigation

If `developer/architecture` regeneration produces truncated output (>8192 tokens), the fallback is to split it into two sections. Monitor the `generate_docs.js` log for output token counts close to 8192.

---

## Executive Summary

This plan prevents architecture diagram drift through a two-layer defense: a zero-cost static validator (`diagram-validation.js`) that catches symbol renames and removals in CI, and corrected regeneration triggers in `doc-mapping.json` that ensure prose-with-diagrams pages are regenerated when their upstream source files change. The existing `generate_docs.js` pipeline already produces inline Mermaid when prompted — the root cause of drift was not generation capability but missing trigger mappings. Four diagram gaps are closed by targeted section regeneration, and five known stale values are corrected. The `.mmd` files in `docs-internal/architecture/` remain as detailed developer references but are not treated as source-of-truth inputs to the automated pipeline.
