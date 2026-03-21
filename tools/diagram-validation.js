#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const ARCH_DIR = path.join(ROOT, 'docs-internal', 'architecture');

function parseMmd(filePath) {
  const content = fs.readFileSync(filePath, 'utf8');
  const lines = content.split('\n');
  let inBlock = false;
  const checks = []; // { sourceFile, identifier }

  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed === '%% @validate-symbols') { inBlock = true; continue; }
    if (trimmed === '%% @end-validate') { inBlock = false; continue; }
    if (!inBlock) continue;
    if (!trimmed.startsWith('%%')) continue;

    // %% src/hal/hal_types.h: HAL_STATE_UNKNOWN, HAL_STATE_AVAILABLE
    const inner = trimmed.slice(2).trim();
    const colonIdx = inner.indexOf(':');
    if (colonIdx === -1) continue;

    const sourceFile = inner.slice(0, colonIdx).trim();
    const identifiers = inner.slice(colonIdx + 1).split(',').map(s => s.trim()).filter(Boolean);
    for (const id of identifiers) {
      checks.push({ sourceFile, identifier: id });
    }
  }
  return checks;
}

function validate() {
  if (!fs.existsSync(ARCH_DIR)) {
    console.error(`✗ Architecture diagram directory not found: ${ARCH_DIR}`);
    process.exit(1);
  }

  const mmdFiles = fs.readdirSync(ARCH_DIR)
    .filter(f => f.endsWith('.mmd'))
    .map(f => path.join(ARCH_DIR, f));

  if (mmdFiles.length === 0) {
    console.log('No .mmd files found — nothing to validate.');
    process.exit(0);
  }

  const failures = [];
  let totalChecks = 0;
  let warnings = 0;

  for (const mmdFile of mmdFiles) {
    const diagramName = path.basename(mmdFile);
    const checks = parseMmd(mmdFile);

    if (checks.length === 0) {
      console.warn(`  ⚠  ${diagramName}: no @validate-symbols block — skipping`);
      warnings++;
      continue;
    }

    const sourceCache = {};
    for (const { sourceFile, identifier } of checks) {
      totalChecks++;
      const absSource = path.join(ROOT, sourceFile);

      if (!sourceCache[sourceFile]) {
        if (!fs.existsSync(absSource)) {
          failures.push({ diagram: diagramName, sourceFile, identifier, reason: 'source file not found' });
          continue;
        }
        sourceCache[sourceFile] = fs.readFileSync(absSource, 'utf8');
      }

      if (!sourceCache[sourceFile].includes(identifier)) {
        failures.push({ diagram: diagramName, sourceFile, identifier, reason: 'identifier not found in source' });
      }
    }
  }

  console.log(`Validating architecture diagrams in docs-internal/architecture/...\n`);

  if (failures.length === 0) {
    console.log(`✓ All ${totalChecks} diagram symbol checks passed (${warnings} diagrams without annotations)`);
    process.exit(0);
  }

  console.error(`✗ ${failures.length} diagram symbol check(s) failed:\n`);
  for (const f of failures) {
    console.error(`  ${f.diagram}`);
    console.error(`    identifier : ${f.identifier}`);
    console.error(`    source file: ${f.sourceFile}`);
    console.error(`    reason     : ${f.reason}`);
    console.error('');
  }
  console.error(`Update the diagram or its @validate-symbols block to match current source.`);
  process.exit(1);
}

validate();
