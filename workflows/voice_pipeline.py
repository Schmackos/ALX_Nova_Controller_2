#!/usr/bin/env python3
"""
Voice Pipeline — Convert audio voice memos into structured concept documents.

Flow: Audio files -> Voice notes -> Concept docs (backlog/)

Usage:
    python workflows/voice_pipeline.py
    python workflows/voice_pipeline.py --dry-run
    python workflows/voice_pipeline.py --stage transcribe
    python workflows/voice_pipeline.py --model small --verbose
"""

import argparse
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import date
from pathlib import Path

# Ensure UTF-8 output on Windows
if sys.stdout.encoding != "utf-8":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# --- Paths ---
PROJECT_ROOT = Path(__file__).resolve().parent.parent
BACKLOG_DIR = PROJECT_ROOT / "docs-internal" / "backlog"
INBOX_DIR = BACKLOG_DIR / "inbox"
PROCESSED_DIR = INBOX_DIR / "processed"
VOICE_NOTES_DIR = BACKLOG_DIR / "voice-notes"
RAW_DIR = VOICE_NOTES_DIR / "raw"
MANIFEST_FILE = VOICE_NOTES_DIR / "processed.json"

AUDIO_EXTENSIONS = {".m4a", ".mp3", ".wav", ".ogg", ".flac"}

# --- Concept doc template ---
MAX_RETRIES = 3
MIN_CONCEPT_DOC_LENGTH = 500

REQUIRED_SECTIONS = [
    "# Concept:",
    "## Problem / Opportunity",
    "## Sub-topics",
    "## Action Items",
    "## Original Transcripts",
]


def validate_concept_doc(content, slug):
    """Validate that a concept doc response contains all required sections.

    Returns (is_valid, list_of_errors).
    """
    errors = []

    if not content or len(content.strip()) < MIN_CONCEPT_DOC_LENGTH:
        errors.append(f"Too short ({len(content.strip())} chars, min {MIN_CONCEPT_DOC_LENGTH})")
        return False, errors

    for section in REQUIRED_SECTIONS:
        if section not in content:
            errors.append(f"Missing section: {section}")

    if "- [ ]" not in content:
        errors.append("No action item checkboxes found (expected '- [ ]')")

    if "<details>" not in content:
        errors.append("No collapsed transcript sections found (expected '<details>')")

    return len(errors) == 0, errors


CONCEPT_TEMPLATE = """# Concept: {title}

| Field | Value |
|---|---|
| Workflow | `raw` |
| Priority | `---` |
| Effort | `---` |
| Sources | {sources} |
| Audio | [`inbox/processed/`](inbox/processed/) |
| Last updated | {date} |

## Problem / Opportunity

{problem}

## Sub-topics

{subtopics}

## Action Items

{actions}

## Original Transcripts

{transcripts}
"""


def log(msg, verbose_only=False, verbose=False):
    if verbose_only and not verbose:
        return
    print(msg, flush=True)


def load_manifest():
    if MANIFEST_FILE.exists():
        return json.loads(MANIFEST_FILE.read_text(encoding="utf-8"))
    return {"transcribed": [], "processed": [], "clustered": []}


def save_manifest(manifest):
    MANIFEST_FILE.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def slugify(name):
    name = Path(name).stem
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return slug


def run_claude(prompt, verbose=False):
    """Shell out to Claude Code CLI and return the response text."""
    cmd = ["claude", "-p", "--output-format", "text"]
    log(f"  [Claude] Sending prompt ({len(prompt)} chars)...", verbose_only=True, verbose=verbose)
    try:
        result = subprocess.run(
            cmd,
            input=prompt,
            capture_output=True,
            text=True,
            timeout=300,
            encoding="utf-8",
        )
        if result.returncode != 0:
            log(f"  [Claude] Error: {result.stderr.strip()}")
            return None
        return result.stdout.strip()
    except subprocess.TimeoutExpired:
        log("  [Claude] Timeout after 300s")
        return None
    except FileNotFoundError:
        log("  [Claude] Error: 'claude' CLI not found. Is Claude Code installed?")
        return None


def run_whisper(audio_path, output_dir, model="base", verbose=False):
    """Run Whisper CLI on an audio file."""
    cmd = [
        "whisper",
        str(audio_path),
        "--model", model,
        "--language", "en",
        "--output_format", "json",
        "--output_dir", str(output_dir),
    ]
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    log(f"  [Whisper] Transcribing with model={model}...", verbose_only=True, verbose=verbose)
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600,
            env=env,
            encoding="utf-8",
        )
        if result.returncode != 0:
            log(f"  [Whisper] Error: {result.stderr.strip()}")
            return False
        return True
    except subprocess.TimeoutExpired:
        log("  [Whisper] Timeout after 600s")
        return False
    except FileNotFoundError:
        log("  [Whisper] Error: 'whisper' CLI not found. Run: pip install openai-whisper")
        return False


# --- Stage 1: Transcribe ---
def stage_transcribe(manifest, model="base", dry_run=False, verbose=False):
    """Scan inbox/ for audio files and transcribe with Whisper."""
    log("\n=== Stage 1: Transcribe ===")

    audio_files = [
        f for f in sorted(INBOX_DIR.iterdir())
        if f.is_file() and f.suffix.lower() in AUDIO_EXTENSIONS
    ]

    if not audio_files:
        log("  No audio files in inbox/")
        return manifest

    already = set(manifest.get("transcribed", []))
    new_files = [f for f in audio_files if f.name not in already]

    if not new_files:
        log("  All audio files already transcribed")
        return manifest

    log(f"  Found {len(new_files)} new audio file(s)")

    for i, audio_file in enumerate(new_files, 1):
        log(f"  [{i}/{len(new_files)}] {audio_file.name}")
        if dry_run:
            continue

        if run_whisper(audio_file, RAW_DIR, model=model, verbose=verbose):
            manifest.setdefault("transcribed", []).append(audio_file.name)
            save_manifest(manifest)
        else:
            log(f"  [WARN] Skipping {audio_file.name} due to Whisper error")

    return manifest


# --- Stage 2: Process into voice notes ---
def stage_process(manifest, dry_run=False, verbose=False):
    """Convert raw Whisper JSON transcripts into structured voice notes."""
    log("\n=== Stage 2: Process Voice Notes ===")

    raw_files = sorted(RAW_DIR.glob("*.json"))
    if not raw_files:
        log("  No raw transcripts in voice-notes/raw/")
        return manifest

    already = set(manifest.get("processed", []))
    new_files = [f for f in raw_files if f.name not in already]

    if not new_files:
        log("  All transcripts already processed")
        return manifest

    log(f"  Found {len(new_files)} new transcript(s)")

    for i, raw_file in enumerate(new_files, 1):
        log(f"  [{i}/{len(new_files)}] {raw_file.stem}")
        if dry_run:
            continue

        try:
            data = json.loads(raw_file.read_text(encoding="utf-8"))
            raw_text = data.get("text", "").strip()
        except (json.JSONDecodeError, KeyError):
            log(f"  [WARN] Skipping {raw_file.name} — invalid JSON")
            continue

        if not raw_text:
            log(f"  [WARN] Skipping {raw_file.name} — empty transcript")
            continue

        prompt = f"""Process this raw Whisper transcript into a structured voice note.

Produce exactly two sections:

## Summary
- 3-7 bullet points capturing the key ideas and decisions

## Cleaned Transcript
The full transcript with:
- Transcription errors fixed (especially technical terms, product names, acronyms)
- Filler words removed (um, uh, like, you know, basically)
- Proper punctuation and paragraphs added
- Original intent and wording preserved as much as possible

Context: This is a voice memo about the ALX Nova audio platform project. Common terms include: mezzanine, carrier board, HAL, DAC, ADC, DSP, ESP32-P4, I2S, I2C, EEPROM, PlatformIO, ESS, Cirrus Logic, PCM, DSD, MQTT, Home Assistant.

Raw transcript:
{raw_text}"""

        response = run_claude(prompt, verbose=verbose)
        if response is None:
            log(f"  [WARN] Skipping {raw_file.stem} — Claude failed")
            continue

        slug = slugify(raw_file.name)
        note_file = VOICE_NOTES_DIR / f"{slug}.md"
        note_file.write_text(
            f"# Voice Note: {raw_file.stem}\n\n{response}\n",
            encoding="utf-8",
        )

        manifest.setdefault("processed", []).append(raw_file.name)
        save_manifest(manifest)
        log(f"  -> {note_file.name}")

    return manifest


# --- Stage 3: Cluster ---
def stage_cluster(manifest, dry_run=False, verbose=False):
    """Group voice notes into concept doc clusters."""
    log("\n=== Stage 3: Cluster ===")

    note_files = sorted(VOICE_NOTES_DIR.glob("*.md"))
    if not note_files:
        log("  No voice notes to cluster")
        return manifest

    already = set(manifest.get("clustered", []))
    new_notes = [f for f in note_files if f.name not in already]

    if not new_notes:
        log("  All voice notes already clustered")
        return manifest

    log(f"  Found {len(new_notes)} new voice note(s) to cluster")
    if dry_run:
        return manifest

    # Gather voice note summaries
    note_summaries = []
    for note_file in new_notes:
        content = note_file.read_text(encoding="utf-8")
        # Extract just the Summary section for clustering
        summary_match = re.search(r"## Summary\n(.*?)(?=\n## |\Z)", content, re.DOTALL)
        summary = summary_match.group(1).strip() if summary_match else content[:500]
        note_summaries.append({
            "filename": note_file.name,
            "summary": summary,
        })

    # Gather existing concept docs
    existing_concepts = []
    for concept_file in sorted(BACKLOG_DIR.glob("concept-*.md")):
        content = concept_file.read_text(encoding="utf-8")
        prob_match = re.search(r"## Problem / Opportunity\n(.*?)(?=\n## |\Z)", content, re.DOTALL)
        problem = prob_match.group(1).strip() if prob_match else ""
        existing_concepts.append({
            "filename": concept_file.name,
            "problem": problem,
        })

    existing_info = ""
    if existing_concepts:
        existing_info = "\n\nExisting concept docs:\n" + json.dumps(existing_concepts, indent=2)

    prompt = f"""You are organizing voice notes about the ALX Nova audio platform project into concept document clusters.

Here are the new voice notes to cluster:
{json.dumps(note_summaries, indent=2)}
{existing_info}

Group related voice notes together. For each group, decide:
1. If it fits an existing concept doc, assign it to that filename
2. If it's a new topic, create a new concept slug

Respond with ONLY valid JSON (no markdown, no explanation):
[
  {{
    "note": "voice-note-filename.md",
    "target": "existing" or "new",
    "concept_slug": "concept-slug-name",
    "concept_title": "Human Readable Title",
    "subtopic": "Specific sub-topic name within the concept"
  }}
]

Rules:
- Group related topics (e.g., all community-related notes under one concept)
- Use descriptive slugs (e.g., "community-platform", "hardware-dsp", "monetization")
- Each note gets exactly one assignment
- Prefer fewer, broader concept docs over many narrow ones"""

    response = run_claude(prompt, verbose=verbose)
    if response is None:
        log("  [WARN] Clustering failed — Claude error")
        return manifest

    # Parse JSON from response (handle markdown code blocks)
    json_text = response
    json_match = re.search(r"```(?:json)?\s*\n?(.*?)\n?```", response, re.DOTALL)
    if json_match:
        json_text = json_match.group(1)

    try:
        clusters = json.loads(json_text)
    except json.JSONDecodeError:
        log(f"  [WARN] Could not parse clustering response as JSON")
        log(f"  Response: {response[:200]}...")
        return manifest

    # Save clustering result
    cluster_file = VOICE_NOTES_DIR / "clusters.json"
    cluster_file.write_text(
        json.dumps(clusters, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    for note in new_notes:
        manifest.setdefault("clustered", []).append(note.name)
    save_manifest(manifest)

    log(f"  Clustered {len(new_notes)} notes into groups")
    # Show grouping
    groups = {}
    for c in clusters:
        slug = c.get("concept_slug", "unknown")
        groups.setdefault(slug, []).append(c.get("note", "?"))
    for slug, notes in groups.items():
        log(f"    {slug}: {len(notes)} note(s)")

    return manifest


# --- Stage 4: Generate concept docs ---
def stage_generate(manifest, dry_run=False, verbose=False):
    """Generate or update concept docs from clustered voice notes."""
    log("\n=== Stage 4: Generate Concept Docs ===")

    cluster_file = VOICE_NOTES_DIR / "clusters.json"
    if not cluster_file.exists():
        log("  No clusters.json — run cluster stage first")
        return manifest

    clusters = json.loads(cluster_file.read_text(encoding="utf-8"))
    if not clusters:
        log("  No clusters to generate")
        return manifest

    if dry_run:
        groups = {}
        for c in clusters:
            groups.setdefault(c.get("concept_slug", "?"), []).append(c.get("note", "?"))
        for slug, notes in groups.items():
            log(f"  Would generate: concept-{slug}.md ({len(notes)} notes)")
        return manifest

    # Group clusters by concept
    groups = {}
    for c in clusters:
        slug = c.get("concept_slug", "unknown")
        groups.setdefault(slug, {
            "title": c.get("concept_title", slug),
            "notes": [],
        })
        groups[slug]["notes"].append(c)

    today = date.today().isoformat()

    for slug, group in groups.items():
        concept_file = BACKLOG_DIR / f"concept-{slug}.md"
        is_update = concept_file.exists()

        # Guard: skip docs that have been manually edited (workflow != raw)
        if is_update:
            existing = concept_file.read_text(encoding="utf-8")
            wf_match = re.search(r"\|\s*Workflow\s*\|\s*`(\w[\w-]*)`", existing)
            if wf_match and wf_match.group(1) != "raw":
                log(f"  [Skipped] concept-{slug}.md — workflow is `{wf_match.group(1)}`, not `raw`")
                continue

        action = "Updating" if is_update else "Creating"
        log(f"  [{action}] concept-{slug}.md ({len(group['notes'])} notes)")

        # Gather voice note contents
        note_contents = []
        source_files = []
        for note_entry in group["notes"]:
            note_file = VOICE_NOTES_DIR / note_entry["note"]
            if note_file.exists():
                note_contents.append({
                    "subtopic": note_entry.get("subtopic", "General"),
                    "content": note_file.read_text(encoding="utf-8"),
                })
            # Find matching raw transcript for original text
            raw_stem = note_file.stem
            raw_file = RAW_DIR / f"{raw_stem}.json"
            if not raw_file.exists():
                # Try finding by similar name
                for rf in RAW_DIR.glob("*.json"):
                    if slugify(rf.name) == raw_stem:
                        raw_file = rf
                        break
            if raw_file.exists():
                try:
                    raw_data = json.loads(raw_file.read_text(encoding="utf-8"))
                    source_files.append({
                        "filename": raw_file.stem + ".m4a",
                        "text": raw_data.get("text", "").strip(),
                    })
                except json.JSONDecodeError:
                    pass

        existing_content = ""
        if is_update:
            existing_content = f"\n\nExisting concept doc to update:\n{concept_file.read_text(encoding='utf-8')}"

        prompt = f"""Generate a concept document for the ALX Nova project using this exact template structure.

Title: {group['title']}
Date: {today}

Voice notes to incorporate:
{json.dumps(note_contents, indent=2, ensure_ascii=False)}

Source audio files for the Original Transcripts section:
{json.dumps(source_files, indent=2, ensure_ascii=False)}
{existing_content}

Generate a markdown document with EXACTLY this structure:

# Concept: {group['title']}

| Field | Value |
|---|---|
| Workflow | `raw` |
| Priority | `---` |
| Effort | `---` |
| Sources | <comma-separated list of source .m4a filenames> |
| Audio | `voice-notes/` |
| Last updated | {today} |

## Problem / Opportunity

<Why this matters for ALX Nova. 2-4 sentences based on the voice notes.>

## Sub-topics

<For each sub-topic from the voice notes, create a section:>

### <Sub-topic name>

#### What we know
- <concrete facts, decisions, or constraints mentioned in the notes>

#### What needs research
- <open questions, unknowns, things that need investigation>

## Action Items

- [ ] <Each action item must be a complete, self-contained instruction that can be handed directly to Claude in a future session. Include enough context that Claude can execute it without additional information. Example: "Research the Pascal amplifier interface protocol. Document the communication protocol, required hardware components, and integration effort for the ALX Nova mezzanine slot. Write findings to docs-internal/backlog/research/pascal-interface.md">

## Original Transcripts

<For each source audio file, create a collapsed section:>

<details>
<summary>Source: <filename></summary>

> <full original transcript text, blockquoted>

</details>

Rules:
- Do NOT add any sections not in the template
- Action items MUST be Claude-promptable (complete instructions, not vague tasks)
- If updating an existing doc, merge new sub-topics and append new transcripts — do not duplicate existing content
- Keep Problem/Opportunity to 2-4 sentences
- Output ONLY the markdown document, no explanations"""

        response = None
        for attempt in range(1, MAX_RETRIES + 1):
            retry_hint = ""
            if attempt > 1:
                retry_hint = (
                    "\n\nCRITICAL: Your previous response failed validation. "
                    "You MUST output ONLY the raw markdown document starting with '# Concept:'. "
                    "Do NOT include any preamble, summary, explanation, or commentary. "
                    "Do NOT say 'here is the document' or similar — just output the markdown."
                )

            raw = run_claude(prompt + retry_hint, verbose=verbose)
            if raw is None:
                log(f"  [Attempt {attempt}/{MAX_RETRIES}] Claude call failed")
                continue

            # Strip any preamble before the actual markdown
            idx = raw.find("# Concept:")
            if idx > 0:
                raw = raw[idx:]

            is_valid, errors = validate_concept_doc(raw, slug)
            if is_valid:
                response = raw
                if attempt > 1:
                    log(f"  [Attempt {attempt}/{MAX_RETRIES}] Passed validation")
                break
            else:
                log(f"  [Attempt {attempt}/{MAX_RETRIES}] Validation failed:")
                for err in errors:
                    log(f"    - {err}")

        if response is None:
            log(f"  [FAIL] Skipping concept-{slug}.md after {MAX_RETRIES} attempts")
            continue

        # Atomic write: temp file then replace
        tmp = tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".md",
            dir=str(BACKLOG_DIR),
            delete=False,
            encoding="utf-8",
        )
        try:
            tmp.write(response)
            tmp.close()
            shutil.move(tmp.name, str(concept_file))
            log(f"  -> concept-{slug}.md")
        except Exception as e:
            log(f"  [WARN] Failed to write concept-{slug}.md: {e}")
            if os.path.exists(tmp.name):
                os.unlink(tmp.name)

    return manifest


# --- Stage 5: Archive ---
def stage_archive(manifest, dry_run=False, verbose=False):
    """Move processed audio files to inbox/processed/."""
    log("\n=== Stage 5: Archive ===")

    audio_files = [
        f for f in sorted(INBOX_DIR.iterdir())
        if f.is_file() and f.suffix.lower() in AUDIO_EXTENSIONS
    ]

    if not audio_files:
        log("  No audio files to archive")
        return manifest

    transcribed = set(manifest.get("transcribed", []))
    to_archive = [f for f in audio_files if f.name in transcribed]

    if not to_archive:
        log("  No fully processed audio files to archive")
        return manifest

    log(f"  Archiving {len(to_archive)} file(s)")

    for f in to_archive:
        log(f"  -> processed/{f.name}", verbose_only=True, verbose=verbose)
        if not dry_run:
            shutil.move(str(f), str(PROCESSED_DIR / f.name))

    return manifest


# --- Shared concept listing ---
WORKFLOW_ORDER = ["raw", "draft", "ready", "in-progress", "done", "archived"]
ARCHIVE_DIR = BACKLOG_DIR / "archive"


def list_concepts():
    """List concept docs with workflow status and action counts. Returns entries list."""
    concept_files = sorted(BACKLOG_DIR.glob("concept-*.md"))
    if not concept_files:
        log("No concept docs found in docs-internal/backlog/")
        return []

    entries = []
    for f in concept_files:
        content = f.read_text(encoding="utf-8")

        workflow_match = re.search(r"\|\s*Workflow\s*\|\s*`(\w[\w-]*)`", content)
        workflow = workflow_match.group(1) if workflow_match else "raw"

        actions = len(re.findall(r"^- \[ \]", content, re.MULTILINE))

        title_match = re.search(r"^# Concept:\s*(.+)", content, re.MULTILINE)
        title = title_match.group(1).strip() if title_match else f.stem

        entries.append({
            "file": f,
            "workflow": workflow,
            "actions": actions,
            "title": title,
        })

    log("\n=== Concept Backlog ===\n")
    log(f"  {'#':>3}  {'Workflow':<12} {'Actions':>7}  Concept")
    for i, e in enumerate(entries, 1):
        log(f"  {i:>3}  {e['workflow']:<12} {e['actions']:>7}  {e['title']}")

    return entries


def pick_concept(entries, prompt_text="Pick a concept"):
    """Prompt user to pick a concept by number. Returns entry or None."""
    log("")
    try:
        choice = input(f"{prompt_text} (1-{len(entries)}): ").strip()
    except (EOFError, KeyboardInterrupt):
        log("\nCancelled.")
        return None

    if not choice.isdigit() or not (1 <= int(choice) <= len(entries)):
        log(f"Invalid choice: {choice}")
        return None

    return entries[int(choice) - 1]


# --- Pick command ---
def command_pick():
    """List concept docs and let user pick one for brainstorming."""
    entries = list_concepts()
    if not entries:
        return

    picked = pick_concept(entries)
    if not picked:
        return

    content = picked["file"].read_text(encoding="utf-8")
    rel_path = picked["file"].relative_to(PROJECT_ROOT).as_posix()

    log(f"\n{'=' * 60}")
    log(content)
    log(f"{'=' * 60}")
    log(f"\nTo work on this, tell Claude:")
    log(f'  "Read {rel_path} and brainstorm it into a PRD"')


# --- Promote command ---
def command_promote():
    """Advance a concept doc to the next workflow stage."""
    entries = list_concepts()
    if not entries:
        return

    picked = pick_concept(entries, "Promote which concept")
    if not picked:
        return

    current = picked["workflow"]
    if current not in WORKFLOW_ORDER:
        log(f"Unknown workflow stage: {current}")
        return

    idx = WORKFLOW_ORDER.index(current)
    if idx >= len(WORKFLOW_ORDER) - 1:
        log(f"  Already at final stage: `{current}`")
        return

    next_stage = WORKFLOW_ORDER[idx + 1]
    content = picked["file"].read_text(encoding="utf-8")
    updated = re.sub(
        r"(\|\s*Workflow\s*\|\s*)`" + re.escape(current) + r"`",
        r"\g<1>`" + next_stage + "`",
        content,
    )

    if next_stage == "archived":
        ARCHIVE_DIR.mkdir(parents=True, exist_ok=True)
        dest = ARCHIVE_DIR / picked["file"].name
        picked["file"].write_text(updated, encoding="utf-8")
        shutil.move(str(picked["file"]), str(dest))
        log(f"\n  {picked['title']}: `{current}` -> `{next_stage}` (moved to archive/)")
    else:
        picked["file"].write_text(updated, encoding="utf-8")
        log(f"\n  {picked['title']}: `{current}` -> `{next_stage}`")


# --- Main ---
STAGES = {
    "transcribe": stage_transcribe,
    "process": stage_process,
    "cluster": stage_cluster,
    "generate": stage_generate,
    "archive": stage_archive,
}


def main():
    parser = argparse.ArgumentParser(
        description="Voice Pipeline — Convert audio memos into structured concept docs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python workflows/voice_pipeline.py                    # Run full pipeline
  python workflows/voice_pipeline.py --dry-run          # Preview without changes
  python workflows/voice_pipeline.py --stage transcribe # Run one stage
  python workflows/voice_pipeline.py --model small      # Use larger Whisper model
  python workflows/voice_pipeline.py --pick             # Browse and pick a concept
  python workflows/voice_pipeline.py --promote          # Advance a concept's workflow stage
  python workflows/voice_pipeline.py --verbose          # Detailed output
        """,
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be processed without making changes",
    )
    parser.add_argument(
        "--stage",
        choices=list(STAGES.keys()),
        help="Run only a specific stage",
    )
    parser.add_argument(
        "--model",
        default="base",
        choices=["tiny", "base", "small", "medium", "large"],
        help="Whisper model to use (default: base)",
    )
    parser.add_argument(
        "--pick",
        action="store_true",
        help="Browse concept docs and pick one for brainstorming",
    )
    parser.add_argument(
        "--promote",
        action="store_true",
        help="Advance a concept doc to the next workflow stage",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable detailed logging",
    )

    args = parser.parse_args()

    if args.pick:
        command_pick()
        return

    if args.promote:
        command_promote()
        return

    if args.dry_run:
        log("[DRY RUN] No changes will be made\n")

    # Ensure directories exist
    for d in [INBOX_DIR, PROCESSED_DIR, VOICE_NOTES_DIR, RAW_DIR]:
        d.mkdir(parents=True, exist_ok=True)

    manifest = load_manifest()

    if args.stage:
        stage_fn = STAGES[args.stage]
        kwargs = {"manifest": manifest, "dry_run": args.dry_run, "verbose": args.verbose}
        if args.stage == "transcribe":
            kwargs["model"] = args.model
        manifest = stage_fn(**kwargs)
    else:
        manifest = stage_transcribe(manifest, model=args.model, dry_run=args.dry_run, verbose=args.verbose)
        manifest = stage_process(manifest, dry_run=args.dry_run, verbose=args.verbose)
        manifest = stage_cluster(manifest, dry_run=args.dry_run, verbose=args.verbose)
        manifest = stage_generate(manifest, dry_run=args.dry_run, verbose=args.verbose)
        manifest = stage_archive(manifest, dry_run=args.dry_run, verbose=args.verbose)

    log("\n=== Done ===")


if __name__ == "__main__":
    main()
