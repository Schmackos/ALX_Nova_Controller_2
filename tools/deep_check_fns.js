// deep_check_fns.js — finds functions called anywhere in JS/HTML but not defined in any module
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, '..');
const dir = path.join(root, 'web_src', 'js');

const files = fs.readdirSync(dir).filter(f => f.endsWith('.js')).sort();

// Collect all defined function names
const defined = new Set();
files.forEach(f => {
    const src = fs.readFileSync(path.join(dir, f), 'utf8');
    const re = /function\s+([a-zA-Z_$][a-zA-Z0-9_$]+)\s*\(/g;
    let m;
    while ((m = re.exec(src)) !== null) defined.add(m[1]);
});

// Collect all calls from JS + HTML
const allSrc = files.map(f => fs.readFileSync(path.join(dir, f), 'utf8')).join('\n');
const htmlSrc = fs.readFileSync(path.join(root, 'web_src', 'index.html'), 'utf8');
const called = new Set();
[allSrc, htmlSrc].forEach(src => {
    const re = /\b([a-zA-Z_$][a-zA-Z0-9_$]+)\s*\(/g;
    let m;
    while ((m = re.exec(src)) !== null) called.add(m[1]);
});

const skip = new Set([
    'if','for','while','switch','catch','function','async','await',
    'parseInt','parseFloat','isNaN','isFinite','Math','JSON','Object','Array','Set','Map',
    'Promise','fetch','console','document','window','navigator','localStorage','sessionStorage',
    'requestAnimationFrame','cancelAnimationFrame','performance','URL','Blob','FileReader',
    'QRCode','marked','DataView','Float32Array','Uint8Array','Int16Array','Int32Array',
    'WebSocket','XMLHttpRequest','Boolean','String','Number','Date','Error','RegExp',
    'encodeURIComponent','decodeURIComponent','atob','btoa','confirm','alert','prompt',
    'require','clearTimeout','clearInterval','setTimeout','setInterval',
    'push','pop','shift','unshift','splice','slice','concat','join','split','reverse',
    'sort','find','findIndex','indexOf','lastIndexOf','includes','every','some','flat',
    'flatMap','fill','keys','values','entries','from','of','isArray','assign','create',
    'freeze','fromEntries','hasOwnProperty','toString','valueOf','toFixed',
    'toUpperCase','toLowerCase','trim','trimStart','trimEnd','padStart','padEnd',
    'startsWith','endsWith','substring','substr','charAt','charCodeAt','fromCharCode',
    'replace','replaceAll','match','matchAll','search','test','exec',
    'stringify','parse','log','error','warn','info','debug',
    'getContext','beginPath','closePath','moveTo','lineTo','arcTo','fill','stroke',
    'fillRect','clearRect','strokeRect','arc','fillText','strokeText','measureText',
    'setTransform','translate','scale','rotate','save','restore','drawImage','clip',
    'createLinearGradient','addColorStop','createPattern','getImageData','putImageData',
    'scrollIntoView','appendChild','insertBefore','removeChild','createElement',
    'createTextNode','setAttribute','getAttribute','removeAttribute','getBoundingClientRect',
    'addEventListener','removeEventListener','dispatchEvent','querySelector','querySelectorAll',
    'getElementById','getElementsByClassName','getElementsByTagName','closest','matches',
    'contains','toggle','add','remove','has','forEach','reduce','map','filter','get','set',
    'delete','clear','then','catch','finally','resolve','reject','all','race','any','allSettled',
    'send','close','open','json','blob','text','arrayBuffer','clone','now',
    'floor','ceil','round','abs','max','min','pow','sqrt','log','log10','sin','cos',
    'tan','atan','atan2','PI','random','trunc','sign','hypot','exp',
    'getTime','toISOString','toLocaleDateString','toLocaleTimeString',
    'getHours','getMinutes','getSeconds','getMilliseconds',
    'getUTCHours','getUTCMinutes','getUTCSeconds','getUTCFullYear',
    'getDay','getDate','getMonth','getFullYear','setTime',
    'getUint8','getFloat32','getInt16','getInt32','getUint16','getUint32',
    'setUint8','setFloat32','setInt16','setInt32',
    'scrollTo','scrollBy','preventDefault','stopPropagation','stopImmediatePropagation',
    'createObjectURL','revokeObjectURL','readAsText','readAsArrayBuffer',
    'getItem','setItem','removeItem','readyState','terminate','postMessage',
    'handler','characters','toggle','isArray','size','type','constructor','prototype',
    'decodeURIComponent','encodeURIComponent','getComputedStyle','getPropertyValue',
    'setProperty','removeProperty','offsetWidth','offsetHeight','scrollTop','scrollHeight',
    'clientHeight','clientWidth','offsetTop','offsetLeft','tagName','nodeName',
    'textContent','innerHTML','innerText','value','checked','disabled','focus','blur',
    'click','submit','reset','select','scrollLeft','scrollWidth','offsetParent',
    'parentElement','firstChild','lastChild','firstElementChild','lastElementChild',
    'nextElementSibling','previousElementSibling','childNodes','children','nodeType',
    'ownerDocument','ownerSVGElement','dataset','style','classList','id','name',
    'href','src','alt','title','lang','dir','hidden','draggable','tabIndex',
    'onload','onerror','onclose','onmessage','onclick','onchange','oninput',
    'onsubmit','onfocus','onblur','onkeydown','onkeyup','onkeypress',
    'ontouchstart','ontouchmove','ontouchend','onmousedown','onmousemove','onmouseup'
]);

const missing = [];
called.forEach(n => {
    if (defined.has(n)) return;
    if (skip.has(n)) return;
    if (n.length <= 3) return;
    if (n[0] !== n[0].toLowerCase() || n[0] === n[0].toUpperCase()) return;
    missing.push(n);
});
missing.sort();
if (missing.length === 0) {
    console.log('All called functions are defined.');
} else {
    console.log('Potentially missing (' + missing.length + '):');
    missing.forEach(n => console.log('  ' + n));
    process.exit(1);
}
