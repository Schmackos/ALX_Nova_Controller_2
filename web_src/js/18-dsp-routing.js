// ===== DSP Routing Matrix =====
let dspRouting = null;

function dspRoutingPreset(name) {
    fetch('/api/dsp/routing', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ preset: name })
    })
    .then(r => r.json())
    .then(d => { if (d.success) { showToast('Routing: ' + name); dspLoadRouting(); } })
    .catch(err => showToast('Error: ' + err, true));
}
function dspLoadRouting() {
    fetch('/api/dsp/routing')
        .then(r => r.json())
        .then(d => { dspRouting = d.matrix; dspRenderRouting(); })
        .catch(() => {});
}
function dspRenderRouting() {
    var el = document.getElementById('dspRoutingGrid');
    if (!el || !dspRouting) return;
    var h = '<table style="border-collapse:collapse;font-size:12px;width:100%;">';
    h += '<tr><td></td>';
    for (var i = 0; i < DSP_MAX_CH; i++) h += '<td style="padding:4px;text-align:center;font-weight:600;color:var(--text-secondary);">' + dspChLabel(i) + '</td>';
    h += '</tr>';
    for (var o = 0; o < DSP_MAX_CH; o++) {
        h += '<tr><td style="padding:4px;font-weight:600;color:var(--text-secondary);">' + dspChLabel(o) + '</td>';
        for (var i = 0; i < DSP_MAX_CH; i++) {
            var v = dspRouting[o] ? dspRouting[o][i] : 0;
            var db = v <= 0.0001 ? 'Off' : (20 * Math.log10(v)).toFixed(1);
            var bg = v > 0.001 ? 'rgba(255,152,0,0.15)' : 'transparent';
            h += '<td style="padding:4px;text-align:center;background:' + bg + ';border:1px solid var(--border);cursor:pointer;border-radius:4px;" onclick="dspEditRoutingCell(' + o + ',' + i + ')">' + db + '</td>';
        }
        h += '</tr>';
    }
    h += '</table>';
    el.innerHTML = h;
}
function dspEditRoutingCell(o, i) {
    var current = dspRouting && dspRouting[o] ? dspRouting[o][i] : 0;
    var currentDb = current <= 0.0001 ? 'Off' : (20 * Math.log10(current)).toFixed(1);
    var val = prompt('Gain for ' + dspChLabel(o) + ' <- ' + dspChLabel(i) + ' (dB, or "off" for silence):', currentDb);
    if (val === null) return;
    var lv = val.trim().toLowerCase();
    var gainDb = lv === 'off' || lv === '-inf' || lv === '' ? -200 : parseFloat(val);
    if (isNaN(gainDb)) return;
    fetch('/api/dsp/routing', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ output: o, input: i, gainDb: gainDb })
    })
    .then(r => r.json())
    .then(d => { if (d.success) dspLoadRouting(); })
    .catch(err => showToast('Error: ' + err, true));
}
