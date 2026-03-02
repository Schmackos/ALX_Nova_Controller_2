#!/usr/bin/env node
/**
 * Web Assets Build Script
 *
 * Reads web_src/index.html as the HTML shell template, injects CSS from
 * web_src/css/*.css and JS from web_src/js/*.js (sorted alphanumerically),
 * writes the assembled page to src/web_pages.cpp as C++ raw string literals,
 * then gzips the result to src/web_pages_gz.cpp.
 *
 * Usage: node tools/build_web_assets.js [--minify]
 */

const zlib = require('zlib');
const fs = require('fs');
const path = require('path');

const MINIFY = process.argv.includes('--minify');

// Source directories and files
const WEB_SRC_DIR   = 'web_src';
const HTML_TEMPLATE = path.join(WEB_SRC_DIR, 'index.html');
const CSS_DIR       = path.join(WEB_SRC_DIR, 'css');
const JS_DIR        = path.join(WEB_SRC_DIR, 'js');

// C++ source and gzip output
const WEB_PAGES_SRC  = 'src/web_pages.cpp';
const LOGIN_PAGE_SRC = 'src/login_page.h';
const OUTPUT_FILE    = 'src/web_pages_gz.cpp';

console.log('=== Web Assets Build Script ===');
console.log(`Minification: ${MINIFY ? 'ENABLED' : 'DISABLED'}`);

// ---------------------------------------------------------------------------
// 1. Read the HTML shell template
//    Throws ENOENT if web_src/index.html does not yet exist — that is
//    the expected error until the companion agent creates it.
// ---------------------------------------------------------------------------
const htmlTemplate = fs.readFileSync(HTML_TEMPLATE, 'utf8');

// ---------------------------------------------------------------------------
// 2. Concatenate CSS files in sorted alphanumeric order
// ---------------------------------------------------------------------------
console.log('\nReading CSS sources...');
const cssFiles = fs.readdirSync(CSS_DIR)
    .filter(f => f.endsWith('.css'))
    .sort();

let css = cssFiles.map(f => {
    const content = fs.readFileSync(path.join(CSS_DIR, f), 'utf8');
    console.log(`  [CSS] ${f} (${(Buffer.byteLength(content, 'utf8') / 1024).toFixed(1)} KB)`);
    return content;
}).join('\n');

// ---------------------------------------------------------------------------
// 3. Concatenate JS files in sorted alphanumeric order.
//    Each file gets a //# sourceURL comment for DevTools debugging.
//    If the directory is empty, js remains ''.
// ---------------------------------------------------------------------------
console.log('Reading JS sources...');
const jsFiles = fs.readdirSync(JS_DIR)
    .filter(f => f.endsWith('.js'))
    .sort();

let js = jsFiles.map(f => {
    const content = fs.readFileSync(path.join(JS_DIR, f), 'utf8');
    console.log(`  [JS]  ${f} (${(Buffer.byteLength(content, 'utf8') / 1024).toFixed(1)} KB)`);
    // Append sourceURL tag so browser DevTools show the original filename
    return content + '\n//# sourceURL=' + f + '\n';
}).join('\n');

// ---------------------------------------------------------------------------
// 3b. Duplicate declaration check — catches script-level let/const/var/function
//     re-declarations that cause SyntaxError when files are concatenated.
// ---------------------------------------------------------------------------
(function checkDuplicates() {
    const topDecls = {};
    const topFuncs = {};
    for (const fname of jsFiles) {
        const src = fs.readFileSync(path.join(JS_DIR, fname), 'utf8');
        const lines = src.split('\n');
        let depth = 0, inBlockComment = false;
        for (let li = 0; li < lines.length; li++) {
            const line = lines[li];
            const trimmed = line.trim();
            let inBC = inBlockComment, d = depth, inLC = false;
            for (let ci = 0; ci < line.length; ci++) {
                if (inBC) { if (line[ci]==='*'&&line[ci+1]==='/'){inBC=false;ci++;} continue; }
                if (inLC) break;
                if (line[ci]==='/'&&line[ci+1]==='*'){inBC=true;ci++;continue;}
                if (line[ci]==='/'&&line[ci+1]==='/'){inLC=true;break;}
                if (line[ci]==='{') d++;
                if (line[ci]==='}') d--;
            }
            inBlockComment = inBC;
            if (depth === 0 && !trimmed.startsWith('//')) {
                const dm = trimmed.match(/^(let|const|var)\s+([a-zA-Z_$][a-zA-Z0-9_$]*)/);
                if (dm) { const n=dm[2]; if(!topDecls[n])topDecls[n]=[]; topDecls[n].push(fname+':'+(li+1)); }
                const fm = trimmed.match(/^(?:async\s+)?function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(/);
                if (fm) { const n=fm[1]; if(!topFuncs[n])topFuncs[n]=[]; topFuncs[n].push(fname+':'+(li+1)); }
            }
            depth = d;
        }
    }
    let errors = [];
    for (const [n,locs] of Object.entries(topDecls)) {
        if (new Set(locs.map(l=>l.split(':')[0])).size > 1) errors.push('DUP VAR: ' + n + '\n  ' + locs.join('\n  '));
    }
    for (const [n,locs] of Object.entries(topFuncs)) {
        if (new Set(locs.map(l=>l.split(':')[0])).size > 1) errors.push('DUP FN: ' + n + '\n  ' + locs.join('\n  '));
    }
    if (errors.length > 0) {
        console.error('\n[ERROR] Duplicate script-level declarations found — will cause SyntaxError in browser:');
        errors.forEach(e => console.error('  ' + e));
        console.error('\nFix duplicates in web_src/js/ before building.\n');
        process.exit(1);
    }
    console.log('  [OK] No duplicate script-level declarations found.');
})();

// ---------------------------------------------------------------------------
// 4. CSS minifier (verbatim from original script)
// ---------------------------------------------------------------------------

/**
 * Simple CSS minifier (removes comments, extra whitespace)
 */
function minifyCSS(css) {
    return css
        .replace(/\/\*[\s\S]*?\*\//g, '')           // Remove comments
        .replace(/\s+/g, ' ')                        // Collapse whitespace
        .replace(/\s*([{}:;,>+~])\s*/g, '$1')       // Remove space around symbols
        .replace(/;}/g, '}')                         // Remove last semicolon
        .replace(/^\s+|\s+$/g, '');                  // Trim
}

if (MINIFY && css.length > 0) {
    css = minifyCSS(css);
}

// ---------------------------------------------------------------------------
// 5. Inject CSS and JS into the template
//    Placeholders in index.html: /* CSS_INJECT */ and /* JS_INJECT */
// ---------------------------------------------------------------------------
let assembledHTML = htmlTemplate
    .replace('/* CSS_INJECT */', css)
    .replace('/* JS_INJECT */', js);

// ---------------------------------------------------------------------------
// 6. Extract apHtmlPage from the current web_pages.cpp.
//    The AP-mode page is not managed by web_src/ and is preserved verbatim.
// ---------------------------------------------------------------------------
console.log('\nReading existing src/web_pages.cpp to extract apHtmlPage...');
const existingWebPages = fs.readFileSync(WEB_PAGES_SRC, 'utf8');

function extractRawLiteralBody(content, varName) {
    const regex = new RegExp(
        'const char ' + varName + '\\[\\] PROGMEM = R"rawliteral\\(([\\s\\S]*?)\\)rawliteral";'
    );
    const match = content.match(regex);
    return match ? match[1] : null;
}

const apHtmlPageBody = extractRawLiteralBody(existingWebPages, 'apHtmlPage');
if (!apHtmlPageBody) {
    console.error('ERROR: Could not extract apHtmlPage from ' + WEB_PAGES_SRC);
    process.exit(1);
}
console.log('  [OK] apHtmlPage extracted (' +
    (Buffer.byteLength(apHtmlPageBody, 'utf8') / 1024).toFixed(1) + ' KB)');

// ---------------------------------------------------------------------------
// 7. Write the new src/web_pages.cpp
//    Preserves the exact format: rawliteral delimiter, PROGMEM attribute,
//    #include directives — identical to what the firmware expects.
// ---------------------------------------------------------------------------
console.log('\nWriting src/web_pages.cpp...');

const webPagesCpp =
    '#include "web_pages.h"\n' +
    '#include <pgmspace.h>\n' +
    '\n' +
    'const char htmlPage[] PROGMEM = R"rawliteral(\n' +
    assembledHTML +
    '\n)rawliteral";\n' +
    '\n' +
    'const char apHtmlPage[] PROGMEM = R"rawliteral(' +
    apHtmlPageBody +
    ')rawliteral";\n';

fs.writeFileSync(WEB_PAGES_SRC, webPagesCpp);
console.log('  [OK] src/web_pages.cpp written');

// ---------------------------------------------------------------------------
// 8. Read login_page.h (unchanged — not managed by this build path)
// ---------------------------------------------------------------------------
const loginPageContent = fs.existsSync(LOGIN_PAGE_SRC)
    ? fs.readFileSync(LOGIN_PAGE_SRC, 'utf8')
    : null;

// ---------------------------------------------------------------------------
// Helpers: minifyHTML, toHexArray, processAsset (verbatim from original)
// ---------------------------------------------------------------------------

/**
 * Minify HTML content (CSS only - JS minification requires a proper parser)
 * Gzip compression handles most redundancy in JS/HTML whitespace anyway
 */
function minifyHTML(html) {
    if (!MINIFY) return html;

    // Only minify CSS in <style> tags (safe - CSS is simpler to minify)
    // Don't touch HTML whitespace as it can break JS strings containing HTML
    html = html.replace(/<style>([\s\S]*?)<\/style>/gi, (match, css) => {
        return '<style>' + minifyCSS(css) + '</style>';
    });

    return html;
}

/**
 * Convert buffer to C hex array format
 */
function toHexArray(buffer) {
    const bytes = [];
    for (let i = 0; i < buffer.length; i++) {
        bytes.push('0x' + buffer[i].toString(16).padStart(2, '0'));
    }

    // Format with 16 bytes per line
    let result = '    ';
    for (let i = 0; i < bytes.length; i++) {
        result += bytes[i];
        if (i < bytes.length - 1) {
            result += ', ';
            if ((i + 1) % 16 === 0) {
                result += '\n    ';
            }
        }
    }
    return result;
}

/**
 * Process an asset: minify (optional) and gzip
 */
function processAsset(name, html) {
    if (!html) {
        console.log('  [SKIP] ' + name + ': not found');
        return null;
    }

    const originalSize = Buffer.byteLength(html, 'utf8');

    // Minify if enabled
    const processed = minifyHTML(html);
    const processedSize = Buffer.byteLength(processed, 'utf8');

    // Gzip with maximum compression
    const gzipped = zlib.gzipSync(Buffer.from(processed), { level: 9 });

    const savings = ((1 - gzipped.length / originalSize) * 100).toFixed(1);

    console.log('  [OK] ' + name + ':');
    console.log('       Original: ' + (originalSize / 1024).toFixed(1) + ' KB');
    if (MINIFY) {
        console.log('       Minified: ' + (processedSize / 1024).toFixed(1) + ' KB');
    }
    console.log('       Gzipped:  ' + (gzipped.length / 1024).toFixed(1) + ' KB (' + savings + '% smaller)');

    return {
        name,
        data: gzipped,
        hexArray: toHexArray(gzipped),
        length: gzipped.length
    };
}

// ---------------------------------------------------------------------------
// 9. Gzip and write src/web_pages_gz.cpp
//    Re-read the freshly written web_pages.cpp so the bodies are consistent.
// ---------------------------------------------------------------------------
console.log('\nProcessing assets for gzip output...');

const freshWebPages  = fs.readFileSync(WEB_PAGES_SRC, 'utf8');
const htmlPageBody   = extractRawLiteralBody(freshWebPages, 'htmlPage');
const apPageBody     = extractRawLiteralBody(freshWebPages, 'apHtmlPage');
const loginPageBody  = loginPageContent
    ? extractRawLiteralBody(loginPageContent, 'loginPage')
    : null;

const assets = [
    processAsset('htmlPage',   htmlPageBody),
    processAsset('apHtmlPage', apPageBody),
    processAsset('loginPage',  loginPageBody)
].filter(a => a !== null);

// Generate output file
console.log('\nGenerating output...');

let output = '// Auto-generated gzipped web assets\n' +
    '// Generated by build_web_assets.js\n' +
    '// DO NOT EDIT - regenerate with: node tools/build_web_assets.js' +
    (MINIFY ? ' --minify' : '') + '\n' +
    '\n' +
    '#include "web_pages.h"\n' +
    '\n';

for (const asset of assets) {
    output += '// Gzipped ' + asset.name + ' (' + asset.length + ' bytes)\n';
    output += 'const uint8_t ' + asset.name + '_gz[] PROGMEM = {\n';
    output += asset.hexArray;
    output += '\n};\n';
    output += 'const size_t ' + asset.name + '_gz_len = ' + asset.length + ';\n\n';
}

fs.writeFileSync(OUTPUT_FILE, output);
console.log('\nOutput written to: ' + OUTPUT_FILE);

// Summary
const totalGzipped = assets.reduce((sum, a) => sum + a.length, 0);
console.log('\nTotal gzipped size: ' + (totalGzipped / 1024).toFixed(1) + ' KB');
console.log('\nTo use gzipped assets:');
console.log('  1. Include "web_pages_gz.cpp" in your build');
console.log('  2. Use sendGzipped() helper from web_pages.h');
console.log('  3. Set Content-Encoding: gzip header');
