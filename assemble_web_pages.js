const fs = require('fs');

const gzippedData = fs.readFileSync('web_assets_gzipped_fix.txt', 'utf8');

let finalContent = `#include "web_pages.h"
#include <Arduino.h>

// Gzipped web assets to save memory

// Gzipped version of htmlPage
const uint8_t htmlPage_gz[] PROGMEM = {
`;

// Find htmlPage_gz data
const htmlMatch = gzippedData.match(/\/\/ htmlPage_gz\s*([\s\S]*?)\s*const size_t htmlPage_gz_len = (\d+);/);
if (htmlMatch) {
    finalContent += htmlMatch[1].trim() + '\n};\n';
    finalContent += `const size_t htmlPage_gz_len = ${htmlMatch[2]};\n\n`;
}

finalContent += '// Gzipped version of apHtmlPage\nconst uint8_t apHtmlPage_gz[] PROGMEM = {\n';

// Find apHtmlPage_gz data
const apMatch = gzippedData.match(/\/\/ apHtmlPage_gz\s*([\s\S]*?)\s*const size_t apHtmlPage_gz_len = (\d+);/);
if (apMatch) {
    finalContent += apMatch[1].trim() + '\n};\n';
    finalContent += `const size_t apHtmlPage_gz_len = ${apMatch[2]};\n`;
}

fs.writeFileSync('src/web_pages.cpp', finalContent);
console.log('web_pages.cpp updated successfully with gzipped assets.');
