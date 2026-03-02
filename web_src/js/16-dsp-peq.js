// ===== DSP Tab Constants =====
const DSP_TYPES = ['LPF','HPF','BPF','Notch','PEQ','Low Shelf','High Shelf','Allpass','AP360','AP180','BPF0dB','Custom','Limiter','FIR','Gain','Delay','Polarity','Mute','Compressor','LPF 1st','HPF 1st','Linkwitz','Decimator','Convolution','Noise Gate','Tone Controls','Speaker Prot','Stereo Width','Loudness','Bass Enhance','Multiband Comp'];
const DSP_MAX_CH = 4;
const DSP_CH_NAMES = ['L1','R1','L2','R2'];
function dspChLabel(c) { return inputNames[c] || DSP_CH_NAMES[c]; }

// ===== DSP/PEQ State Variables =====
let dspState = null;
let dspCh = 0; // selected channel
let dspOpenStage = -1; // expanded stage index
let dspImportMode = ''; // 'apo' or 'json'

// ===== PEQ State =====
const DSP_PEQ_BANDS = 10;
const PEQ_COLORS = ['#F44336','#E91E63','#9C27B0','#3F51B5','#2196F3','#00BCD4','#4CAF50','#8BC34A','#FFC107','#FF5722'];
const PEQ_FILTER_TYPES = [
    {value:4,label:'PEQ'},{value:5,label:'Low Shelf'},{value:6,label:'High Shelf'},
    {value:3,label:'Notch'},{value:2,label:'BPF'},{value:0,label:'LPF'},
    {value:1,label:'HPF'},{value:7,label:'Allpass'}
];
let peqSelectedBand = 0;
let peqLinked = false;
let peqGraphLayers = { individual: true, rta: false, chain: true };
let peqRtaData = null;
let peqDragging = null;
let peqCanvasInited = false;

// Hit-test radii for mouse vs touch
var PEQ_MOUSE_HIT_RADIUS = 12;  // pixels
var PEQ_TOUCH_HIT_RADIUS = 22;  // larger for finger touch

// Pinch-to-zoom state
var peqCanvasZoom = { fMin: 20, fMax: 20000 };
var _peqPinchStartDist = 0;
var _peqPinchStartZoom = null;

function peqGetBands() {
    if (!dspState || !dspState.channels[dspCh]) return [];
    return (dspState.channels[dspCh].stages || []).slice(0, DSP_PEQ_BANDS);
}
function peqRenderBandStrip() {
    var el = document.getElementById('peqBandStrip');
    if (!el) return;
    var bands = peqGetBands();
    var html = '';
    for (var i = 0; i < DSP_PEQ_BANDS; i++) {
        var b = bands[i];
        var active = (i === peqSelectedBand);
        var enabled = b && b.enabled;
        html += '<button class="peq-band-pill' + (active ? ' active' : '') + (enabled ? ' enabled' : '') + '" onclick="peqSelectBand(' + i + ')" style="--band-color:' + PEQ_COLORS[i] + ';">' + (i + 1) + '</button>';
    }
    el.innerHTML = html;
}
function peqSelectBand(band) {
    peqSelectedBand = band;
    peqRenderBandStrip();
    peqRenderBandDetail();
    dspDrawFreqResponse();
    // On mobile, show band detail as bottom sheet
    if (window.innerWidth <= 600) {
        var detail = document.querySelector('.peq-band-detail');
        if (detail) {
            detail.classList.add('visible');
            // Add handle if not present
            if (!detail.querySelector('.peq-band-detail-handle')) {
                var handle = document.createElement('div');
                handle.className = 'peq-band-detail-handle';
                detail.insertBefore(handle, detail.firstChild);
            }
        }
    }
}
function peqRenderBandDetail() {
    var el = document.getElementById('peqBandDetail');
    if (!el || !dspState || !dspState.channels[dspCh]) return;
    var bands = peqGetBands();
    var b = bands[peqSelectedBand];
    if (!b) { el.innerHTML = ''; return; }
    var t = b.type;
    var hasGain = (t === 4 || t === 5 || t === 6);
    var html = '<div class="peq-detail-panel" style="border-left:3px solid ' + PEQ_COLORS[peqSelectedBand] + ';">';
    html += '<div style="display:flex;align-items:center;gap:8px;margin-bottom:8px;">';
    html += '<label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (b.enabled ? 'checked' : '') + ' onchange="peqSetBandEnabled(' + peqSelectedBand + ',this.checked)"><span class="slider round"></span></label>';
    html += '<select class="select-sm" onchange="peqSetBandType(' + peqSelectedBand + ',parseInt(this.value))">';
    for (var fi = 0; fi < PEQ_FILTER_TYPES.length; fi++) {
        var ft = PEQ_FILTER_TYPES[fi];
        html += '<option value="' + ft.value + '"' + (t === ft.value ? ' selected' : '') + '>' + ft.label + '</option>';
    }
    html += '</select>';
    html += '<button class="btn btn-secondary" style="padding:2px 8px;font-size:11px;margin-left:auto;" onclick="peqResetBand(' + peqSelectedBand + ')">Reset</button>';
    html += '<span style="font-size:11px;color:var(--text-secondary);">Band ' + (peqSelectedBand + 1) + '</span>';
    html += '</div>';
    html += peqSlider('freq', 'Frequency', b.freq || 1000, 5, 20000, 1, 'Hz');
    if (hasGain) html += peqSlider('gain', 'Gain', b.gain || 0, -24, 24, 0.5, 'dB');
    html += peqSlider('Q', 'Q Factor', b.Q || 0.707, 0.1, 25, 0.01, '');
    html += '</div>';
    el.innerHTML = html;
}
function peqSlider(key, label, val, min, max, step, unit) {
    var numVal = parseFloat(val) || 0;
    var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
    var id = 'peq_' + peqSelectedBand + '_' + key;
    return '<div class="dsp-param"><label>' + label + '</label>' +
        '<button class="dsp-step-btn" onclick="peqParamStep(\'' + key + '\',' + (-step) + ',' + min + ',' + max + ',' + step + ')">&lsaquo;</button>' +
        '<input type="range" id="' + id + '_s" min="' + min + '" max="' + max + '" step="' + step + '" value="' + numVal + '" ' +
        'oninput="document.getElementById(\'' + id + '_n\').value=parseFloat(this.value).toFixed(' + dec + ')" ' +
        'onchange="peqParamSync(\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
        '<button class="dsp-step-btn" onclick="peqParamStep(\'' + key + '\',' + step + ',' + min + ',' + max + ',' + step + ')">&rsaquo;</button>' +
        '<input type="number" class="dsp-num-input" id="' + id + '_n" value="' + numVal.toFixed(dec) + '" min="' + min + '" max="' + max + '" step="' + step + '" ' +
        'onchange="peqParamSync(\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
        '<span class="dsp-unit">' + unit + '</span></div>';
}
function peqParamSync(key, val, min, max, step) {
    val = Math.min(max, Math.max(min, parseFloat(val) || 0));
    peqUpdateBandParam(peqSelectedBand, key, val);
}
function peqParamStep(key, delta, min, max, step) {
    var id = 'peq_' + peqSelectedBand + '_' + key;
    var sl = document.getElementById(id + '_s');
    var cur = sl ? parseFloat(sl.value) : 0;
    var newVal = Math.min(max, Math.max(min, cur + delta));
    var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
    if (sl) sl.value = newVal;
    var ni = document.getElementById(id + '_n');
    if (ni) ni.value = newVal.toFixed(dec);
    peqUpdateBandParam(peqSelectedBand, key, newVal);
}
function peqUpdateBandParam(band, key, val) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    var bands = peqGetBands();
    var b = bands[band];
    if (!b) return;
    var msg = { type: 'updatePeqBand', ch: dspCh, band: band,
        freq: b.freq || 1000, gain: b.gain || 0, Q: b.Q || 0.707,
        filterType: b.type, enabled: b.enabled };
    if (key === 'filterType') msg.filterType = val;
    else msg[key] = val;
    ws.send(JSON.stringify(msg));
    if (peqLinked) {
        ws.send(JSON.stringify(Object.assign({}, msg, { ch: dspCh ^ 1 })));
    }
}
function peqSetBandEnabled(band, en) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ type: 'setPeqBandEnabled', ch: dspCh, band: band, enabled: en }));
    if (peqLinked) ws.send(JSON.stringify({ type: 'setPeqBandEnabled', ch: dspCh ^ 1, band: band, enabled: en }));
}
function peqSetBandType(band, typeInt) {
    peqUpdateBandParam(band, 'filterType', typeInt);
}
// Equal logarithmic spacing from 20 Hz to 20 kHz (10 bands)
const PEQ_DEFAULT_FREQS = [20,43,93,200,430,930,2000,4300,9300,20000];
function peqResetBand(band) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    var msg = { type: 'updatePeqBand', ch: dspCh, band: band,
        freq: PEQ_DEFAULT_FREQS[band] || 1000, gain: 0, Q: 1.0, filterType: 4, enabled: true };
    ws.send(JSON.stringify(msg));
    if (peqLinked) ws.send(JSON.stringify(Object.assign({}, msg, { ch: dspCh ^ 1 })));
}
function peqToggleLink() {
    peqLinked = !peqLinked;
    var btn = document.getElementById('peqLinkBtn');
    if (btn) {
        btn.classList.toggle('active', peqLinked);
        btn.style.background = peqLinked ? 'var(--accent)' : '';
        btn.style.color = peqLinked ? '#fff' : '';
    }
}
function peqToggleAll() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    var bands = peqGetBands();
    var allEnabled = bands.length > 0 && bands.every(function(b) { return b && b.enabled; });
    var en = !allEnabled;
    ws.send(JSON.stringify({ type: 'setPeqAllEnabled', ch: dspCh, enabled: en }));
    if (peqLinked) ws.send(JSON.stringify({ type: 'setPeqAllEnabled', ch: dspCh ^ 1, enabled: en }));
}
function peqUpdateToggleAllBtn() {
    var btn = document.getElementById('peqToggleAllBtn');
    if (!btn) return;
    var bands = peqGetBands();
    var allEnabled = bands.length > 0 && bands.every(function(b) { return b && b.enabled; });
    btn.textContent = allEnabled ? 'Disable All' : 'Enable All';
}
function peqCopyChannel(target) {
    if (!target || !ws || ws.readyState !== WebSocket.OPEN) return;
    if (target === 'all') {
        for (var c = 0; c < DSP_MAX_CH; c++) {
            if (c !== dspCh) ws.send(JSON.stringify({ type: 'copyPeqChannel', from: dspCh, to: c }));
        }
    } else {
        ws.send(JSON.stringify({ type: 'copyPeqChannel', from: dspCh, to: parseInt(target) }));
    }
}
function dspCopyChainChannel(target) {
    if (!target || !ws || ws.readyState !== WebSocket.OPEN) return;
    if (target === 'all') {
        for (var c = 0; c < DSP_MAX_CH; c++) {
            if (c !== dspCh) ws.send(JSON.stringify({ type: 'copyChainStages', from: dspCh, to: c }));
        }
    } else {
        ws.send(JSON.stringify({ type: 'copyChainStages', from: dspCh, to: parseInt(target) }));
    }
}
function updateChainCopyToDropdown() {
    var sel = document.getElementById('chainCopyTo');
    if (!sel) return;
    var html = '<option value="">Copy to...</option>';
    for (var i = 0; i < DSP_MAX_CH; i++) {
        var name = inputNames[i] || DSP_CH_NAMES[i];
        html += '<option value="' + i + '">' + name + '</option>';
    }
    html += '<option value="all">All Channels</option>';
    sel.innerHTML = html;
}
function peqPresetAction(val) {
    if (!val) return;
    if (val === '_save') {
        var name = prompt('Preset name (max 20 chars):');
        if (!name) return;
        if (ws && ws.readyState === WebSocket.OPEN)
            ws.send(JSON.stringify({ type: 'savePeqPreset', ch: dspCh, name: name.substring(0, 20) }));
    } else if (val === '_load') {
        if (ws && ws.readyState === WebSocket.OPEN)
            ws.send(JSON.stringify({ type: 'listPeqPresets' }));
    } else {
        if (ws && ws.readyState === WebSocket.OPEN)
            ws.send(JSON.stringify({ type: 'loadPeqPreset', ch: dspCh, name: val }));
    }
}
function peqToggleGraphLayer(layer) {
    peqGraphLayers[layer] = !peqGraphLayers[layer];
    var btn = document.getElementById('tog' + layer.charAt(0).toUpperCase() + layer.slice(1));
    if (btn) btn.classList.toggle('active', peqGraphLayers[layer]);
    if (layer === 'rta' && ws && ws.readyState === WebSocket.OPEN) {
        if (peqGraphLayers.rta) {
            ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
            ws.send(JSON.stringify({ type: 'setSpectrumEnabled', enabled: true }));
        } else if (!audioSubscribed) {
            ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
        }
    }
    dspDrawFreqResponse();
}
function peqHandlePresetsList(presets) {
    var sel = document.getElementById('peqPresetSel');
    if (!sel) return;
    var html = '<option value="">Presets...</option><option value="_save">Save Preset...</option><option value="_load">Refresh List</option>';
    if (presets && presets.length > 0) {
        html += '<option disabled>──────────</option>';
        for (var i = 0; i < presets.length; i++) {
            html += '<option value="' + presets[i] + '">' + presets[i] + '</option>';
        }
    }
    sel.innerHTML = html;
    showToast('Found ' + (presets ? presets.length : 0) + ' presets');
}

// ===== PEQ Canvas Interaction (drag control points) =====
function peqInitCanvas() {
    if (peqCanvasInited) return;
    var canvas = document.getElementById('dspFreqCanvas');
    if (!canvas) return;
    peqCanvasInited = true;
    canvas.addEventListener('mousedown', peqCanvasMouseDown);
    canvas.addEventListener('mousemove', peqCanvasMouseMove);
    canvas.addEventListener('mouseup', peqCanvasMouseUp);
    canvas.addEventListener('mouseleave', peqCanvasMouseUp);
    canvas.addEventListener('touchstart', peqCanvasTouchStart, { passive: false });
    canvas.addEventListener('touchmove', peqCanvasTouchMove, { passive: false });
    canvas.addEventListener('touchend', peqCanvasMouseUp);
}
function peqCanvasCoords(canvas, clientX, clientY) {
    var rect = canvas.getBoundingClientRect();
    return { x: clientX - rect.left, y: clientY - rect.top };
}
function peqCanvasToFreqGain(canvas, x, y) {
    var dpr = window.devicePixelRatio || 1;
    var dims = canvasDims[canvas.id];
    if (!dims) return null;
    var w = dims.tw / dpr, h = dims.th / dpr;
    var padL = 35, padR = 10, padT = 10, padB = 20;
    var gw = w - padL - padR, gh = h - padT - padB;
    var logMin = Math.log10(5), logRange = Math.log10(24000) - logMin;
    var normX = (x - padL) / gw;
    var normY = (y - padT) / gh;
    var freq = Math.pow(10, logMin + logRange * normX);
    var gain = 24 - normY * 48;
    return { freq: Math.max(5, Math.min(20000, Math.round(freq))), gain: Math.max(-24, Math.min(24, Math.round(gain * 2) / 2)) };
}
function peqFreqGainToCanvas(canvas, freq, gain) {
    var dpr = window.devicePixelRatio || 1;
    var dims = canvasDims[canvas.id];
    if (!dims) return null;
    var w = dims.tw / dpr, h = dims.th / dpr;
    var padL = 35, padR = 10, padT = 10, padB = 20;
    var gw = w - padL - padR, gh = h - padT - padB;
    var logMin = Math.log10(5), logRange = Math.log10(24000) - logMin;
    return {
        x: padL + gw * (Math.log10(freq) - logMin) / logRange,
        y: padT + gh * (1 - (gain + 24) / 48)
    };
}
function peqFindNearestBand(canvas, x, y, maxDist) {
    var bands = peqGetBands();
    var closest = -1, closestDist = (maxDist !== undefined && maxDist !== null) ? maxDist : 25;
    for (var i = 0; i < bands.length; i++) {
        var b = bands[i];
        var pos = peqFreqGainToCanvas(canvas, b.freq || 1000, b.gain || 0);
        if (!pos) continue;
        var dist = Math.sqrt(Math.pow(x - pos.x, 2) + Math.pow(y - pos.y, 2));
        if (dist < closestDist) { closestDist = dist; closest = i; }
    }
    return closest;
}
function peqCanvasMouseDown(e) {
    var canvas = e.target;
    var pos = peqCanvasCoords(canvas, e.clientX, e.clientY);
    var band = peqFindNearestBand(canvas, pos.x, pos.y, PEQ_MOUSE_HIT_RADIUS);
    if (band >= 0) {
        peqSelectBand(band);
        peqDragging = { band: band };
        canvas.style.cursor = 'grabbing';
        e.preventDefault();
    } else {
        var nearest = peqFindNearestBand(canvas, pos.x, pos.y, Infinity);
        if (nearest >= 0) peqSelectBand(nearest);
    }
}
function peqCanvasMouseMove(e) {
    var canvas = e.target;
    var pos = peqCanvasCoords(canvas, e.clientX, e.clientY);
    if (!peqDragging) {
        var hover = peqFindNearestBand(canvas, pos.x, pos.y, PEQ_MOUSE_HIT_RADIUS);
        canvas.style.cursor = hover >= 0 ? 'grab' : 'crosshair';
        return;
    }
    var fg = peqCanvasToFreqGain(canvas, pos.x, pos.y);
    if (!fg) return;
    var b = peqDragging.band;
    var bands = peqGetBands();
    if (bands[b]) {
        bands[b].freq = fg.freq;
        bands[b].gain = fg.gain;
    }
    var freqSl = document.getElementById('peq_' + b + '_freq_s');
    var freqNi = document.getElementById('peq_' + b + '_freq_n');
    var gainSl = document.getElementById('peq_' + b + '_gain_s');
    var gainNi = document.getElementById('peq_' + b + '_gain_n');
    if (freqSl) freqSl.value = fg.freq;
    if (freqNi) freqNi.value = fg.freq;
    if (gainSl) gainSl.value = fg.gain.toFixed(1);
    if (gainNi) gainNi.value = fg.gain.toFixed(1);
    dspDrawFreqResponse();
    e.preventDefault();
}
function peqCanvasMouseUp(e) {
    if (!peqDragging) return;
    var canvas = e.target || document.getElementById('dspFreqCanvas');
    canvas.style.cursor = 'crosshair';
    var b = peqDragging.band;
    peqDragging = null;
    var bands = peqGetBands();
    if (bands[b]) {
        peqUpdateBandParam(b, 'freq', bands[b].freq);
        setTimeout(function() { peqUpdateBandParam(b, 'gain', bands[b].gain); }, 10);
    }
}
function peqCanvasTouchStart(e) {
    // Two-finger pinch: capture start state
    if (e.touches.length === 2) {
        var dx = e.touches[0].clientX - e.touches[1].clientX;
        var dy = e.touches[0].clientY - e.touches[1].clientY;
        _peqPinchStartDist = Math.sqrt(dx * dx + dy * dy);
        _peqPinchStartZoom = { fMin: peqCanvasZoom.fMin, fMax: peqCanvasZoom.fMax };
        e.preventDefault();
        return;
    }
    if (e.touches.length !== 1) return;
    var canvas = e.target;
    var touch = e.touches[0];
    var pos = peqCanvasCoords(canvas, touch.clientX, touch.clientY);
    var band = peqFindNearestBand(canvas, pos.x, pos.y, PEQ_TOUCH_HIT_RADIUS);
    if (band >= 0) {
        peqSelectBand(band);
        peqDragging = { band: band };
        e.preventDefault();
    }
}
function peqCanvasTouchMove(e) {
    // Two-finger pinch: scale zoom window
    if (e.touches.length === 2 && _peqPinchStartZoom) {
        var dx = e.touches[0].clientX - e.touches[1].clientX;
        var dy = e.touches[0].clientY - e.touches[1].clientY;
        var dist = Math.sqrt(dx * dx + dy * dy);
        var scale = _peqPinchStartDist / dist; // pinch in = scale > 1 = zoom in
        var logMin = Math.log10(20), logMax = Math.log10(20000);
        var logRange = (Math.log10(_peqPinchStartZoom.fMax) - Math.log10(_peqPinchStartZoom.fMin)) * scale;
        logRange = Math.max(0.5, Math.min(logMax - logMin, logRange));
        var logCenter = (Math.log10(_peqPinchStartZoom.fMin) + Math.log10(_peqPinchStartZoom.fMax)) / 2;
        peqCanvasZoom.fMin = Math.max(20, Math.pow(10, logCenter - logRange / 2));
        peqCanvasZoom.fMax = Math.min(20000, Math.pow(10, logCenter + logRange / 2));
        dspDrawFreqResponse();
        e.preventDefault();
        return;
    }
    if (!peqDragging || e.touches.length !== 1) return;
    var canvas = e.target;
    var touch = e.touches[0];
    var pos = peqCanvasCoords(canvas, touch.clientX, touch.clientY);
    var fg = peqCanvasToFreqGain(canvas, pos.x, pos.y);
    if (!fg) return;
    var b = peqDragging.band;
    var bands = peqGetBands();
    if (bands[b]) {
        bands[b].freq = fg.freq;
        bands[b].gain = fg.gain;
    }
    var freqSl = document.getElementById('peq_' + b + '_freq_s');
    var freqNi = document.getElementById('peq_' + b + '_freq_n');
    var gainSl = document.getElementById('peq_' + b + '_gain_s');
    var gainNi = document.getElementById('peq_' + b + '_gain_n');
    if (freqSl) freqSl.value = fg.freq;
    if (freqNi) freqNi.value = fg.freq;
    if (gainSl) gainSl.value = fg.gain.toFixed(1);
    if (gainNi) gainNi.value = fg.gain.toFixed(1);
    dspDrawFreqResponse();
    e.preventDefault();
}
function peqResetZoom() {
    peqCanvasZoom = { fMin: 20, fMax: 20000 };
    dspDrawFreqResponse();
}

// ===== Frequency Response Graph (PEQ-aware) =====
function dspBiquadMagDb(coeffs, f, fs) {
    var b0=coeffs[0], b1=coeffs[1], b2=coeffs[2], a1=coeffs[3], a2=coeffs[4];
    var omega = 2 * Math.PI * f / fs;
    var cosW = Math.cos(omega), sinW = Math.sin(omega);
    var cos2W = Math.cos(2*omega), sin2W = Math.sin(2*omega);
    var numR = b0 + b1*cosW + b2*cos2W, numI = -(b1*sinW + b2*sin2W);
    var denR = 1 + a1*cosW + a2*cos2W, denI = -(a1*sinW + a2*sin2W);
    return 10 * Math.log10(Math.max((numR*numR + numI*numI) / (denR*denR + denI*denI), 1e-20));
}

// Client-side biquad coefficient computation (RBJ Audio EQ Cookbook).
// Returns [b0, b1, b2, a1, a2] normalized so a0=1.
// Uses ESP-DSP sign convention: denominator is 1 + a1*z^-1 + a2*z^-2,
// processing uses y = b0*x + d0; d0 = b1*x - a1*y + d1; d1 = b2*x - a2*y.
function dspComputeCoeffs(type, freq, gain, Q, fs) {
    var fn = freq / fs;
    if (fn < 0.0001) fn = 0.0001;
    if (fn > 0.4999) fn = 0.4999;
    var w0 = 2 * Math.PI * fn;
    var cosW = Math.cos(w0), sinW = Math.sin(w0);
    if (Q <= 0) Q = 0.707;
    var alpha = sinW / (2 * Q);
    var A, b0, b1, b2, a0, a1, a2;

    switch (type) {
        case 0: // LPF
            b1 = 1 - cosW; b0 = b1 / 2; b2 = b0;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 1: // HPF
            b1 = -(1 + cosW); b0 = -b1 / 2; b2 = b0;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 2: // BPF
            b0 = sinW / 2; b1 = 0; b2 = -b0;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 3: // Notch
            b0 = 1; b1 = -2 * cosW; b2 = 1;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 4: // PEQ
            A = Math.pow(10, gain / 40);
            b0 = 1 + alpha * A; b1 = -2 * cosW; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cosW; a2 = 1 - alpha / A;
            break;
        case 5: // Low Shelf
            A = Math.pow(10, gain / 40);
            var sq = 2 * Math.sqrt(A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cosW + sq);
            b1 = 2 * A * ((A - 1) - (A + 1) * cosW);
            b2 = A * ((A + 1) - (A - 1) * cosW - sq);
            a0 = (A + 1) + (A - 1) * cosW + sq;
            a1 = -2 * ((A - 1) + (A + 1) * cosW);
            a2 = (A + 1) + (A - 1) * cosW - sq;
            break;
        case 6: // High Shelf
            A = Math.pow(10, gain / 40);
            var sq = 2 * Math.sqrt(A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cosW + sq);
            b1 = -2 * A * ((A - 1) + (A + 1) * cosW);
            b2 = A * ((A + 1) + (A - 1) * cosW - sq);
            a0 = (A + 1) - (A - 1) * cosW + sq;
            a1 = 2 * ((A - 1) - (A + 1) * cosW);
            a2 = (A + 1) - (A - 1) * cosW - sq;
            break;
        case 7: case 8: // Allpass / AP360
            b0 = 1 - alpha; b1 = -2 * cosW; b2 = 1 + alpha;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 9: // AP180
            b0 = -(1 - alpha); b1 = 2 * cosW; b2 = -(1 + alpha);
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 10: // BPF 0dB
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha;
            break;
        case 19: // LPF 1st order
            var wt = Math.tan(Math.PI * fn);
            var n = 1 / (1 + wt);
            return [wt * n, wt * n, 0, (wt - 1) * n, 0];
        case 20: // HPF 1st order
            var wt = Math.tan(Math.PI * fn);
            var n = 1 / (1 + wt);
            return [n, -n, 0, (wt - 1) * n, 0];
        default: // passthrough
            return [1, 0, 0, 0, 0];
    }
    // Normalize by a0
    var inv = 1 / a0;
    return [b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv];
}

// Compute magnitude in dB from stage parameters (no server coefficients needed)
function dspStageMagDb(s, f, fs) {
    var coeffs = dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
    return dspBiquadMagDb(coeffs, f, fs);
}

function dspDrawFreqResponse() {
    var canvas = document.getElementById('dspFreqCanvas');
    if (!canvas || !dspState || currentActiveTab !== 'dsp') return;
    peqInitCanvas();
    var ctx = canvas.getContext('2d');
    var resized = resizeCanvasIfNeeded(canvas);
    if (resized === -1) return;
    var dims = canvasDims[canvas.id];
    var w = dims.tw, h = dims.th;
    var dpr = window.devicePixelRatio;

    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-card').trim();
    ctx.fillRect(0, 0, w, h);

    // Compare mode — draw all channels and return
    if (typeof dspCompareMode !== 'undefined' && dspCompareMode) {
        var _sr = (dspState && dspState.sampleRate) ? dspState.sampleRate : 48000;
        dspCompareDrawAllChannels(canvas, ctx, w, h, _sr);
        return;
    }

    var padL = 35 * dpr, padR = 10 * dpr, padT = 10 * dpr, padB = 20 * dpr;
    var gw = w - padL - padR, gh = h - padT - padB;
    var yMin = -24, yMax = 24;
    var fMin = (typeof peqCanvasZoom !== 'undefined') ? peqCanvasZoom.fMin : 20;
    var fMax = (typeof peqCanvasZoom !== 'undefined') ? peqCanvasZoom.fMax : 20000;
    var logMin = Math.log10(fMin), logRange = Math.log10(fMax) - logMin;

    // Grid
    ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--border').trim();
    ctx.lineWidth = 0.5 * dpr;
    ctx.font = (9 * dpr) + 'px sans-serif';
    ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--text-disabled').trim();
    for (var db = yMin; db <= yMax; db += 6) {
        var y = padT + gh * (1 - (db - yMin) / (yMax - yMin));
        ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(w - padR, y); ctx.stroke();
        ctx.textAlign = 'right';
        ctx.fillText(db + '', padL - 4 * dpr, y + 3 * dpr);
    }
    ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--text-secondary').trim();
    ctx.lineWidth = 1 * dpr;
    var y0 = padT + gh * (1 - (0 - yMin) / (yMax - yMin));
    ctx.beginPath(); ctx.moveTo(padL, y0); ctx.lineTo(w - padR, y0); ctx.stroke();
    ctx.strokeStyle = getComputedStyle(document.body).getPropertyValue('--border').trim();
    ctx.lineWidth = 0.5 * dpr;
    ctx.textAlign = 'center';
    var freqs = [5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
    var labels = ['5','10','20','50','100','200','500','1k','2k','5k','10k','20k'];
    for (var fi = 0; fi < freqs.length; fi++) {
        var x = padL + gw * (Math.log10(freqs[fi]) - logMin) / logRange;
        ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, padT + gh); ctx.stroke();
        ctx.fillText(labels[fi], x, h - 2 * dpr);
    }

    var ch = dspState.channels[dspCh];
    if (!ch || !ch.stages) return;
    var fs = dspState.sampleRate || 48000;
    var nPts = 256;
    var stages = ch.stages || [];

    // Layer 1: RTA overlay (dBFS spectrum mapped to full graph height)
    if (peqGraphLayers.rta && peqRtaData && peqRtaData.length >= 16) {
        var BAND_EDGES = [0, 60, 150, 250, 400, 600, 1000, 2500, 4000, 6000, 8000, 10000, 12000, 14000, 16000, 20000, 24000];
        var rtaFloor = -96, rtaCeil = 0;
        ctx.beginPath();
        ctx.moveTo(padL, padT + gh);
        for (var bi = 0; bi < 16; bi++) {
            var midF = Math.max(fMin, (BAND_EDGES[bi] + BAND_EDGES[bi+1]) / 2);
            var xb = padL + gw * (Math.log10(midF) - logMin) / logRange;
            var rtaNorm = Math.max(0, Math.min(1, (peqRtaData[bi] - rtaFloor) / (rtaCeil - rtaFloor)));
            var rtaY = padT + gh * (1 - rtaNorm);
            ctx.lineTo(xb, rtaY);
        }
        ctx.lineTo(w - padR, padT + gh);
        ctx.closePath();
        ctx.fillStyle = 'rgba(76,175,80,0.12)';
        ctx.fill();
        // RTA line stroke
        ctx.beginPath();
        for (var bi = 0; bi < 16; bi++) {
            var midF = Math.max(fMin, (BAND_EDGES[bi] + BAND_EDGES[bi+1]) / 2);
            var xb = padL + gw * (Math.log10(midF) - logMin) / logRange;
            var rtaNorm = Math.max(0, Math.min(1, (peqRtaData[bi] - rtaFloor) / (rtaCeil - rtaFloor)));
            var rtaY = padT + gh * (1 - rtaNorm);
            if (bi === 0) ctx.moveTo(xb, rtaY); else ctx.lineTo(xb, rtaY);
        }
        ctx.strokeStyle = 'rgba(76,175,80,0.5)';
        ctx.lineWidth = 1.5 * dpr;
        ctx.stroke();
    }

    // Separate PEQ bands (0-9) from chain stages (10+)
    var peqBands = stages.slice(0, DSP_PEQ_BANDS);
    var chainStages = stages.slice(DSP_PEQ_BANDS);
    var peqCombined = new Float32Array(nPts);
    var chainCombined = new Float32Array(nPts);
    var hasPeq = false, hasChain = false;

    // Layer 2: Individual PEQ band curves (client-side coefficient computation)
    for (var si = 0; si < peqBands.length; si++) {
        var s = peqBands[si];
        if (!dspIsBiquad(s.type) || !s.enabled) continue;
        var peqCoeffs = dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
        hasPeq = true;
        if (peqGraphLayers.individual) {
            ctx.beginPath();
            ctx.strokeStyle = PEQ_COLORS[si] + '40';
            ctx.lineWidth = 1 * dpr;
        }
        for (var p = 0; p < nPts; p++) {
            var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
            var magDb = dspBiquadMagDb(peqCoeffs, f, fs);
            peqCombined[p] += magDb;
            if (peqGraphLayers.individual) {
                var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                var xp = padL + gw * p / (nPts - 1);
                if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
            }
        }
        if (peqGraphLayers.individual) ctx.stroke();
    }

    // Layer 3: Chain biquad curves (use server coefficients, fallback to client-side)
    for (var si = 0; si < chainStages.length; si++) {
        var s = chainStages[si];
        if (!dspIsBiquad(s.type) || !s.enabled) continue;
        var chainCoeffs = s.coeffs || dspComputeCoeffs(s.type, s.freq || 1000, s.gain || 0, s.Q || 0.707, fs);
        hasChain = true;
        if (peqGraphLayers.chain) {
            ctx.beginPath();
            ctx.strokeStyle = 'rgba(180,180,180,0.15)';
            ctx.lineWidth = 1 * dpr;
        }
        for (var p = 0; p < nPts; p++) {
            var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
            var magDb = dspBiquadMagDb(chainCoeffs, f, fs);
            chainCombined[p] += magDb;
            if (peqGraphLayers.chain) {
                var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                var xp = padL + gw * p / (nPts - 1);
                if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
            }
        }
        if (peqGraphLayers.chain) ctx.stroke();
    }

    // Layer 4: Combined PEQ response (orange dashed)
    if (hasPeq) {
        ctx.beginPath();
        ctx.strokeStyle = '#FF9800';
        ctx.lineWidth = 1.5 * dpr;
        ctx.setLineDash([4*dpr, 4*dpr]);
        for (var p = 0; p < nPts; p++) {
            var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, peqCombined[p])) - yMin) / (yMax - yMin));
            var xp = padL + gw * p / (nPts - 1);
            if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
        }
        ctx.stroke();
        ctx.setLineDash([]);
    }

    // Layer 5: Combined total response (PEQ + chain, orange solid)
    if (hasPeq || hasChain) {
        ctx.beginPath();
        ctx.strokeStyle = '#FF9800';
        ctx.lineWidth = 2.5 * dpr;
        for (var p = 0; p < nPts; p++) {
            var total = peqCombined[p] + chainCombined[p];
            var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, total)) - yMin) / (yMax - yMin));
            var xp = padL + gw * p / (nPts - 1);
            if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
        }
        ctx.stroke();
    }

    // Highlight selected chain stage (if expanded)
    if (dspOpenStage >= DSP_PEQ_BANDS && dspOpenStage < stages.length) {
        var ss = stages[dspOpenStage];
        if (dspIsBiquad(ss.type)) {
            var hlCoeffs = ss.coeffs || dspComputeCoeffs(ss.type, ss.freq || 1000, ss.gain || 0, ss.Q || 0.707, fs);
            ctx.beginPath();
            ctx.strokeStyle = '#FFFFFF';
            ctx.lineWidth = 2 * dpr;
            ctx.setLineDash([4*dpr, 4*dpr]);
            for (var p = 0; p < nPts; p++) {
                var f = Math.pow(10, logMin + logRange * p / (nPts - 1));
                var magDb = dspBiquadMagDb(hlCoeffs, f, fs);
                var yp = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, magDb)) - yMin) / (yMax - yMin));
                var xp = padL + gw * p / (nPts - 1);
                if (p === 0) ctx.moveTo(xp, yp); else ctx.lineTo(xp, yp);
            }
            ctx.stroke();
            ctx.setLineDash([]);
        }
    }

    // Layer 6: PEQ control point circles with band numbers
    for (var i = 0; i < peqBands.length; i++) {
        var b = peqBands[i];
        var freq = b.freq || 1000, gain = b.gain || 0;
        var cx = padL + gw * (Math.log10(freq) - logMin) / logRange;
        var cy = padT + gh * (1 - (Math.max(yMin, Math.min(yMax, gain)) - yMin) / (yMax - yMin));
        var isSelected = (i === peqSelectedBand);
        var radius = isSelected ? 11 * dpr : 9 * dpr;
        ctx.beginPath();
        ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
        if (b.enabled) {
            ctx.fillStyle = PEQ_COLORS[i];
            ctx.fill();
        } else {
            ctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--bg-card').trim();
            ctx.fill();
            ctx.strokeStyle = PEQ_COLORS[i];
            ctx.lineWidth = 1.5 * dpr;
            ctx.stroke();
        }
        // Band number label inside dot
        ctx.font = 'bold ' + (isSelected ? 11 * dpr : 10 * dpr) + 'px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillStyle = b.enabled ? '#fff' : PEQ_COLORS[i];
        ctx.fillText('' + (i + 1), cx, cy + 0.5 * dpr);
        if (isSelected) {
            ctx.beginPath();
            ctx.arc(cx, cy, radius + 3 * dpr, 0, 2 * Math.PI);
            ctx.strokeStyle = '#fff';
            ctx.lineWidth = 1.5 * dpr;
            ctx.stroke();
        }
    }
}
