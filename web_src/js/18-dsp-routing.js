// ===== Pipeline Routing Matrix (8x8) =====
// Replaces old 4x4 DspRoutingMatrix — now driven by audio_pipeline 8x8 matrix.
// Uses GET /api/pipeline/matrix and POST /api/pipeline/matrix/cell endpoints.

let pipelineMatrix = null;
let pipelineInputNames = [];
let pipelineOutputNames = [];
let pipelineMatrixBypass = false;

function dspLoadRouting() {
    fetch('/api/pipeline/matrix')
        .then(function(r) { return r.json(); })
        .then(function(d) {
            pipelineMatrix = d.matrix;
            pipelineInputNames = d.inputs || [];
            pipelineOutputNames = d.outputs || [];
            pipelineMatrixBypass = !!d.bypass;
            dspRenderRouting();
        })
        .catch(function() {});
}

function dspRenderRouting() {
    var el = document.getElementById('dspRoutingGrid');
    if (!el || !pipelineMatrix) return;
    var numOut = pipelineMatrix.length;
    var numIn = numOut > 0 ? pipelineMatrix[0].length : 0;
    if (numOut === 0 || numIn === 0) { el.innerHTML = '<p style="color:var(--text-secondary);">No matrix data</p>'; return; }

    var h = '<table class="routing-matrix-table">';
    // Header row: input names
    h += '<tr><td class="routing-corner"></td>';
    for (var i = 0; i < numIn; i++) {
        var inName = pipelineInputNames[i] || ('In ' + i);
        h += '<td class="routing-header">' + inName + '</td>';
    }
    h += '</tr>';

    // Data rows: one per output
    for (var o = 0; o < numOut; o++) {
        var outName = pipelineOutputNames[o] || ('Out ' + o);
        h += '<tr><td class="routing-row-label">' + outName + '</td>';
        for (var i = 0; i < numIn; i++) {
            var linear = pipelineMatrix[o][i];
            var db = linear <= 0.0001 ? 'Off' : (20 * Math.log10(linear)).toFixed(1);
            var active = linear > 0.001;
            var isUnity = Math.abs(linear - 1.0) < 0.001;
            var cellClass = 'routing-cell' + (active ? ' active' : '') + (isUnity ? ' unity' : '');
            h += '<td class="' + cellClass + '" onclick="dspEditRoutingCell(' + o + ',' + i + ')" title="' + outName + ' \u2190 ' + (pipelineInputNames[i] || 'In ' + i) + '">' + db + '</td>';
        }
        h += '</tr>';
    }
    h += '</table>';
    el.innerHTML = h;
}

function dspEditRoutingCell(o, i) {
    var current = pipelineMatrix && pipelineMatrix[o] ? pipelineMatrix[o][i] : 0;
    var currentDb = current <= 0.0001 ? 'Off' : (20 * Math.log10(current)).toFixed(1);
    var outName = pipelineOutputNames[o] || ('Out ' + o);
    var inName = pipelineInputNames[i] || ('In ' + i);
    var val = prompt('Gain for ' + outName + ' \u2190 ' + inName + ' (dB, or "off" for silence):', currentDb);
    if (val === null) return;
    var lv = val.trim().toLowerCase();
    var gainDb = (lv === 'off' || lv === '-inf' || lv === '') ? -96 : parseFloat(val);
    if (isNaN(gainDb)) return;

    fetch('/api/pipeline/matrix/cell', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ out: o, in: i, gainDb: gainDb })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
        if (d.status === 'ok') {
            // Update local state immediately
            if (pipelineMatrix && pipelineMatrix[o]) {
                pipelineMatrix[o][i] = d.gainLinear;
            }
            dspRenderRouting();
        }
    })
    .catch(function(err) { showToast('Error: ' + err, true); });
}

function dspRoutingPreset(name) {
    // Quick presets for the pipeline matrix
    if (!pipelineMatrix) return;
    var numCh = pipelineMatrix.length;
    var promises = [];

    for (var o = 0; o < numCh; o++) {
        for (var i = 0; i < numCh; i++) {
            var gainDb = -96;
            if (name === 'identity') {
                gainDb = (o === i) ? 0 : -96;
            } else if (name === 'clear') {
                gainDb = -96;
            } else if (name === 'stereo') {
                // Standard stereo: In0→Out0, In1→Out1
                gainDb = (o === i && i < 2) ? 0 : -96;
            }
            if (pipelineMatrix[o][i] !== undefined) {
                var currentLinear = pipelineMatrix[o][i];
                var currentDb = currentLinear <= 0.0001 ? -96 : 20 * Math.log10(currentLinear);
                // Only send changed cells
                if (Math.abs(currentDb - gainDb) > 0.1) {
                    promises.push(fetch('/api/pipeline/matrix/cell', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ out: o, in: i, gainDb: gainDb })
                    }));
                }
            }
        }
    }

    if (promises.length > 0) {
        Promise.all(promises)
            .then(function() { showToast('Routing: ' + name); dspLoadRouting(); })
            .catch(function(err) { showToast('Error: ' + err, true); });
    }
}
