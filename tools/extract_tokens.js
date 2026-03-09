#!/usr/bin/env node
/**
 * Design Token Extraction Script
 *
 * Reads src/design_tokens.h and generates two CSS files:
 *   - docs-site/src/css/tokens.css  (Docusaurus :root + IFM overrides)
 *   - web_src/css/00-tokens.css     (Web UI :root + body.night-mode)
 *
 * Tokens with `LVGL focus highlight` comments (no `CSS:` prefix) are skipped.
 *
 * Usage: node tools/extract_tokens.js
 */

'use strict';

const fs   = require('fs');
const path = require('path');

// ---------------------------------------------------------------------------
// Paths (relative to project root, resolved from this script's location)
// ---------------------------------------------------------------------------

const ROOT        = path.resolve(__dirname, '..');
const TOKENS_H    = path.join(ROOT, 'src', 'design_tokens.h');
const DOCS_CSS    = path.join(ROOT, 'docs-site', 'src', 'css', 'tokens.css');
const WEB_CSS     = path.join(ROOT, 'web_src', 'css', '00-tokens.css');

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

/**
 * @typedef {{ name: string, hex: string, cssVar: string, isNight: boolean }} Token
 */

/**
 * Parse `#define DT_*` lines from the header.
 * Returns only tokens that have a `CSS: --varname` comment.
 * Tokens marked `(night)` are flagged as dark-mode-only.
 *
 * @param {string} source
 * @returns {Token[]}
 */
function parseTokens(source) {
  const tokens = [];
  // Match: #define DT_NAME  0xRRGGBB  /* ... */
  const re = /^#define\s+(DT_\w+)\s+(0x[0-9A-Fa-f]{6})\s+\/\*(.*?)\*\//gm;
  let match;

  while ((match = re.exec(source)) !== null) {
    const name    = match[1];
    const rawHex  = match[2];           // e.g. 0xFF9800
    const comment = match[3].trim();    // e.g. "CSS: --accent" or "LVGL focus highlight"

    // Skip tokens that have no CSS mapping
    if (!comment.includes('CSS:')) {
      continue;
    }

    // Extract the CSS variable name and night-mode flag
    // Comment format: "CSS: --varname" or "CSS: --varname (night)"
    const cssMatch = comment.match(/CSS:\s*(--[\w-]+)(\s+\(night\))?/);
    if (!cssMatch) {
      process.stderr.write(`WARN: Could not parse CSS name from comment on ${name}: "${comment}"\n`);
      continue;
    }

    const cssVar  = cssMatch[1];            // e.g. --accent
    const isNight = cssMatch[2] !== undefined; // true when "(night)" present

    // Convert 0xRRGGBB → #RRGGBB
    const numeric = parseInt(rawHex, 16);
    const r = (numeric >> 16) & 0xff;
    const g = (numeric >> 8)  & 0xff;
    const b =  numeric        & 0xff;
    const hex = '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0').toUpperCase()).join('');

    tokens.push({ name, hex, cssVar, isNight });
  }

  if (tokens.length === 0) {
    throw new Error('No DT_* tokens with CSS: comments found — check the header path or format');
  }

  return tokens;
}

// ---------------------------------------------------------------------------
// Output generators
// ---------------------------------------------------------------------------

const AUTOGEN_HEADER = [
  '/* AUTO-GENERATED from src/design_tokens.h — do not edit manually */',
  '/* Run: node tools/extract_tokens.js */',
  '',
].join('\n');

/**
 * IFM variable mappings for Docusaurus.
 * Key = dt cssVar name, Value = array of additional --ifm-* vars to set.
 */
const IFM_MAPPINGS = {
  '--dt-accent':       ['--ifm-color-primary'],
  '--dt-accent-light': ['--ifm-color-primary-light', '--ifm-color-primary-lighter'],
  '--dt-accent-dark':  ['--ifm-color-primary-dark', '--ifm-color-primary-darker'],
};

/**
 * Generate docs-site/src/css/tokens.css
 *
 * Structure:
 *   :root {
 *     --dt-* variables (light-mode tokens)
 *     --ifm-color-primary overrides (accent tokens only)
 *   }
 *   [data-theme='dark'] {
 *     --dt-* variables (night/dark-mode tokens)
 *     --ifm-color-primary overrides for dark mode
 *   }
 *
 * @param {Token[]} tokens
 * @returns {string}
 */
function generateDocsCss(tokens) {
  const lightTokens = tokens.filter(t => !t.isNight);
  const darkTokens  = tokens.filter(t => t.isNight);

  const lines = [AUTOGEN_HEADER];

  // :root block — light mode
  lines.push(':root {');

  for (const t of lightTokens) {
    const dtVar = '--dt-' + t.cssVar.slice(2); // --accent → --dt-accent
    lines.push(`  ${dtVar}: ${t.hex};`);

    // Add IFM overrides immediately after the relevant token
    const ifmVars = IFM_MAPPINGS[dtVar];
    if (ifmVars) {
      for (const ifm of ifmVars) {
        lines.push(`  ${ifm}: ${t.hex};`);
      }
    }
  }

  lines.push('}', '');

  // [data-theme='dark'] block — dark mode
  if (darkTokens.length > 0) {
    lines.push("[data-theme='dark'] {");

    for (const t of darkTokens) {
      const dtVar = '--dt-' + t.cssVar.slice(2);
      lines.push(`  ${dtVar}: ${t.hex};`);
    }

    // In dark mode Docusaurus uses accent as --ifm-color-primary too.
    // The accent tokens are light-mode (not night), but we still want
    // the primary colour to be set in dark mode. Re-emit the accent dt
    // vars from lightTokens that have IFM mappings.
    const accentTokens = lightTokens.filter(t => {
      const dtVar = '--dt-' + t.cssVar.slice(2);
      return Boolean(IFM_MAPPINGS[dtVar]);
    });
    if (accentTokens.length > 0) {
      lines.push('');
      lines.push('  /* IFM primary colour overrides for dark mode */');
      for (const t of accentTokens) {
        const dtVar = '--dt-' + t.cssVar.slice(2);
        const ifmVars = IFM_MAPPINGS[dtVar];
        for (const ifm of ifmVars) {
          lines.push(`  ${ifm}: ${t.hex};`);
        }
      }
    }

    lines.push('}', '');
  }

  return lines.join('\n');
}

/**
 * Generate web_src/css/00-tokens.css
 *
 * Structure:
 *   :root {
 *     light-mode tokens using original CSS variable names (e.g. --accent)
 *   }
 *   body.night-mode {
 *     dark-mode tokens using original CSS variable names
 *   }
 *
 * @param {Token[]} tokens
 * @returns {string}
 */
function generateWebCss(tokens) {
  const lightTokens = tokens.filter(t => !t.isNight);
  const darkTokens  = tokens.filter(t => t.isNight);

  const lines = [AUTOGEN_HEADER];

  // :root block — light mode
  lines.push(':root {');
  for (const t of lightTokens) {
    lines.push(`  ${t.cssVar}: ${t.hex};`);
  }
  lines.push('}', '');

  // body.night-mode block — dark mode
  if (darkTokens.length > 0) {
    lines.push('body.night-mode {');
    for (const t of darkTokens) {
      lines.push(`  ${t.cssVar}: ${t.hex};`);
    }
    lines.push('}', '');
  }

  return lines.join('\n');
}

// ---------------------------------------------------------------------------
// File writing helper
// ---------------------------------------------------------------------------

/**
 * Write content to filePath, creating parent directories as needed.
 *
 * @param {string} filePath
 * @param {string} content
 */
function writeFile(filePath, content) {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, content, 'utf8');
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

function main() {
  // Read the header
  let source;
  try {
    source = fs.readFileSync(TOKENS_H, 'utf8');
  } catch (err) {
    process.stderr.write(`ERROR: Could not read ${TOKENS_H}: ${err.message}\n`);
    process.exit(1);
  }

  // Parse tokens
  let tokens;
  try {
    tokens = parseTokens(source);
  } catch (err) {
    process.stderr.write(`ERROR: ${err.message}\n`);
    process.exit(1);
  }

  process.stderr.write(`Parsed ${tokens.length} CSS tokens from src/design_tokens.h\n`);
  process.stderr.write(`  Light-mode: ${tokens.filter(t => !t.isNight).length} tokens\n`);
  process.stderr.write(`  Dark-mode:  ${tokens.filter(t => t.isNight).length} tokens\n`);

  // Generate and write docs-site tokens
  const docsCss = generateDocsCss(tokens);
  writeFile(DOCS_CSS, docsCss);
  process.stderr.write(`\nWrote: ${path.relative(ROOT, DOCS_CSS)}\n`);

  // Generate and write web UI tokens
  const webCss = generateWebCss(tokens);
  writeFile(WEB_CSS, webCss);
  process.stderr.write(`Wrote: ${path.relative(ROOT, WEB_CSS)}\n`);

  process.stderr.write('\nDone.\n');
}

main();
