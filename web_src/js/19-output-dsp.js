// ===== Per-Output DSP Panels =====
// Manages per-output mono DSP chains (post-matrix, pre-sink).
// Uses REST API at /api/output/dsp (GET, PUT, POST, DELETE).

let outputDspCh = 0;
let outputDspConfig = null;

const OUTPUT_DSP_STAGE_TYPES = [
    { value: 'PEQ', label: 'PEQ' },
    { value: 'LPF', label: 'LPF' },
    { value: 'HPF', label: 'HPF' },
    { value: 'LOW_SHELF', label: 'Low Shelf' },
    { value: 'HIGH_SHELF', label: 'High Shelf' },
    { value: 'BPF', label: 'BPF' },
    { value: 'NOTCH', label: 'Notch' },
    { value: 'GAIN', label: 'Gain' },
    { value: 'LIMITER', label: 'Limiter' },
    { value: 'COMPRESSOR', label: 'Compressor' },
    { value: 'POLARITY', label: 'Polarity' },
    { value: 'MUTE', label: 'Mute' }
];

function outputDspLoadChannel(ch) {
    if (ch === undefined) ch = outputDspCh;
    fetch('/api/output/dsp?ch=' + ch)
        .then(function(r) { return r.json(); })
        .then(function(d) {
            outputDspConfig = d;
            outputDspCh = ch;
            outputDspRenderTabs();
            outputDspRenderPanel();
        })
        .catch(function() {});
}

function outputDspRenderTabs() {
    var el = document.getElementById('outputDspChannelTabs');
    if (!el) return;
    var numCh = 8;
    var names = pipelineOutputNames.length > 0 ? pipelineOutputNames : [];
    var h = '';
    for (var c = 0; c < numCh; c++) {
        var name = names[c] || ('Out ' + c);
        h += '<button class="dsp-ch-tab' + (c === outputDspCh ? ' active' : '') + '" onclick="outputDspSelectChannel(' + c + ')">' + name + '</button>';
    }
    el.innerHTML = h;
}

function outputDspSelectChannel(ch) {
    outputDspCh = ch;
    outputDspLoadChannel(ch);
}

function outputDspRenderPanel() {
    var el = document.getElementById('outputDspPanel');
    if (!el || !outputDspConfig) return;

    var h = '';
    var bypass = outputDspConfig.bypass;

    // Bypass toggle
    h += '<div class="output-dsp-bypass">';
    h += '<label class="toggle-label"><input type="checkbox" ' + (bypass ? '' : 'checked') + ' onchange="outputDspSetBypass(!this.checked)"> <span>Active</span></label>';
    h += '</div>';

    // Stage list
    var stages = outputDspConfig.stages || [];
    if (stages.length === 0) {
        h += '<p style="color:var(--text-secondary);font-size:12px;">No DSP stages. Add one below.</p>';
    } else {
        for (var i = 0; i < stages.length; i++) {
            var s = stages[i];
            h += '<div class="output-dsp-stage">';
            h += '<input type="checkbox" ' + (s.enabled ? 'checked' : '') + ' onchange="outputDspToggleStage(' + i + ',this.checked)" title="Enable/disable">';
            h += '<span class="stage-type">' + (s.type || '?') + '</span>';
            h += '<span class="stage-label">' + (s.label || '') + '</span>';
            h += '<span class="stage-params">' + outputDspFormatParams(s) + '</span>';
            h += '<button class="btn-icon" onclick="outputDspEditStage(' + i + ')" title="Edit"><svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg></button>';
            h += '<button class="btn-icon" onclick="outputDspRemoveStage(' + i + ')" title="Remove"><svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg></button>';
            h += '</div>';
        }
    }

    // Add stage buttons
    h += '<div class="output-dsp-add-row">';
    h += '<select id="outputDspAddType" style="font-size:12px;padding:4px 8px;border-radius:4px;border:1px solid var(--border);background:var(--bg-surface);color:var(--text-primary);">';
    for (var t = 0; t < OUTPUT_DSP_STAGE_TYPES.length; t++) {
        h += '<option value="' + OUTPUT_DSP_STAGE_TYPES[t].value + '">' + OUTPUT_DSP_STAGE_TYPES[t].label + '</option>';
    }
    h += '</select>';
    h += '<button class="btn btn-secondary" style="font-size:12px;padding:4px 12px;" onclick="outputDspAddStage()">Add Stage</button>';
    h += '</div>';

    // Crossover quick setup
    h += '<div style="margin-top:12px;padding-top:8px;border-top:1px solid var(--border);">';
    h += '<span style="font-size:11px;font-weight:600;color:var(--text-secondary);">Crossover</span>';
    h += '<div class="btn-row" style="margin-top:6px;gap:6px;">';
    h += '<input type="number" id="outputDspXoFreq" value="80" min="20" max="20000" step="1" style="width:70px;font-size:12px;padding:4px;border:1px solid var(--border);border-radius:4px;background:var(--bg-surface);color:var(--text-primary);"> Hz';
    h += '<select id="outputDspXoOrder" style="font-size:12px;padding:4px;border:1px solid var(--border);border-radius:4px;background:var(--bg-surface);color:var(--text-primary);">';
    h += '<option value="2">LR2 (12dB)</option><option value="4" selected>LR4 (24dB)</option><option value="8">LR8 (48dB)</option>';
    h += '</select>';
    h += '<input type="number" id="outputDspXoPairCh" value="0" min="0" max="7" style="width:50px;font-size:12px;padding:4px;border:1px solid var(--border);border-radius:4px;background:var(--bg-surface);color:var(--text-primary);"> pair ch';
    h += '<button class="btn btn-secondary" style="font-size:12px;padding:4px 12px;" onclick="outputDspSetupCrossover()">Apply XO</button>';
    h += '</div></div>';

    el.innerHTML = h;
}

function outputDspFormatParams(s) {
    if (!s) return '';
    var type = s.type || '';
    if (type === 'GAIN') return (s.gainDb !== undefined ? s.gainDb.toFixed(1) + ' dB' : '');
    if (type === 'LIMITER') return (s.thresholdDb !== undefined ? s.thresholdDb.toFixed(1) + ' dB' : '');
    if (type === 'COMPRESSOR') return (s.thresholdDb !== undefined ? s.thresholdDb.toFixed(1) + ' dB ' + (s.ratio || '') + ':1' : '');
    if (type === 'POLARITY') return (s.inverted ? 'Inv' : 'Normal');
    if (type === 'MUTE') return (s.muted ? 'Muted' : 'Unmuted');
    // Biquad types
    if (s.frequency !== undefined) {
        var p = formatFreq(s.frequency);
        if (s.gain !== undefined && s.gain !== 0) p += ' ' + s.gain.toFixed(1) + 'dB';
        if (s.Q !== undefined) p += ' Q' + s.Q.toFixed(2);
        return p;
    }
    return '';
}

function outputDspSetBypass(bp) {
    fetch('/api/output/dsp', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ch: outputDspCh, bypass: bp })
    })
    .then(function(r) { return r.json(); })
    .then(function() { outputDspLoadChannel(); })
    .catch(function(err) { showToast('Error: ' + err, true); });
}

function outputDspToggleStage(idx, enabled) {
    // Use PUT to update the stage enable state
    // For simplicity, reload the full config
    fetch('/api/output/dsp', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ch: outputDspCh, stageIndex: idx, stageEnabled: enabled })
    })
    .then(function() { outputDspLoadChannel(); })
    .catch(function(err) { showToast('Error: ' + err, true); });
}

function outputDspAddStage() {
    var sel = document.getElementById('outputDspAddType');
    if (!sel) return;
    var type = sel.value;
    fetch('/api/output/dsp/stage', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ch: outputDspCh, type: type })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
        if (d.status === 'ok') { showToast('Stage added'); outputDspLoadChannel(); }
        else showToast('Failed: ' + (d.error || ''), true);
    })
    .catch(function(err) { showToast('Error: ' + err, true); });
}

function outputDspRemoveStage(idx) {
    fetch('/api/output/dsp/stage', {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ch: outputDspCh, index: idx })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
        if (d.status === 'ok') { showToast('Stage removed'); outputDspLoadChannel(); }
        else showToast('Failed: ' + (d.error || ''), true);
    })
    .catch(function(err) { showToast('Error: ' + err, true); });
}

function outputDspEditStage(idx) {
    if (!outputDspConfig || !outputDspConfig.stages) return;
    var s = outputDspConfig.stages[idx];
    if (!s) return;
    var type = s.type || '';

    // Simple prompt-based editing for now
    if (type === 'GAIN') {
        var val = prompt('Gain (dB):', s.gainDb !== undefined ? s.gainDb : 0);
        if (val === null) return;
        // Would need a stage-update endpoint — for now, rebuild via remove+add
        showToast('Stage editing: use API directly for now');
    } else if (type === 'POLARITY') {
        showToast(s.inverted ? 'Toggling to normal' : 'Toggling to inverted');
    } else if (s.frequency !== undefined) {
        var freq = prompt('Frequency (Hz):', s.frequency);
        if (freq === null) return;
        showToast('Stage editing: use API directly for now');
    } else {
        showToast('Edit not available for this stage type');
    }
}

function outputDspSetupCrossover() {
    var freqEl = document.getElementById('outputDspXoFreq');
    var orderEl = document.getElementById('outputDspXoOrder');
    var pairEl = document.getElementById('outputDspXoPairCh');
    if (!freqEl || !orderEl || !pairEl) return;

    var freqHz = parseFloat(freqEl.value) || 80;
    var order = parseInt(orderEl.value) || 4;
    var pairCh = parseInt(pairEl.value) || 0;

    fetch('/api/output/dsp/crossover', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ subCh: outputDspCh, mainCh: pairCh, freqHz: freqHz, order: order })
    })
    .then(function(r) { return r.json(); })
    .then(function(d) {
        if (d.status === 'ok') {
            showToast('Crossover applied (' + d.stagesAdded + ' stages)');
            outputDspLoadChannel();
        } else {
            showToast('Failed: ' + (d.error || ''), true);
        }
    })
    .catch(function(err) { showToast('Error: ' + err, true); });
}
