// find_dups.js — finds duplicate script-level declarations across web_src/js modules
const fs = require('fs');
const path = require('path');
const dir = path.join(__dirname, '..', 'web_src', 'js');
const files = fs.readdirSync(dir).filter(f => f.endsWith('.js')).sort();

const topDecls = {};
const topFuncs = {};

for (const fname of files) {
    const src = fs.readFileSync(path.join(dir, fname), 'utf8');
    const lines = src.split('\n');
    let depth = 0;
    let inBlockComment = false;

    for (let li = 0; li < lines.length; li++) {
        const line = lines[li];
        const trimmed = line.trim();

        // Track block comments
        let inLC = false;
        let inBC = inBlockComment;
        const lineDepthStart = depth;
        let d = depth;

        for (let ci = 0; ci < line.length; ci++) {
            if (inBC) {
                if (line[ci] === '*' && line[ci + 1] === '/') { inBC = false; ci++; }
                continue;
            }
            if (inLC) break;
            if (line[ci] === '/' && line[ci + 1] === '*') { inBC = true; ci++; continue; }
            if (line[ci] === '/' && line[ci + 1] === '/') { inLC = true; break; }
            if (line[ci] === '{') d++;
            if (line[ci] === '}') d--;
        }
        inBlockComment = inBC;

        if (lineDepthStart === 0 && !trimmed.startsWith('//') && !inBlockComment) {
            const dm = trimmed.match(/^(let|const|var)\s+([a-zA-Z_$][a-zA-Z0-9_$]*)/);
            if (dm) {
                const name = dm[2];
                if (!topDecls[name]) topDecls[name] = [];
                topDecls[name].push({ fname, line: li + 1, text: trimmed.slice(0, 90) });
            }
            const fm = trimmed.match(/^(?:async\s+)?function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(/);
            if (fm) {
                const name = fm[1];
                if (!topFuncs[name]) topFuncs[name] = [];
                topFuncs[name].push({ fname, line: li + 1 });
            }
        }

        depth = d;
    }
}

let anyFound = false;

console.log('=== DUPLICATE let/const/var at script top level ===');
for (const [name, locs] of Object.entries(topDecls).sort()) {
    const fileSet = new Set(locs.map(l => l.fname));
    if (fileSet.size > 1) {
        anyFound = true;
        console.log('DUP VAR: ' + name);
        locs.forEach(l => console.log('  ' + l.fname + ':' + l.line + ': ' + l.text));
    }
}

console.log('\n=== DUPLICATE function declarations at script top level ===');
for (const [name, locs] of Object.entries(topFuncs).sort()) {
    const fileSet = new Set(locs.map(l => l.fname));
    if (fileSet.size > 1) {
        anyFound = true;
        console.log('DUP FN: ' + name);
        locs.forEach(l => console.log('  ' + l.fname + ':' + l.line));
    }
}

if (!anyFound) {
    console.log('\nNo duplicates found.');
} else {
    process.exit(1);
}
