const zlib = require('zlib');
const fs = require('fs');

const webPagesPath = 'src/web_pages.cpp';
const content = fs.readFileSync(webPagesPath, 'utf8');

function extractAsset(varName) {
    const regex = new RegExp(`const char ${varName}\\[\\] PROGMEM = R"rawliteral\\(([\\s\\S]*?)\\)rawliteral";`);
    const match = content.match(regex);
    return match ? match[1] : null;
}

const htmlPage = extractAsset('htmlPage');
const apHtmlPage = extractAsset('apHtmlPage');

function gzipToHex(text) {
    const buffer = zlib.gzipSync(Buffer.from(text));
    let hex = '';
    for (let i = 0; i < buffer.length; i++) {
        hex += '0x' + buffer[i].toString(16).padStart(2, '0') + ', ';
        if ((i + 1) % 12 === 0) hex += '\n  ';
    }
    return { hex, len: buffer.length };
}

if (htmlPage) {
    const result = gzipToHex(htmlPage);
    console.log('// htmlPage_gz');
    console.log(result.hex);
    console.log(`const size_t htmlPage_gz_len = ${result.len};\n`);
}

if (apHtmlPage) {
    const result = gzipToHex(apHtmlPage);
    console.log('// apHtmlPage_gz');
    console.log(result.hex);
    console.log(`const size_t apHtmlPage_gz_len = ${result.len};\n`);
}
