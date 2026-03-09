#!/usr/bin/env node
/**
 * detect_doc_changes.js — git-diff-based documentation section detector
 *
 * Compares two git refs, determines which source files changed, and maps
 * those files to documentation sections using tools/doc-mapping.json.
 * Outputs a deduplicated JSON array of section identifiers that need
 * regeneration.
 *
 * Usage:
 *   node tools/detect_doc_changes.js                  # HEAD~1..HEAD
 *   node tools/detect_doc_changes.js <fromRef>         # fromRef..HEAD
 *   node tools/detect_doc_changes.js <fromRef> <toRef> # fromRef..toRef
 *
 * Output (always to stdout, always exits 0):
 *   ["api/rest-main","developer/websocket"]   # sections affected
 *   []                                        # nothing doc-relevant changed
 *
 * The mapping file at tools/doc-mapping.json maps file path patterns to
 * section identifiers.  Simple glob patterns (containing *) are supported
 * via an internal minimatch-compatible implementation — no npm packages
 * required.
 */

'use strict';

const fs            = require('fs');
const path          = require('path');
const childProcess  = require('child_process');

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

const REPO_ROOT     = path.resolve(__dirname, '..');
const MAPPING_FILE  = path.join(__dirname, 'doc-mapping.json');

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

const args    = process.argv.slice(2);
const fromRef = args[0] || 'HEAD~1';
const toRef   = args[1] || 'HEAD';

// ---------------------------------------------------------------------------
// Glob matching
// ---------------------------------------------------------------------------

/**
 * Convert a glob pattern string to a RegExp.
 *
 * Supported metacharacters:
 *   **  — matches any number of path segments (including zero)
 *   *   — matches any sequence of characters that does NOT include `/`
 *   ?   — matches exactly one character that is NOT `/`
 *
 * All other RegExp special characters are escaped.
 *
 * @param {string} glob
 * @returns {RegExp}
 */
function globToRegExp(glob) {
    // Escape all RegExp metacharacters except * and ?
    let pattern = glob.replace(/[.+^${}()|[\]\\]/g, '\\$&');

    // Replace ** before converting * so we can tell them apart.
    // Use a placeholder that won't appear in real paths.
    pattern = pattern.replace(/\*\*/g, '\x00GLOBSTAR\x00');

    // Single * matches anything except /
    pattern = pattern.replace(/\*/g, '[^/]*');

    // ? matches any single character except /
    pattern = pattern.replace(/\?/g, '[^/]');

    // Expand GLOBSTAR placeholder: matches any sequence of path segments
    pattern = pattern.replace(/\x00GLOBSTAR\x00/g, '.*');

    return new RegExp('^' + pattern + '$');
}

/**
 * Test whether a file path matches a mapping key.
 *
 * Exact string match is tried first (fast path).  If the key contains
 * glob metacharacters (* or ?) it is compiled to a RegExp.
 *
 * Paths are compared using forward slashes on all platforms.
 *
 * @param {string} filePath  - Changed file path from git diff (forward-slash separated).
 * @param {string} mappingKey - Key from doc-mapping.json.
 * @returns {boolean}
 */
function fileMatchesKey(filePath, mappingKey) {
    // Normalise both sides to forward slashes for cross-platform safety.
    const normFile = filePath.replace(/\\/g, '/');
    const normKey  = mappingKey.replace(/\\/g, '/');

    if (normFile === normKey) return true;

    // Only compile to regex when globs are present (avoids unnecessary work).
    if (normKey.includes('*') || normKey.includes('?')) {
        return globToRegExp(normKey).test(normFile);
    }

    return false;
}

// ---------------------------------------------------------------------------
// Git operations
// ---------------------------------------------------------------------------

/**
 * Run `git diff --name-only fromRef toRef` in the repository root and return
 * the list of changed file paths.
 *
 * On failure (e.g. invalid ref, not a git repository) logs a warning to
 * stderr and returns an empty array so the script still exits 0.
 *
 * @param {string} from
 * @param {string} to
 * @returns {string[]}
 */
function getChangedFiles(from, to) {
    try {
        const output = childProcess.execSync(
            `git diff --name-only ${from} ${to}`,
            {
                cwd:      REPO_ROOT,
                encoding: 'utf8',
                // Suppress stderr from git itself — we handle errors below.
                stdio:    ['pipe', 'pipe', 'pipe'],
            }
        );
        return output
            .split('\n')
            .map(l => l.trim())
            .filter(l => l.length > 0);
    } catch (err) {
        const msg = err.stderr
            ? err.stderr.toString().trim()
            : err.message;
        process.stderr.write(
            `[detect_doc_changes] WARNING: git diff failed (${from}..${to}): ${msg}\n`
        );
        return [];
    }
}

// ---------------------------------------------------------------------------
// Mapping file loading
// ---------------------------------------------------------------------------

/**
 * Load and validate tools/doc-mapping.json.
 *
 * @returns {{ mappings: Record<string, string[]>, sectionToPromptType: Record<string, string> }}
 */
function loadMapping() {
    if (!fs.existsSync(MAPPING_FILE)) {
        process.stderr.write(
            `[detect_doc_changes] ERROR: mapping file not found: ${MAPPING_FILE}\n`
        );
        process.exit(1);
    }

    let parsed;
    try {
        parsed = JSON.parse(fs.readFileSync(MAPPING_FILE, 'utf8'));
    } catch (err) {
        process.stderr.write(
            `[detect_doc_changes] ERROR: failed to parse doc-mapping.json: ${err.message}\n`
        );
        process.exit(1);
    }

    if (!parsed || typeof parsed.mappings !== 'object') {
        process.stderr.write(
            '[detect_doc_changes] ERROR: doc-mapping.json must have a "mappings" object\n'
        );
        process.exit(1);
    }

    return parsed;
}

// ---------------------------------------------------------------------------
// Section detection
// ---------------------------------------------------------------------------

/**
 * Given a list of changed file paths and the doc-mapping, return a
 * deduplicated, sorted array of documentation section identifiers.
 *
 * @param {string[]}                                changedFiles
 * @param {Record<string, string[]>}                mappings
 * @returns {string[]}
 */
function detectSections(changedFiles, mappings) {
    const sectionSet = new Set();

    for (const filePath of changedFiles) {
        for (const [mappingKey, sections] of Object.entries(mappings)) {
            if (fileMatchesKey(filePath, mappingKey)) {
                for (const section of sections) {
                    sectionSet.add(section);
                }
            }
        }
    }

    // Return sorted for deterministic output — makes diffs and CI logs readable.
    return Array.from(sectionSet).sort();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

(function main() {
    process.stderr.write(
        `[detect_doc_changes] Comparing ${fromRef}..${toRef}\n`
    );

    const { mappings } = loadMapping();

    const changedFiles = getChangedFiles(fromRef, toRef);

    if (changedFiles.length === 0) {
        process.stderr.write(
            '[detect_doc_changes] No changed files detected (empty diff or git error)\n'
        );
        process.stdout.write('[]\n');
        process.exit(0);
    }

    process.stderr.write(
        `[detect_doc_changes] ${changedFiles.length} changed file(s):\n`
    );
    for (const f of changedFiles) {
        process.stderr.write(`  ${f}\n`);
    }

    const sections = detectSections(changedFiles, mappings);

    if (sections.length === 0) {
        process.stderr.write(
            '[detect_doc_changes] No documentation sections affected\n'
        );
    } else {
        process.stderr.write(
            `[detect_doc_changes] ${sections.length} section(s) need regeneration:\n`
        );
        for (const s of sections) {
            process.stderr.write(`  ${s}\n`);
        }
    }

    process.stdout.write(JSON.stringify(sections) + '\n');
    process.exit(0);
}());
