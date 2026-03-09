#!/usr/bin/env node
/**
 * Documentation Generation Script
 *
 * Calls the Anthropic Claude API to generate Docusaurus documentation pages
 * from firmware source files, using the docusaurus-expert agent system prompt
 * and section-specific writing skill templates.
 *
 * Usage:
 *   node tools/generate_docs.js --all
 *   node tools/generate_docs.js --sections api/rest-main,developer/websocket
 *   node tools/generate_docs.js --dry-run --all
 *   node tools/generate_docs.js --dry-run --sections api/rest-main
 */

'use strict';

const fs   = require('fs');
const path = require('path');

// ---------------------------------------------------------------------------
// Paths (all relative to project root, resolved from this file's location)
// ---------------------------------------------------------------------------
const ROOT         = path.resolve(__dirname, '..');
const AGENTS_DIR   = path.join(ROOT, '.claude', 'agents');
const PROMPTS_DIR  = path.join(__dirname, 'prompts');
const MAPPING_FILE = path.join(__dirname, 'doc-mapping.json');
const DOCS_OUT     = path.join(ROOT, 'docs-site', 'docs');
const LOGS_DIR     = path.join(ROOT, 'logs');

// Max characters to read from any single source file before truncating
const MAX_FILE_CHARS = 15000;

// Delay between API calls to respect rate limits (ms)
const API_CALL_DELAY_MS = 1000;

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------
const args = process.argv.slice(2);

if (args.length === 0 || args.includes('--help') || args.includes('-h')) {
    printUsage();
    process.exit(0);
}

const DRY_RUN        = args.includes('--dry-run');
const GEN_ALL        = args.includes('--all');
const sectionsIdx    = args.indexOf('--sections');
const SECTION_LIST   = sectionsIdx !== -1 ? (args[sectionsIdx + 1] || '').split(',').filter(Boolean) : [];

if (!GEN_ALL && SECTION_LIST.length === 0) {
    console.error('ERROR: Specify --all or --sections <comma-separated list>');
    printUsage();
    process.exit(1);
}

function printUsage() {
    console.log('');
    console.log('Usage:');
    console.log('  node tools/generate_docs.js --all                            Regenerate all sections');
    console.log('  node tools/generate_docs.js --sections api/rest-main,developer/websocket');
    console.log('  node tools/generate_docs.js --dry-run --all                  Show what would be generated');
    console.log('  node tools/generate_docs.js --dry-run --sections api/rest-main');
    console.log('');
    console.log('Environment:');
    console.log('  ANTHROPIC_API_KEY   Required (unless --dry-run)');
    console.log('');
}

// ---------------------------------------------------------------------------
// Validate API key early (skip in dry-run)
// ---------------------------------------------------------------------------
if (!DRY_RUN && !process.env.ANTHROPIC_API_KEY) {
    console.error('');
    console.error('ERROR: ANTHROPIC_API_KEY environment variable is not set.');
    console.error('');
    console.error('Set it with:');
    console.error('  export ANTHROPIC_API_KEY=sk-ant-...');
    console.error('');
    process.exit(1);
}

// ---------------------------------------------------------------------------
// Load Anthropic SDK lazily (not available during dry-run, or if not installed)
// ---------------------------------------------------------------------------
let Anthropic = null;
if (!DRY_RUN) {
    try {
        Anthropic = require('@anthropic-ai/sdk');
    } catch (e) {
        console.error('');
        console.error('ERROR: @anthropic-ai/sdk is not installed.');
        console.error('');
        console.error('Install it with:');
        console.error('  npm install @anthropic-ai/sdk');
        console.error('  # or, if running in the docs-site context:');
        console.error('  cd docs-site && npm install @anthropic-ai/sdk');
        console.error('');
        process.exit(1);
    }
}

// ---------------------------------------------------------------------------
// Load doc-mapping.json
// ---------------------------------------------------------------------------
if (!fs.existsSync(MAPPING_FILE)) {
    console.error(`ERROR: Mapping file not found: ${MAPPING_FILE}`);
    process.exit(1);
}

const docMapping = JSON.parse(fs.readFileSync(MAPPING_FILE, 'utf8'));
const { mappings, sectionToPromptType } = docMapping;

if (!mappings || !sectionToPromptType) {
    console.error('ERROR: doc-mapping.json must have "mappings" and "sectionToPromptType" keys.');
    process.exit(1);
}

// ---------------------------------------------------------------------------
// Load docusaurus-expert agent system prompt (strip YAML frontmatter)
// ---------------------------------------------------------------------------
const agentFile = path.join(AGENTS_DIR, 'docusaurus-expert.md');
if (!fs.existsSync(agentFile)) {
    console.error(`ERROR: Agent prompt not found: ${agentFile}`);
    process.exit(1);
}

const agentRaw = fs.readFileSync(agentFile, 'utf8');
const agentPrompt = stripFrontmatter(agentRaw);

/**
 * Strips YAML frontmatter (content between leading --- markers) from a
 * markdown string.  If there is no frontmatter the string is returned as-is.
 *
 * @param {string} text
 * @returns {string}
 */
function stripFrontmatter(text) {
    const trimmed = text.trimStart();
    if (!trimmed.startsWith('---')) return text;
    const endIdx = trimmed.indexOf('\n---', 3);
    if (endIdx === -1) return text;
    // Skip past the closing '---' line (and an optional trailing newline)
    return trimmed.slice(endIdx + 4).replace(/^\n/, '');
}

// ---------------------------------------------------------------------------
// Determine which sections to generate
// ---------------------------------------------------------------------------
const allSections = Object.keys(sectionToPromptType);
const targetSections = GEN_ALL ? allSections : SECTION_LIST;

// Validate requested sections
const unknownSections = targetSections.filter(s => !sectionToPromptType[s]);
if (unknownSections.length > 0) {
    console.error('ERROR: Unknown section(s). Check sectionToPromptType in doc-mapping.json:');
    unknownSections.forEach(s => console.error(`  - ${s}`));
    process.exit(1);
}

// ---------------------------------------------------------------------------
// Build reverse lookup: section -> [source file paths]
// ---------------------------------------------------------------------------

/**
 * Returns all source file paths that map to the given section.
 *
 * @param {string} section
 * @returns {string[]}
 */
function sourceFilesForSection(section) {
    return Object.entries(mappings)
        .filter(([, sections]) => sections.includes(section))
        .map(([filePath]) => filePath);
}

// ---------------------------------------------------------------------------
// Determine whether a section needs API data from extract_api.js
// ---------------------------------------------------------------------------
const API_PROMPT_TYPES = new Set(['api-reference']);

/**
 * Returns true if the section's prompt type is an API reference type that
 * benefits from structured API data extracted by extract_api.js.
 *
 * @param {string} section
 * @returns {boolean}
 */
function sectionNeedsApiData(section) {
    const promptType = sectionToPromptType[section];
    return API_PROMPT_TYPES.has(promptType);
}

/**
 * Loads API data via extract_api.js and returns the subset relevant to the
 * given section's source files.  Returns null if extract_api.js is not
 * present or extraction fails.
 *
 * @param {string} section
 * @param {string[]} sectionSourceFiles  Relative paths of source files for this section
 * @returns {object|null}
 */
function loadApiDataForSection(section, sectionSourceFiles) {
    const extractScript = path.join(__dirname, 'extract_api.js');
    if (!fs.existsSync(extractScript)) {
        return null;
    }

    try {
        // eslint-disable-next-line global-require
        const extractor = require(extractScript);
        if (typeof extractor.extractAll !== 'function') {
            console.warn(`  [WARN] extract_api.js does not export extractAll() — skipping API data`);
            return null;
        }

        const allApiData = extractor.extractAll();

        // Filter to keys matching any of this section's source files
        const filtered = {};
        for (const srcFile of sectionSourceFiles) {
            const baseName = path.basename(srcFile);
            for (const [key, value] of Object.entries(allApiData)) {
                if (key === srcFile || key === baseName || key.includes(baseName)) {
                    filtered[key] = value;
                }
            }
        }

        return Object.keys(filtered).length > 0 ? filtered : null;
    } catch (err) {
        console.warn(`  [WARN] extract_api.js threw an error: ${err.message}`);
        return null;
    }
}

// ---------------------------------------------------------------------------
// Read source file contents
// ---------------------------------------------------------------------------

/**
 * Reads a source file and returns its content, truncated to MAX_FILE_CHARS if
 * needed.  Returns null if the file does not exist (with a warning).
 *
 * @param {string} relativePath  Path relative to project root
 * @returns {{ path: string, content: string }|null}
 */
function readSourceFile(relativePath) {
    const absPath = path.join(ROOT, relativePath);
    if (!fs.existsSync(absPath)) {
        console.warn(`  [WARN] Source file not found, skipping: ${relativePath}`);
        return null;
    }

    let content = fs.readFileSync(absPath, 'utf8');
    const originalLength = content.length;

    if (content.length > MAX_FILE_CHARS) {
        content = content.slice(0, MAX_FILE_CHARS);
        const truncKB = (MAX_FILE_CHARS / 1024).toFixed(0);
        const origKB  = (originalLength / 1024).toFixed(0);
        console.log(`  [TRUNCATED] ${relativePath} (${origKB} KB → first ${truncKB} KB)`);
    }

    return { path: relativePath, content };
}

// ---------------------------------------------------------------------------
// Prompt template loader
// ---------------------------------------------------------------------------

/**
 * Loads the writing skill template for the given prompt type.
 *
 * @param {string} promptType
 * @returns {string}
 */
function loadPromptTemplate(promptType) {
    const templateFile = path.join(PROMPTS_DIR, `${promptType}.txt`);
    if (!fs.existsSync(templateFile)) {
        throw new Error(`Prompt template not found: ${templateFile}`);
    }
    return fs.readFileSync(templateFile, 'utf8');
}

// ---------------------------------------------------------------------------
// Build user message content
// ---------------------------------------------------------------------------

/**
 * Assembles the full user message to send to the API.
 *
 * @param {string} template      Writing skill template text
 * @param {{ path: string, content: string }[]} sourceFiles
 * @param {object|null} apiData  Structured API data or null
 * @returns {string}
 */
function buildUserMessage(template, sourceFiles, apiData) {
    let msg = template.trimEnd() + '\n\n';

    msg += '<source_files>\n';
    for (const file of sourceFiles) {
        msg += `<file path="${file.path}">\n${file.content}\n</file>\n`;
    }
    msg += '</source_files>\n';

    if (apiData) {
        msg += `\n<api_data>\n${JSON.stringify(apiData, null, 2)}\n</api_data>\n`;
    }

    return msg;
}

// ---------------------------------------------------------------------------
// Claude API call
// ---------------------------------------------------------------------------

/**
 * Calls the Claude API and returns generated content plus token counts.
 *
 * @param {string} userContent   Full assembled user message
 * @param {string} systemPrompt  docusaurus-expert agent prompt (no frontmatter)
 * @returns {Promise<{ content: string, inputTokens: number, outputTokens: number }>}
 */
async function callClaudeApi(userContent, systemPrompt) {
    const client = new Anthropic();

    const msg = await client.messages.create({
        model:      'claude-sonnet-4-6',
        max_tokens: 8192,
        system:     systemPrompt,
        messages:   [{ role: 'user', content: userContent }],
    });

    const text = msg.content
        .filter(block => block.type === 'text')
        .map(block => block.text)
        .join('');

    return {
        content:      text,
        inputTokens:  msg.usage.input_tokens,
        outputTokens: msg.usage.output_tokens,
    };
}

// ---------------------------------------------------------------------------
// Write output file
// ---------------------------------------------------------------------------

/**
 * Writes generated markdown content to the appropriate docs-site output path.
 * Creates intermediate directories as needed.
 *
 * @param {string} section    e.g. "api/rest-main" or "developer/hal/drivers"
 * @param {string} content    Markdown text to write
 * @returns {string}          Absolute path of the file written
 */
function writeOutputFile(section, content) {
    const outPath = path.join(DOCS_OUT, `${section}.md`);
    const outDir  = path.dirname(outPath);

    if (!fs.existsSync(outDir)) {
        fs.mkdirSync(outDir, { recursive: true });
    }

    fs.writeFileSync(outPath, content, 'utf8');
    return outPath;
}

// ---------------------------------------------------------------------------
// Sleep helper
// ---------------------------------------------------------------------------

/**
 * Returns a promise that resolves after `ms` milliseconds.
 *
 * @param {number} ms
 * @returns {Promise<void>}
 */
function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// ---------------------------------------------------------------------------
// Ensure logs directory exists
// ---------------------------------------------------------------------------
if (!fs.existsSync(LOGS_DIR)) {
    fs.mkdirSync(LOGS_DIR, { recursive: true });
}

// ---------------------------------------------------------------------------
// Main generation loop
// ---------------------------------------------------------------------------
async function main() {
    console.log('');
    console.log('=== Documentation Generation Script ===');
    console.log(`Mode:     ${DRY_RUN ? 'DRY RUN (no API calls)' : 'LIVE'}`);
    console.log(`Sections: ${GEN_ALL ? 'ALL (' + allSections.length + ')' : targetSections.join(', ')}`);
    console.log(`Target:   ${DOCS_OUT}`);
    console.log('');

    let successCount = 0;
    let errorCount   = 0;
    let totalInput   = 0;
    let totalOutput  = 0;
    const errors     = [];

    for (let i = 0; i < targetSections.length; i++) {
        const section    = targetSections[i];
        const promptType = sectionToPromptType[section];
        const progress   = `[${i + 1}/${targetSections.length}]`;

        console.log(`${progress} Section: ${section}  (prompt: ${promptType})`);

        // Gather source files for this section
        const relativeSourcePaths = sourceFilesForSection(section);
        if (relativeSourcePaths.length === 0) {
            console.warn(`  [WARN] No source files mapped to "${section}" — skipping`);
            continue;
        }

        console.log(`  Source files (${relativeSourcePaths.length}):`);
        relativeSourcePaths.forEach(f => console.log(`    - ${f}`));

        // In dry-run mode, just report and move on
        if (DRY_RUN) {
            const outPath = path.join(DOCS_OUT, `${section}.md`);
            console.log(`  [DRY RUN] Would write: ${outPath}`);
            console.log('');
            successCount++;
            continue;
        }

        try {
            // Load writing skill template
            const template = loadPromptTemplate(promptType);

            // Read source file contents
            const sourceFiles = relativeSourcePaths
                .map(readSourceFile)
                .filter(Boolean);

            if (sourceFiles.length === 0) {
                console.warn(`  [WARN] All source files missing for "${section}" — skipping`);
                continue;
            }

            // Optionally load structured API data
            let apiData = null;
            if (sectionNeedsApiData(section)) {
                apiData = loadApiDataForSection(section, relativeSourcePaths);
                if (apiData) {
                    console.log(`  API data keys: ${Object.keys(apiData).join(', ')}`);
                } else {
                    console.log('  API data: none (extract_api.js absent or no matches)');
                }
            }

            // Build user message
            const userContent = buildUserMessage(template, sourceFiles, apiData);

            // Call Claude API
            console.log('  Calling Claude API...');
            const result = await callClaudeApi(userContent, agentPrompt);

            totalInput  += result.inputTokens;
            totalOutput += result.outputTokens;

            // Write output file
            const outPath = writeOutputFile(section, result.content);

            console.log(`  Tokens: ${result.inputTokens} in / ${result.outputTokens} out`);
            console.log(`  Written: ${outPath}`);
            console.log('');

            successCount++;

            // Rate-limit: wait between calls (skip after last item)
            if (i < targetSections.length - 1) {
                await sleep(API_CALL_DELAY_MS);
            }

        } catch (err) {
            errorCount++;
            errors.push({ section, message: err.message });
            console.error(`  [ERROR] Failed to generate "${section}": ${err.message}`);
            console.log('');
            // Continue to next section — do not abort
        }
    }

    // ---------------------------------------------------------------------------
    // Summary
    // ---------------------------------------------------------------------------
    console.log('=== Summary ===');
    console.log(`Sections attempted:  ${targetSections.length}`);
    console.log(`Successfully generated: ${successCount}`);
    console.log(`Errors:              ${errorCount}`);

    if (!DRY_RUN) {
        console.log(`Total tokens used:   ${totalInput + totalOutput} (${totalInput} in / ${totalOutput} out)`);
    }

    if (errors.length > 0) {
        console.log('');
        console.log('Failed sections:');
        errors.forEach(e => console.log(`  - ${e.section}: ${e.message}`));
    }

    // Write log file for this run
    if (!DRY_RUN) {
        const logTimestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
        const logPath = path.join(LOGS_DIR, `generate_docs_${logTimestamp}.log`);
        const logContent = [
            `Run: ${new Date().toISOString()}`,
            `Mode: ${DRY_RUN ? 'dry-run' : 'live'}`,
            `Sections: ${targetSections.join(', ')}`,
            `Success: ${successCount}`,
            `Errors: ${errorCount}`,
            `Tokens: ${totalInput} in / ${totalOutput} out`,
            '',
            ...(errors.length > 0
                ? ['Errors:', ...errors.map(e => `  ${e.section}: ${e.message}`)]
                : ['No errors']),
        ].join('\n');

        fs.writeFileSync(logPath, logContent, 'utf8');
        console.log('');
        console.log(`Log written: ${logPath}`);
    }

    console.log('');

    if (errorCount > 0) {
        process.exit(1);
    }
}

main().catch(err => {
    console.error('');
    console.error('FATAL:', err.message);
    console.error(err.stack);
    process.exit(1);
});
