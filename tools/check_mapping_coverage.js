#!/usr/bin/env node
/**
 * check_mapping_coverage.js
 *
 * Validates that every .h and .cpp file under src/ is accounted for in
 * tools/doc-mapping.json — either mapped to a doc section or explicitly
 * excluded. Used as a CI quality gate and pre-commit hook.
 *
 * Usage:
 *   node tools/check_mapping_coverage.js
 *   node tools/check_mapping_coverage.js --verbose
 *   node tools/check_mapping_coverage.js --stats
 *   node tools/check_mapping_coverage.js --verbose --stats
 *
 * Exit codes:
 *   0 — all source files are covered
 *   1 — one or more files are uncovered (or doc-mapping.json missing)
 */

'use strict';

const fs   = require('fs');
const path = require('path');

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------
const args    = process.argv.slice(2);
const verbose = args.includes('--verbose');
const stats   = args.includes('--stats');

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------
const repoRoot      = path.join(__dirname, '..');
const mappingPath   = path.join(__dirname, 'doc-mapping.json');
const srcDir        = path.join(repoRoot, 'src');

// ---------------------------------------------------------------------------
// Load doc-mapping.json
// ---------------------------------------------------------------------------
if (!fs.existsSync(mappingPath)) {
  console.error('✗ tools/doc-mapping.json not found.');
  console.error('  Create the file before running this check.');
  process.exit(1);
}

let mapping;
try {
  mapping = JSON.parse(fs.readFileSync(mappingPath, 'utf8'));
} catch (err) {
  console.error('✗ Failed to parse tools/doc-mapping.json:', err.message);
  process.exit(1);
}

const mappings = mapping.mappings || {};
const excludes = mapping.excludes || [];

// Build lookup sets for O(1) checks
const mappedSet  = new Set(Object.keys(mappings));
const excludeSet = new Set(excludes);

// ---------------------------------------------------------------------------
// Recursive directory walk — collect all .h and .cpp files under src/
// ---------------------------------------------------------------------------
function walkDir(dir, results) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walkDir(full, results);
    } else if (entry.isFile()) {
      const ext = path.extname(entry.name).toLowerCase();
      if (ext === '.h' || ext === '.cpp') {
        results.push(full);
      }
    }
  }
}

const allFiles = [];
walkDir(srcDir, allFiles);

// ---------------------------------------------------------------------------
// Normalize to repo-root-relative forward-slash paths
// ---------------------------------------------------------------------------
function normalize(absPath) {
  return path.relative(repoRoot, absPath).replace(/\\/g, '/');
}

// ---------------------------------------------------------------------------
// Classify each file
// ---------------------------------------------------------------------------
const fileMapped   = [];
const fileExcluded = [];
const fileMissing  = [];

for (const absPath of allFiles) {
  const rel = normalize(absPath);
  if (mappedSet.has(rel)) {
    fileMapped.push(rel);
  } else if (excludeSet.has(rel)) {
    fileExcluded.push(rel);
  } else {
    fileMissing.push(rel);
  }
}

const total = allFiles.length;

// ---------------------------------------------------------------------------
// Verbose output — full coverage report
// ---------------------------------------------------------------------------
if (verbose) {
  console.log('doc-mapping coverage report\n');

  // Sort all files for readability
  const allSorted = [...fileMapped, ...fileExcluded, ...fileMissing].sort();
  for (const rel of allSorted) {
    let status;
    if (mappedSet.has(rel)) {
      status = 'mapped   ';
    } else if (excludeSet.has(rel)) {
      status = 'excluded ';
    } else {
      status = 'MISSING  ';
    }
    console.log(`  ${status} ${rel}`);
  }
  console.log('');
}

// ---------------------------------------------------------------------------
// Stats output
// ---------------------------------------------------------------------------
if (stats) {
  console.log('Coverage statistics:');
  console.log(`  Total source files : ${total}`);
  console.log(`  Mapped             : ${fileMapped.length}`);
  console.log(`  Excluded           : ${fileExcluded.length}`);
  console.log(`  Missing            : ${fileMissing.length}`);
  console.log('');
}

// ---------------------------------------------------------------------------
// Summary / result
// ---------------------------------------------------------------------------
console.log('Checking doc-mapping coverage for src/**/*.{h,cpp}...\n');

if (fileMissing.length === 0) {
  console.log(`\u2713 All ${total} source files are mapped in doc-mapping.json`);
  process.exit(0);
} else {
  console.log(`\u2717 ${fileMissing.length} source file(s) not in doc-mapping.json:`);
  for (const rel of fileMissing.sort()) {
    console.log(`  ${rel}`);
  }
  console.log('');
  console.log('Add each file to tools/doc-mapping.json under "mappings" (with doc section) or "excludes" (intentionally undocumented).');
  if (!verbose) {
    console.log("Run 'node tools/check_mapping_coverage.js --verbose' for full coverage report.");
  }
  process.exit(1);
}
