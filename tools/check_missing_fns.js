// check_missing_fns.js — finds functions called in ws-router + HTML but not defined in any JS module
const fs = require('fs');
const path = require('path');

const jsDir = path.join(__dirname, '..', 'web_src', 'js');
const htmlFile = path.join(__dirname, '..', 'web_src', 'index.html');

// Get all function calls from ws-router and HTML
const router = fs.readFileSync(path.join(jsDir, '02-ws-router.js'), 'utf8');
const html = fs.readFileSync(htmlFile, 'utf8');

const called = new Set();
[router, html].forEach(function(src) {
    const re = /\b([a-zA-Z_$][a-zA-Z0-9_$]+)\s*\(/g;
    let m;
    while ((m = re.exec(src)) !== null) called.add(m[1]);
});

// Get all defined functions from all JS files
const defined = new Set();
const jsFiles = fs.readdirSync(jsDir).filter(function(f) { return f.endsWith('.js'); });
jsFiles.forEach(function(f) {
    const src = fs.readFileSync(path.join(jsDir, f), 'utf8');
    const re = /function\s+([a-zA-Z_$][a-zA-Z0-9_$]+)\s*\(/g;
    let m;
    while ((m = re.exec(src)) !== null) defined.add(m[1]);
});

const builtins = new Set([
    'if','for','while','switch','catch','function','setTimeout','setInterval',
    'clearTimeout','clearInterval','parseInt','parseFloat','isNaN','Math','JSON',
    'Object','Array','Set','Map','Promise','fetch','console','document','window',
    'navigator','localStorage','sessionStorage','requestAnimationFrame',
    'cancelAnimationFrame','performance','URL','Blob','FileReader','QRCode','marked',
    'DataView','Float32Array','Uint8Array','Int16Array','WebSocket','XMLHttpRequest',
    'Boolean','String','Number','Date','Error','RegExp','encodeURIComponent',
    'decodeURIComponent','atob','btoa','confirm','alert','prompt','require',
    'addEventListener','removeEventListener','querySelector','querySelectorAll',
    'getElementById','getElementsBy','push','forEach','filter','map','sort','find',
    'reduce','join','split','replace','trim','slice','indexOf','includes','keys',
    'values','entries','assign','create','fromEntries','hasOwnProperty','test',
    'exec','toString','padStart','padEnd','toFixed','toUpperCase','toLowerCase',
    'startsWith','endsWith','substring','substr','charAt','charCodeAt','fromCharCode',
    'stringify','parse','log','error','warn','info','debug','write','read',
    'getContext','beginPath','closePath','moveTo','lineTo','arcTo','fill','stroke',
    'fillRect','clearRect','strokeRect','arc','getImageData','putImageData',
    'createLinearGradient','addColorStop','measureText','fillText','strokeText',
    'setTransform','transform','translate','scale','rotate','save','restore',
    'drawImage','createPattern','clip','isPointInPath','scrollIntoView',
    'appendChild','insertBefore','removeChild','createElement','createTextNode',
    'setAttribute','getAttribute','removeAttribute','classList','style','textContent',
    'innerHTML','value','checked','disabled','focus','blur','click','submit',
    'getBoundingClientRect','offsetWidth','offsetHeight','scrollTop','scrollHeight',
    'clientHeight','parentElement','firstChild','lastChild','childNodes','children',
    'closest','matches','contains','scrollTo','open','close','then','catch','finally',
    'resolve','reject','all','race','any','allSettled','send','close','json','blob',
    'text','arrayBuffer','ok','status','headers','body','clone','type','size',
    'readAsText','readAsArrayBuffer','createObjectURL','revokeObjectURL','now',
    'floor','ceil','round','abs','max','min','pow','sqrt','log','log10','sin','cos',
    'tan','atan2','PI','random','trunc','sign','hypot','exp','getTime','toISOString',
    'toLocaleDateString','toLocaleTimeString','getHours','getMinutes','getSeconds',
    'getMilliseconds','getUTCHours','getUTCMinutes','getUTCSeconds','getUTCFullYear',
    'getDay','getDate','getMonth','getFullYear','setTime','constructor','prototype',
    'length','width','height','name','id','src','href','target','rel','type',
    'onload','onerror','onclose','onmessage','onclick','onchange','oninput',
    'onsubmit','onfocus','onblur','onkeydown','onkeyup','onkeypress',
    'ontouchstart','ontouchmove','ontouchend','onmousedown','onmousemove','onmouseup',
    'getUint8','getFloat32','getInt16','byteLength','byteOffset','buffer',
    'appendData','appendBIN','disconnect','getState','readyState','OPEN','CLOSED',
    'responseText','responseType','response','onreadystatechange','setRequestHeader',
    'send','abort','upload','progress','load','error','timeout','withCredentials',
    'textContent','nodeType','nodeName','tagName','nodeValue','parentNode',
    'nextSibling','previousSibling','ownerDocument','childElementCount',
    'matches','dataset','tabIndex','accessKey','title','lang','dir','hidden',
    'draggable','spellcheck','contentEditable','isContentEditable'
]);

const missing = [];
called.forEach(function(n) {
    if (!defined.has(n) && !builtins.has(n) && n.length > 4 && n[0] === n[0].toLowerCase() && n[0] !== n[0].toUpperCase()) {
        missing.push(n);
    }
});
missing.sort();
if (missing.length === 0) {
    console.log('All functions referenced in router/HTML appear to be defined.');
} else {
    console.log('Potentially missing functions (' + missing.length + '):');
    missing.forEach(function(n) { console.log('  ' + n); });
}
