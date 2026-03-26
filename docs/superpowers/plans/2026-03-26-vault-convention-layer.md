# Obsidian Vault Enhancement — Convention Layer

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 3 new note types (handoff, capacity-budget, deprecation-log), cross-linking conventions, and dashboard queries to the Obsidian Vault — closing the gaps between Claude Memory, CLAUDE.md, and the Vault.

**Architecture:** No new tooling. Enrich existing Vault with templates, seeded data notes, expanded Dashboard Dataview queries, and updated CLAUDE.md instructions in both project and Vault. All changes are markdown files.

**Tech Stack:** Obsidian (Templater, Dataview plugins), Markdown, YAML frontmatter

---

## File Map

### New Files — Templates
- `Vault/Templates/Handoff.md` — Templater template for session handoff notes
- `Vault/Templates/Capacity-Budget.md` — Templater template for resource ceiling tracking
- `Vault/Templates/Deprecation-Log.md` — Templater template for removed API registry

### New Files — Seeded Data
- `Vault/Projects/ALX Nova/Handoffs/.gitkeep` — Empty directory placeholder
- `Vault/Projects/ALX Nova/Capacity/capacity-budget-event-bits.md` — Seeded from CLAUDE.md
- `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-slots.md` — Seeded from CLAUDE.md
- `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-drivers.md` — Seeded from CLAUDE.md
- `Vault/Projects/ALX Nova/Capacity/capacity-budget-pipeline-sinks.md` — Seeded from CLAUDE.md
- `Vault/Projects/ALX Nova/Deprecations/deprecation-log-i2s.md` — Known I2S removals
- `Vault/Projects/ALX Nova/Deprecations/deprecation-log-hal.md` — Known HAL removals
- `Vault/Projects/ALX Nova/Deprecations/deprecation-log-websocket.md` — Known WS removals
- `Vault/Projects/ALX Nova/Deprecations/deprecation-log-rest.md` — Known REST removals

### Modified Files
- `Vault/Templates/ADR.md` — Add `pr:` and `source_files:` fields
- `Vault/Templates/Concept.md` — Add `adr:`, `branch:`, `pr:` fields
- `Vault/Projects/ALX Nova/Dashboard.md` — Add 3 Dataview sections, reorder
- `Vault/CLAUDE.md` — Add cross-linking rules, register 3 new note types
- `ALX_Nova_Controller_2/CLAUDE.md` — Expand Obsidian Vault section with integration rules

### Backfill (existing ADRs)
- All 24 ADRs in `Vault/Projects/ALX Nova/ADRs/` — Add `pr:` field to frontmatter

---

## Task 1: Create New Templates

**Files:**
- Create: `Vault/Templates/Handoff.md`
- Create: `Vault/Templates/Capacity-Budget.md`
- Create: `Vault/Templates/Deprecation-Log.md`

- [ ] **Step 1: Create Handoff template**

Write `Vault/Templates/Handoff.md`:

```markdown
---
type: handoff
project:
branch:
status: paused
relates_to: []
created: {{date:YYYY-MM-DD}}
resumed:
tags: []
---

# Handoff: {{title}}

## What I Was Doing



## Current State

- [ ]

## Key Context Claude Needs



## Files Touched

-
```

- [ ] **Step 2: Create Capacity-Budget template**

Write `Vault/Templates/Capacity-Budget.md`:

```markdown
---
type: capacity-budget
project:
resource:
total:
assigned:
reserved: 0
updated: {{date:YYYY-MM-DD}}
tags: [capacity]
---

# Capacity Budget: {{title}}

## Current Allocation

| Slot | Name | Added | Notes |
|------|------|-------|-------|
|      |      |       |       |

## Freed / Reclaimable

-

## Projection


```

- [ ] **Step 3: Create Deprecation-Log template**

Write `Vault/Templates/Deprecation-Log.md`:

```markdown
---
type: deprecation-log
project:
domain:
updated: {{date:YYYY-MM-DD}}
tags: [deprecation]
---

# Deprecation Log: {{title}}

## Removed APIs

| API | Removed In | Replaced By | Notes |
|-----|-----------|-------------|-------|
|     |           |             |       |
```

- [ ] **Step 4: Verify templates have valid YAML frontmatter**

Open each template in a text editor and confirm YAML between `---` delimiters parses cleanly. Templater macros (`{{date:YYYY-MM-DD}}`, `{{title}}`) should be the only non-standard tokens.

- [ ] **Step 5: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add Templates/Handoff.md Templates/Capacity-Budget.md Templates/Deprecation-Log.md
git commit -m "feat: add handoff, capacity-budget, and deprecation-log templates"
```

---

## Task 2: Update Existing Templates with Cross-Link Fields

**Files:**
- Modify: `Vault/Templates/ADR.md`
- Modify: `Vault/Templates/Concept.md`

- [ ] **Step 1: Add cross-link fields to ADR template**

In `Vault/Templates/ADR.md`, add `pr:` and `source_files: []` to the YAML frontmatter, after the `superseded_by:` field:

```yaml
---
type: adr
adr_number:
project:
status: proposed
superseded_by:
pr:
source_files: []
created: {{date:YYYY-MM-DD}}
tags: []
---
```

No changes to the body — just frontmatter.

- [ ] **Step 2: Add cross-link fields to Concept template**

In `Vault/Templates/Concept.md`, add `adr:`, `branch:`, `pr:` to the YAML frontmatter, after the `effort:` field:

```yaml
---
type: concept
project:
status: raw
priority:
effort:
adr:
branch:
pr:
source: manual
created: {{date:YYYY-MM-DD}}
updated: {{date:YYYY-MM-DD}}
sources: []
tags: []
---
```

No changes to the body.

- [ ] **Step 3: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add Templates/ADR.md Templates/Concept.md
git commit -m "feat: add cross-link fields (pr, source_files, adr, branch) to ADR and Concept templates"
```

---

## Task 3: Create Handoffs Directory

**Files:**
- Create: `Vault/Projects/ALX Nova/Handoffs/.gitkeep`

- [ ] **Step 1: Create the Handoffs directory with .gitkeep**

```bash
mkdir -p "C:/Users/Necrosis/Documents/GitHub/Vault/Projects/ALX Nova/Handoffs"
touch "C:/Users/Necrosis/Documents/GitHub/Vault/Projects/ALX Nova/Handoffs/.gitkeep"
```

- [ ] **Step 2: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add "Projects/ALX Nova/Handoffs/.gitkeep"
git commit -m "chore: create Handoffs directory for session pause/resume notes"
```

---

## Task 4: Seed Capacity Budget Notes

**Files:**
- Create: `Vault/Projects/ALX Nova/Capacity/capacity-budget-event-bits.md`
- Create: `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-slots.md`
- Create: `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-drivers.md`
- Create: `Vault/Projects/ALX Nova/Capacity/capacity-budget-pipeline-sinks.md`

Data sourced from project CLAUDE.md and Claude Memory.

- [ ] **Step 1: Research current event bit allocation**

Read `ALX_Nova_Controller_2/src/app_events.h` to get the exact bit-to-name mapping for all 18 assigned event bits. Record which bits are assigned, which are freed (5, 13), and which are spare (20-23).

- [ ] **Step 2: Research current HAL slot allocation**

Read `ALX_Nova_Controller_2/src/hal/hal_builtin_devices.cpp` to identify all 14 onboard devices registered at boot with their slot assignments.

- [ ] **Step 3: Research current HAL driver registry**

Read `ALX_Nova_Controller_2/src/hal/hal_device_db.cpp` to get the list of all 44 registered drivers and their compatible strings.

- [ ] **Step 4: Research current pipeline sink count**

Read `ALX_Nova_Controller_2/src/audio_pipeline.h` to confirm `AUDIO_PIPELINE_MAX_OUTPUTS` (16) and current active sink count.

- [ ] **Step 5: Create capacity-budget-event-bits.md**

Write `Vault/Projects/ALX Nova/Capacity/capacity-budget-event-bits.md` with full allocation table from Step 1:

```markdown
---
type: capacity-budget
project: ALX Nova
resource: event-bits
total: 24
assigned: 18
reserved: 0
updated: 2026-03-26
tags: [capacity]
---

# Capacity Budget: Event Bits

Defined in `src/app_events.h`. FreeRTOS event group: 24 usable bits (0-23).

## Current Allocation

| Bit | Name | Added | Notes |
|------|------|-------|-------|
| 0 | EVT_xxx | ... | ... |
<!-- Populate from Step 1 research — exact names from app_events.h -->

## Freed / Reclaimable

- Bit 5: freed in PR #95
- Bit 13: freed in PR #95

## Projection

18 assigned, 6 spare (bits 20-23 + freed 5, 13). At ~2 bits/month, ~3 months headroom. Mitigation: combine related events with sub-flags, or expand to two event groups.
```

**Note:** The table rows MUST be populated from the actual `app_events.h` content read in Step 1. No placeholders.

- [ ] **Step 6: Create capacity-budget-hal-slots.md**

Write `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-slots.md` with the 14 onboard devices from Step 2:

```markdown
---
type: capacity-budget
project: ALX Nova
resource: hal-slots
total: 32
assigned: 14
reserved: 0
updated: 2026-03-26
tags: [capacity]
---

# Capacity Budget: HAL Device Slots

`HAL_MAX_DEVICES=32` in `src/hal/hal_device_manager.h`. 14 onboard at boot + up to 2 expansion.

## Current Allocation

| Slot | Device | Type | Notes |
|------|--------|------|-------|
| 0 | ... | onboard | ... |
<!-- Populate from Step 2 research — exact devices from hal_builtin_devices.cpp -->

## Freed / Reclaimable

None — all 14 onboard slots are permanent.

## Projection

14/32 assigned. 18 spare slots for expansion mezzanines (each adds 1-2 devices). Comfortable headroom.
```

**Note:** Table rows from Step 2 research.

- [ ] **Step 7: Create capacity-budget-hal-drivers.md**

Write `Vault/Projects/ALX Nova/Capacity/capacity-budget-hal-drivers.md`:

```markdown
---
type: capacity-budget
project: ALX Nova
resource: hal-drivers
total: 64
assigned: 44
reserved: 0
updated: 2026-03-26
tags: [capacity]
---

# Capacity Budget: HAL Driver Registry

`HAL_MAX_DRIVERS=64` in `src/hal/hal_device_manager.h`. Registered via `HAL_REGISTER()` macro.

## Current Allocation

| # | Compatible String | Pattern | Notes |
|---|-------------------|---------|-------|
| 1 | ... | ... | ... |
<!-- Populate from Step 3 research — exact compatible strings from hal_device_db.cpp -->

## Freed / Reclaimable

None currently.

## Projection

44/64 assigned. 20 spare. Each new mezzanine chip needs 1 driver. Comfortable headroom.
```

**Note:** Table rows from Step 3 research.

- [ ] **Step 8: Create capacity-budget-pipeline-sinks.md**

Write `Vault/Projects/ALX Nova/Capacity/capacity-budget-pipeline-sinks.md`:

```markdown
---
type: capacity-budget
project: ALX Nova
resource: pipeline-sinks
total: 16
assigned: 0
reserved: 0
updated: 2026-03-26
tags: [capacity]
---

# Capacity Budget: Audio Pipeline Sinks

`AUDIO_PIPELINE_MAX_OUTPUTS=16` in `src/audio_pipeline.h`. Sinks registered dynamically by HAL devices.

## Current Allocation

Dynamic — sinks are registered/removed at runtime by HAL audio devices. Up to 14 onboard + 2 expansion devices may each register 1 sink.

## Freed / Reclaimable

N/A — dynamic allocation.

## Projection

16 max sinks vs 16 max possible audio devices. Tight 1:1 mapping. If a multi-output device needs >1 sink, increase `AUDIO_PIPELINE_MAX_OUTPUTS`.
```

- [ ] **Step 9: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add "Projects/ALX Nova/Capacity/"
git commit -m "feat: seed 4 capacity-budget notes (event-bits, hal-slots, hal-drivers, pipeline-sinks)"
```

---

## Task 5: Seed Deprecation Log Notes

**Files:**
- Create: `Vault/Projects/ALX Nova/Deprecations/deprecation-log-i2s.md`
- Create: `Vault/Projects/ALX Nova/Deprecations/deprecation-log-hal.md`
- Create: `Vault/Projects/ALX Nova/Deprecations/deprecation-log-websocket.md`
- Create: `Vault/Projects/ALX Nova/Deprecations/deprecation-log-rest.md`

- [ ] **Step 1: Research removed I2S APIs**

Search git log for removed I2S wrapper functions. Key source: PR #91 (Audio Tab Batch A) removed 19 deprecated I2S wrappers. Search `git log --all --oneline -- src/i2s_audio.*` and grep for "remove" or "deprecate" in recent commits.

- [ ] **Step 2: Research removed HAL APIs**

Search git log for removed HAL functions. Key source: PR #97 (tech debt cleanup) and the HAL consolidation that merged 42 drivers into 4 generic classes.

- [ ] **Step 3: Research removed WebSocket commands**

Search git log for removed WS commands. Key source: Audio Tab batches replaced old WS commands with HAL-routed ones.

- [ ] **Step 4: Research removed REST endpoints**

Search git log for removed REST endpoints. Check if any `/api/` routes were deprecated during API versioning work.

- [ ] **Step 5: Create deprecation-log-i2s.md**

Write with researched data:

```markdown
---
type: deprecation-log
project: ALX Nova
domain: i2s
updated: 2026-03-26
tags: [deprecation]
---

# Deprecation Log: I2S

## Removed APIs

| API | Removed In | Replaced By | Notes |
|-----|-----------|-------------|-------|
<!-- Populate from Step 1 research -->
```

- [ ] **Step 6: Create deprecation-log-hal.md, deprecation-log-websocket.md, deprecation-log-rest.md**

Same pattern — one file per domain, populated from Steps 2-4 research. If a domain has no known removals yet, create the file with an empty table and a note: "No removals recorded yet."

- [ ] **Step 7: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add "Projects/ALX Nova/Deprecations/"
git commit -m "feat: seed deprecation-log notes for i2s, hal, websocket, rest domains"
```

---

## Task 6: Expand Dashboard

**Files:**
- Modify: `Vault/Projects/ALX Nova/Dashboard.md`

- [ ] **Step 1: Rewrite Dashboard.md with new sections and reordered layout**

The new section order:
1. Active Handoffs *(new)*
2. Capacity Budgets *(new)*
3. Open Concepts *(existing)*
4. Architecture Decisions *(existing)*
5. Deprecation Logs *(new)*
6. Recent Clips *(existing)*
7. Meetings *(existing)*

Full replacement content for `Dashboard.md`:

```markdown
---
type: note
project: ALX Nova
created: 2026-03-26
tags: [dashboard]
---

# ALX Nova Dashboard

## Active Handoffs

```dataview
TABLE branch, status, created
FROM "Projects/ALX Nova/Handoffs"
WHERE type = "handoff" AND status = "paused"
SORT created DESC
```

## Capacity Budgets

```dataview
TABLE resource, total, assigned, (total - assigned) AS "spare", updated
FROM "Projects/ALX Nova/Capacity"
WHERE type = "capacity-budget"
SORT (total - assigned) ASC
```

## Open Concepts

```dataview
TABLE status, priority, effort
FROM ""
WHERE type = "concept" AND project = "ALX Nova" AND status != "done" AND status != "archived"
SORT priority ASC
```

## Architecture Decisions

```dataview
TABLE adr_number AS "#", status, created
FROM "Projects/ALX Nova/ADRs"
WHERE type = "adr"
SORT adr_number DESC
LIMIT 10
```

## Deprecation Logs

```dataview
TABLE domain, updated
FROM "Projects/ALX Nova/Deprecations"
WHERE type = "deprecation-log"
SORT domain ASC
```

## Recent Clips

```dataview
TABLE source_url, created
FROM ""
WHERE type = "clip" AND project = "ALX Nova"
SORT created DESC
LIMIT 10
```

## Meetings

```dataview
TABLE participants, created
FROM ""
WHERE type = "meeting" AND project = "ALX Nova"
SORT created DESC
LIMIT 5
```
```

- [ ] **Step 2: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add "Projects/ALX Nova/Dashboard.md"
git commit -m "feat: expand dashboard with handoff, capacity, and deprecation sections"
```

---

## Task 7: Update Vault CLAUDE.md

**Files:**
- Modify: `Vault/CLAUDE.md`

- [ ] **Step 1: Add cross-linking rules section**

After the existing "## Architecture Decision Records" section in `Vault/CLAUDE.md`, add:

```markdown
## Cross-Linking Rules

- ADRs: always fill `pr:` after merge, `source_files:` with 2-5 primary files
- Concepts: update `adr:` when design is decided, `branch:` when work starts, `pr:` when merged
- Handoffs: set status to `resumed` with date when picked back up — never delete, they're history
- Capacity-budgets: update `assigned` count and allocation table after any PR that consumes or frees a resource
- Deprecation-logs: append-only — never remove entries, even if the replacement is later removed too
```

- [ ] **Step 2: Register new note types in frontmatter quick reference**

Add after the existing `# Note` block in the frontmatter section:

```yaml
# Handoff
---
type: handoff
project:
branch:
status: paused
relates_to: []
created: YYYY-MM-DD
resumed:
tags: []
---

# Capacity Budget
---
type: capacity-budget
project:
resource:
total:
assigned:
reserved: 0
updated: YYYY-MM-DD
tags: [capacity]
---

# Deprecation Log
---
type: deprecation-log
project:
domain:
updated: YYYY-MM-DD
tags: [deprecation]
---
```

- [ ] **Step 3: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add CLAUDE.md
git commit -m "docs: add cross-linking rules and register handoff, capacity-budget, deprecation-log note types"
```

---

## Task 8: Update Project CLAUDE.md

**Files:**
- Modify: `ALX_Nova_Controller_2/CLAUDE.md` (lines 185-189, the "Obsidian Vault" section)

- [ ] **Step 1: Replace the Obsidian Vault section**

Replace the current 3-line section:

```markdown
## Obsidian Vault

Architecture Decision Records and project notes are tracked in the Obsidian Vault.
Vault path: `C:\Users\Necrosis\Documents\GitHub\Vault\Projects\ALX Nova\`
After architectural decisions, create an ADR there (see Vault CLAUDE.md for format).
```

With:

```markdown
## Obsidian Vault

Architecture Decision Records, capacity tracking, and project notes in the Obsidian Vault.
Vault path: `C:\Users\Necrosis\Documents\GitHub\Vault\Projects\ALX Nova\`

When completing work:
- **Architectural decision made** → create ADR with `pr:` and `source_files:` fields
- **Feature paused mid-session** → create handoff note in `Projects/ALX Nova/Handoffs/`
- **API removed or replaced** → append to the domain's deprecation-log in `Projects/ALX Nova/Deprecations/`
- **Resource allocated** (event bit, HAL slot, driver) → update matching capacity-budget in `Projects/ALX Nova/Capacity/`

When starting work:
- Check `Handoffs/` for paused work on the same area
- Check capacity-budget notes before allocating event bits, HAL slots, or driver registry entries
```

- [ ] **Step 2: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/ALX_Nova_Controller_2"
git add CLAUDE.md
git commit -m "docs: expand Obsidian Vault integration section with capacity, handoff, and deprecation rules"
```

---

## Task 9: Backfill ADR Cross-Links

**Files:**
- Modify: All 24 ADRs in `Vault/Projects/ALX Nova/ADRs/ADR-*.md`

- [ ] **Step 1: Build PR mapping from Claude Memory and git log**

Read Claude Memory files to map ADRs to PRs:
- ADR-001 through ADR-006: Created during HAL architecture review → no single PR (pre-existing)
- ADR-007 through ADR-024: Created during specific PRs — cross-reference memory entries

Run `git log --oneline --all` in the ALX Nova repo to find PR merge commits and match them to ADR topics.

- [ ] **Step 2: Add `pr:` and `source_files: []` to each ADR's frontmatter**

For each of the 24 ADRs, add the two new fields after `superseded_by:`. If the implementing PR is unknown, leave `pr:` empty. Always add `source_files: []` (can be populated later).

Example edit for ADR-001:
```yaml
---
type: adr
adr_number: 1
project: ALX Nova
status: accepted
superseded_by:
pr:
source_files:
  - src/hal/hal_ess_adc_2ch.h
  - src/hal/hal_ess_dac_2ch.h
created: 2026-03-24
tags: [hal, architecture, drivers]
---
```

- [ ] **Step 3: Commit**

```bash
cd "C:/Users/Necrosis/Documents/GitHub/Vault"
git add "Projects/ALX Nova/ADRs/"
git commit -m "feat: backfill pr and source_files cross-link fields on all 24 ADRs"
```

---

## Task 10: Update MEMORY.md Index Convention

**Files:**
- Modify: `C:\Users\Necrosis\.claude\projects\C--Users-Necrosis-Documents-GitHub-ALX-Nova-Controller-2\memory\MEMORY.md`

- [ ] **Step 1: Add Vault ADR references to relevant completed work entries**

For each MEMORY.md entry that maps to a Vault ADR, append `(Vault: ADR-NNN)`. Examples:

```markdown
- [Phase 3 Platform Expansion](phase3_platform_expansion.md) — PR #87: clock diagnostics, device dependency graph (Vault: ADR-019, ADR-021)
- [HAL Architecture Review](hal_review_2026_03_24.md) — 42 drivers→4 generic classes, ~4500 lines removed (Vault: ADR-001)
- [CSP + server.send Migration](csp_security_headers.md) — PR #90: CSP header, server_send() wrappers (Vault: ADR-014)
```

Only add references where the mapping is clear. Don't force a mapping for entries that don't have a corresponding ADR.

- [ ] **Step 2: Add a "Vault Integration" note to the Reference section**

```markdown
## Reference
- Convention: MEMORY.md entries reference Vault ADRs as `(Vault: ADR-NNN)` when applicable
```

- [ ] **Step 3: No git commit needed** — memory files are not in the git repo.

---

## Verification

- [ ] **Open Obsidian** and navigate to `Projects/ALX Nova/Dashboard.md`. Confirm all 7 Dataview sections render (Active Handoffs will be empty — that's correct).
- [ ] **Check Capacity Budgets section** — should show 4 rows sorted by spare count ascending.
- [ ] **Check Deprecation Logs section** — should show 4 rows (i2s, hal, websocket, rest).
- [ ] **Create a test handoff note** using the Handoff template. Confirm it appears in the Active Handoffs section. Set status to `resumed` — confirm it disappears from the query.
- [ ] **Verify ADR frontmatter** — open any backfilled ADR and confirm `pr:` and `source_files:` fields are present.
- [ ] **Verify Concept template** — create a test concept and confirm `adr:`, `branch:`, `pr:` fields appear in frontmatter.
- [ ] **Read both CLAUDE.md files** — confirm integration rules are clear and actionable.
