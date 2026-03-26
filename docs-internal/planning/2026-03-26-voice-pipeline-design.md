# Voice Pipeline Design

**Date:** 2026-03-26
**Status:** Approved

## Overview

A repeatable automation pipeline that converts audio voice memos into structured concept documents for the ALX Nova project backlog.

## Pipeline Flow

```
Audio → Voice notes → Concept docs → Amend/edit → [ready] → Brainstorm → PRD → Plan → Execute
         (Whisper)     (pipeline)    (pipeline +     ↑        (Claude)   (Claude) (Claude)
                                      manual)     --pick
```

1. **Transcribe** — Whisper CLI converts audio to raw JSON transcripts
2. **Process** — Claude Code CLI produces voice notes (summary + cleaned text)
3. **Cluster** — Claude Code CLI groups related notes, maps to existing or new concept docs
4. **Generate** — Claude Code CLI creates/updates concept docs using the standard template
5. **Archive** — Processed audio files move to `inbox/processed/`

## Directory Structure

```
workflows/
├── voice_pipeline.py         # Main pipeline script
└── README.md                 # Usage docs, arguments, examples

docs-internal/backlog/
├── inbox/                    # Drop audio files here
│   └── processed/            # Archived after processing
├── voice-notes/              # Intermediate processed notes
│   ├── raw/                  # Whisper JSON output
│   └── processed.json        # Manifest tracking processed files
└── concept-*.md              # Final concept docs
```

## Concept Doc Template

Each concept doc follows this structure:

```markdown
# Concept: <Topic Name>

| Field | Value |
|---|---|
| Workflow | `raw` / `draft` / `ready` / `in-progress` / `done` / `archived` |
| Priority | `---` (unset until decided) |
| Effort | `---` (unset until researched) |
| Sources | <list of source audio filenames> |
| Last updated | YYYY-MM-DD |

## Problem / Opportunity

Why this matters for ALX Nova. 2-4 sentences.

## Sub-topics

### <Sub-topic name>

#### What we know
- Facts, decisions, constraints from transcript

#### What needs research
- Open questions, unknowns, dependencies

## Action Items

- [ ] <Claude-promptable instruction>

## Original Transcripts

<details>
<summary>Source: <filename>.m4a</summary>

> Full transcript text

</details>
```

## Design Decisions

- **Merged concept docs**: Related transcripts are grouped into single concept docs with sub-topic sections (~8-10 docs from 24 transcripts)
- **Claude Code CLI**: Pipeline shells out to `claude -p` rather than using the Anthropic API directly — no extra API key needed
- **Whisper base model**: Good accuracy/speed balance for voice memos. Override with `--model` flag
- **Idempotent**: `processed.json` manifest prevents reprocessing. Safe to re-run
- **Auto-append**: New audio files that relate to an existing concept doc are appended as new sub-topics
- **Atomic writes**: Concept docs are written to temp files first, then replaced

## CLI Arguments

| Argument | Description | Default |
|---|---|---|
| `--dry-run` | Show what would be processed without doing it | off |
| `--stage <name>` | Run only a specific stage (transcribe/process/cluster/generate) | all |
| `--model <name>` | Whisper model (tiny/base/small/medium/large) | base |
| `--verbose` | Detailed logging | off |

## Action Item Format

All action items are written as Claude-promptable instructions, e.g.:
> "Research the Pascal amplifier interface protocol. Document the communication protocol, hardware requirements, and integration effort for the ALX Nova mezzanine slot. Write findings to docs-internal/backlog/research/pascal-interface.md"

This allows any action item to be handed directly to Claude in a future session.

## Edge Cases

- **Clustering ambiguity**: If Claude is unsure about a match, it creates a new concept doc (split over wrong merge)
- **Whisper failure**: File is skipped with a warning, left in inbox for retry
- **Claude CLI failure**: Already-processed voice notes are preserved; re-run picks up where it left off
- **No partial writes**: Temp file + replace pattern prevents corrupt concept docs
