#!/usr/bin/env node
/**
 * extract_api.js — C++ REST endpoint and WebSocket message extractor
 *
 * Parses ALX Nova Controller firmware source files to produce a structured
 * inventory of all REST API endpoints and WebSocket message types. Output is
 * consumed by generate_docs.js for automated documentation generation.
 *
 * No external dependencies — uses only Node.js built-in modules (fs, path).
 *
 * Usage (standalone):
 *   node tools/extract_api.js
 *   node tools/extract_api.js --pretty   # pretty-print JSON
 *
 * Programmatic import:
 *   const { extractAll } = require('./tools/extract_api');
 *   const api = extractAll();
 */

'use strict';

const fs   = require('fs');
const path = require('path');

// ---------------------------------------------------------------------------
// Path resolution — all paths relative to the repository root, which is the
// parent directory of the tools/ folder this script lives in.
// ---------------------------------------------------------------------------
const REPO_ROOT = path.resolve(__dirname, '..');

/** Resolve a repo-relative path to an absolute path. */
function repoPath(rel) {
    return path.join(REPO_ROOT, rel);
}

// ---------------------------------------------------------------------------
// Source files to scan
// ---------------------------------------------------------------------------

/** Files that contain `server.on(...)` REST registrations. */
const ENDPOINT_SOURCE_FILES = [
    'src/main.cpp',
    'src/hal/hal_api.cpp',
    'src/dsp_api.cpp',
    'src/pipeline_api.cpp',
    'src/dac_api.cpp',
];

/** Files scanned for WebSocket command/broadcast patterns. */
const WS_SOURCE_FILES = [
    'src/websocket_handler.h',
    'src/websocket_handler.cpp',
];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Read a source file and return its lines as an array.
 * Returns an empty array (with a warning) if the file does not exist.
 *
 * @param {string} relPath - Repo-relative path.
 * @returns {{ lines: string[], exists: boolean }}
 */
function readLines(relPath) {
    const abs = repoPath(relPath);
    if (!fs.existsSync(abs)) {
        process.stderr.write(`[extract_api] WARNING: file not found: ${relPath}\n`);
        return { lines: [], exists: false };
    }
    return { lines: fs.readFileSync(abs, 'utf8').split('\n'), exists: true };
}

/**
 * Scan backward from a given line index looking for a single-line comment
 * (`// ...`) that describes the endpoint.  Skips blank lines and stops as
 * soon as it hits a non-comment, non-blank line or travels more than
 * MAX_LOOK lines.
 *
 * @param {string[]} lines
 * @param {number}   startIdx - The line index of the `server.on(` call.
 * @returns {string} Trimmed comment text (without `//`), or empty string.
 */
function extractNearbyComment(lines, startIdx) {
    const MAX_LOOK = 4;
    for (let i = startIdx - 1; i >= Math.max(0, startIdx - MAX_LOOK); i--) {
        const trimmed = lines[i].trim();
        if (trimmed === '') continue;
        if (trimmed.startsWith('//')) {
            // Strip leading slashes, dashes, and whitespace.
            return trimmed.replace(/^\/\/\s*[-=]*\s*/, '').trim();
        }
        // Non-comment, non-blank line — stop looking.
        break;
    }
    return '';
}

// ---------------------------------------------------------------------------
// REST endpoint extraction
// ---------------------------------------------------------------------------

/**
 * HTTP method token to canonical string.
 * The firmware uses Arduino WebServer constants like HTTP_GET, HTTP_POST, etc.
 */
const METHOD_MAP = {
    HTTP_GET:    'GET',
    HTTP_POST:   'POST',
    HTTP_PUT:    'PUT',
    HTTP_DELETE: 'DELETE',
    HTTP_PATCH:  'PATCH',
    HTTP_OPTIONS: 'OPTIONS',
};

/**
 * Parse all `server.on(...)` registrations from a C++ source file.
 *
 * Handles two layout styles found in this codebase:
 *
 *   Style A — single line:
 *     server.on("/api/path", HTTP_GET, handler);
 *
 *   Style B — path on first line, method on second:
 *     server.on(
 *         "/api/firmware/upload", HTTP_POST,
 *         ...
 *     );
 *
 * @param {string} relPath
 * @returns {Array<{method: string, path: string, source: string, line: number, description: string}>}
 */
function extractEndpointsFromFile(relPath) {
    const { lines, exists } = readLines(relPath);
    if (!exists) return [];

    const results = [];

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];

        // Fast pre-check: must contain server.on(
        if (!line.includes('server.on(')) continue;

        // Build a window of up to 4 lines to handle multi-line registrations.
        const window = lines.slice(i, i + 4).join(' ');

        // Match: server.on( "path", HTTP_METHOD ...
        // The path may be on the same line or the next line after the opening paren.
        const match = window.match(
            /server\.on\s*\(\s*"([^"]+)"\s*,\s*(HTTP_GET|HTTP_POST|HTTP_PUT|HTTP_DELETE|HTTP_PATCH|HTTP_OPTIONS)/
        );

        if (!match) continue;

        const urlPath = match[1];
        const method  = METHOD_MAP[match[2]] || match[2];

        // Skip noise routes (browser auto-requests, captive portal probes).
        const NOISE_PATHS = new Set([
            '/favicon.ico',
            '/manifest.json',
            '/robots.txt',
            '/sitemap.xml',
            '/apple-touch-icon.png',
            '/apple-touch-icon-precomposed.png',
            '/generate_204',
            '/hotspot-detect.html',
        ]);
        if (NOISE_PATHS.has(urlPath)) continue;

        const description = extractNearbyComment(lines, i);

        results.push({
            method,
            path: urlPath,
            source: relPath,
            line: i + 1,
            description,
        });
    }

    return results;
}

/**
 * Extract REST endpoints from all registered source files.
 *
 * @returns {Array<{method: string, path: string, source: string, line: number, description: string}>}
 */
function extractEndpoints() {
    const all = [];
    for (const relPath of ENDPOINT_SOURCE_FILES) {
        const entries = extractEndpointsFromFile(relPath);
        all.push(...entries);
        process.stderr.write(
            `[extract_api] ${relPath}: ${entries.length} endpoint(s) found\n`
        );
    }
    return all;
}

// ---------------------------------------------------------------------------
// WebSocket extraction
// ---------------------------------------------------------------------------

/**
 * Parse inbound WebSocket command types from C++ source.
 *
 * Matches patterns:
 *   if (msgType == "commandName")
 *   } else if (msgType == "commandName")
 *   if (doc["type"] == "commandName")
 *   else if (doc["type"] == "commandName")
 *
 * The "auth" type is intentionally included — it is a real protocol message.
 *
 * @param {string} relPath
 * @returns {Array<{type: string, source: string, line: number}>}
 */
function extractWsCommandsFromFile(relPath) {
    const { lines, exists } = readLines(relPath);
    if (!exists) return [];

    const results = [];

    // Matches both `msgType == "X"` and `doc["type"] == "X"` forms.
    const CMD_RE = /(?:if|else\s+if)\s*\(\s*(?:msgType|doc\["type"\])\s*==\s*"([^"]+)"/;

    for (let i = 0; i < lines.length; i++) {
        const m = lines[i].match(CMD_RE);
        if (!m) continue;
        results.push({ type: m[1], source: relPath, line: i + 1 });
    }

    return results;
}

/**
 * Parse outbound WebSocket broadcast types from C++ source.
 *
 * Matches:
 *   doc["type"] = "broadcastName";
 *
 * @param {string} relPath
 * @returns {Array<{type: string, source: string, line: number}>}
 */
function extractWsBroadcastsFromFile(relPath) {
    const { lines, exists } = readLines(relPath);
    if (!exists) return [];

    const results = [];

    // Match assignment form: doc["type"] = "value";
    const BCAST_RE = /doc\["type"\]\s*=\s*"([^"]+)"/;

    for (let i = 0; i < lines.length; i++) {
        const m = lines[i].match(BCAST_RE);
        if (!m) continue;
        results.push({ type: m[1], source: relPath, line: i + 1 });
    }

    return results;
}

/**
 * Parse binary WebSocket frame type constants from a header file.
 *
 * Matches:
 *   #define WS_BIN_NAME 0xNN
 *
 * @param {string} relPath
 * @returns {Array<{name: string, value: string, source: string, line: number}>}
 */
function extractWsBinaryTypesFromFile(relPath) {
    const { lines, exists } = readLines(relPath);
    if (!exists) return [];

    const results = [];

    const BIN_RE = /^#define\s+(WS_BIN_\w+)\s+(0x[0-9A-Fa-f]+|\d+)/;

    for (let i = 0; i < lines.length; i++) {
        const m = lines[i].match(BIN_RE);
        if (!m) continue;
        // Normalise numeric value to lowercase hex with 0x prefix.
        const raw = m[2];
        const value = raw.startsWith('0x') || raw.startsWith('0X')
            ? '0x' + parseInt(raw, 16).toString(16).padStart(2, '0')
            : '0x' + parseInt(raw, 10).toString(16).padStart(2, '0');
        results.push({ name: m[1], value, source: relPath, line: i + 1 });
    }

    return results;
}

/**
 * Deduplicate an array of objects by a key field, keeping the first occurrence.
 *
 * @template T
 * @param {T[]}    arr
 * @param {string} key
 * @returns {T[]}
 */
function deduplicateByKey(arr, key) {
    const seen = new Set();
    return arr.filter(item => {
        const val = item[key];
        if (seen.has(val)) return false;
        seen.add(val);
        return true;
    });
}

/**
 * Extract all WebSocket types (commands, broadcasts, binary constants).
 *
 * Commands and broadcasts are deduplicated by type name — the same logical
 * type string sometimes appears in both .h and .cpp; we keep the first hit.
 *
 * @returns {{
 *   wsCommands:    Array<{type: string, source: string, line: number}>,
 *   wsBroadcasts:  Array<{type: string, source: string, line: number}>,
 *   wsBinaryTypes: Array<{name: string, value: string, source: string, line: number}>,
 * }}
 */
function extractWebSocket() {
    const commands    = [];
    const broadcasts  = [];
    const binaryTypes = [];

    for (const relPath of WS_SOURCE_FILES) {
        const cmds = extractWsCommandsFromFile(relPath);
        commands.push(...cmds);
        process.stderr.write(
            `[extract_api] ${relPath}: ${cmds.length} WS command(s) found\n`
        );

        const bcasts = extractWsBroadcastsFromFile(relPath);
        broadcasts.push(...bcasts);
        process.stderr.write(
            `[extract_api] ${relPath}: ${bcasts.length} WS broadcast(s) found\n`
        );

        const bins = extractWsBinaryTypesFromFile(relPath);
        binaryTypes.push(...bins);
        if (bins.length > 0) {
            process.stderr.write(
                `[extract_api] ${relPath}: ${bins.length} binary type constant(s) found\n`
            );
        }
    }

    return {
        wsCommands:    deduplicateByKey(commands,   'type'),
        wsBroadcasts:  deduplicateByKey(broadcasts, 'type'),
        wsBinaryTypes: deduplicateByKey(binaryTypes, 'name'),
    };
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Extract all API information from firmware source files.
 *
 * @returns {{
 *   endpoints:     Array<{method: string, path: string, source: string, line: number, description: string}>,
 *   wsCommands:    Array<{type: string, source: string, line: number}>,
 *   wsBroadcasts:  Array<{type: string, source: string, line: number}>,
 *   wsBinaryTypes: Array<{name: string, value: string, source: string, line: number}>,
 * }}
 */
function extractAll() {
    const endpoints = extractEndpoints();
    const { wsCommands, wsBroadcasts, wsBinaryTypes } = extractWebSocket();

    return { endpoints, wsCommands, wsBroadcasts, wsBinaryTypes };
}

// ---------------------------------------------------------------------------
// Standalone entry point
// ---------------------------------------------------------------------------

if (require.main === module) {
    const PRETTY = process.argv.includes('--pretty');

    process.stderr.write('[extract_api] Scanning firmware sources...\n');

    const result = extractAll();

    process.stderr.write(
        `[extract_api] Done. ` +
        `${result.endpoints.length} endpoints, ` +
        `${result.wsCommands.length} WS commands, ` +
        `${result.wsBroadcasts.length} WS broadcasts, ` +
        `${result.wsBinaryTypes.length} binary types\n`
    );

    const json = PRETTY
        ? JSON.stringify(result, null, 2)
        : JSON.stringify(result);

    process.stdout.write(json + '\n');
}

module.exports = { extractAll };
