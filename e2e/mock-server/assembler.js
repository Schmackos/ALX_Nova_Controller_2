/**
 * HTML Assembler — replicates tools/build_web_assets.js assembly logic.
 * Reads web_src/index.html, injects CSS and JS from web_src/ directories.
 * Also extracts login page HTML from src/login_page.h.
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const WEB_SRC = path.join(ROOT, 'web_src');
const HTML_TEMPLATE = path.join(WEB_SRC, 'index.html');
const CSS_DIR = path.join(WEB_SRC, 'css');
const JS_DIR = path.join(WEB_SRC, 'js');
const LOGIN_PAGE_H = path.join(ROOT, 'src', 'login_page.h');

let _cachedPage = null;
let _cachedLogin = null;
let _cachedPageMtime = 0;

function _getMaxMtime() {
  let max = 0;
  try {
    const check = (dir) => {
      fs.readdirSync(dir).forEach(f => {
        const stat = fs.statSync(path.join(dir, f));
        if (stat.mtimeMs > max) max = stat.mtimeMs;
      });
    };
    check(CSS_DIR);
    check(JS_DIR);
    const htmlStat = fs.statSync(HTML_TEMPLATE);
    if (htmlStat.mtimeMs > max) max = htmlStat.mtimeMs;
  } catch (e) { /* ignore */ }
  return max;
}

function assembleMainPage() {
  const mtime = _getMaxMtime();
  if (_cachedPage && mtime <= _cachedPageMtime) return _cachedPage;

  const html = fs.readFileSync(HTML_TEMPLATE, 'utf8');

  // Concatenate CSS files (sorted alpha)
  const cssFiles = fs.readdirSync(CSS_DIR).filter(f => f.endsWith('.css')).sort();
  const css = cssFiles.map(f => fs.readFileSync(path.join(CSS_DIR, f), 'utf8')).join('\n');

  // Concatenate JS files (sorted alpha) with sourceURL tags
  const jsFiles = fs.readdirSync(JS_DIR).filter(f => f.endsWith('.js')).sort();
  const js = jsFiles.map(f => {
    const content = fs.readFileSync(path.join(JS_DIR, f), 'utf8');
    return content + '\n//# sourceURL=' + f + '\n';
  }).join('\n');

  // Inject into template placeholders
  _cachedPage = html
    .replace('/* CSS_INJECT */', css)
    .replace('/* JS_INJECT */', js);
  _cachedPageMtime = mtime;

  return _cachedPage;
}

function assembleLoginPage() {
  if (_cachedLogin) return _cachedLogin;

  try {
    const src = fs.readFileSync(LOGIN_PAGE_H, 'utf8');
    const match = src.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/);
    if (match) {
      _cachedLogin = match[1];
      return _cachedLogin;
    }
  } catch (e) {
    // Fallback: minimal login page
  }

  _cachedLogin = `<!DOCTYPE html><html><body>
    <h1>Login</h1>
    <form id="loginForm"><input type="password" id="password" required>
    <button type="submit">Login</button></form></body></html>`;
  return _cachedLogin;
}

function invalidateCache() {
  _cachedPage = null;
  _cachedLogin = null;
}

module.exports = { assembleMainPage, assembleLoginPage, invalidateCache };
