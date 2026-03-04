// ===== DSP Stage/Chain Management =====

function dspSetEnabled(en) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'setDspBypass', enabled: en, bypass: dspState ? dspState.dspBypass : false }));
}
function dspSetBypass(bp) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'setDspBypass', enabled: dspState ? dspState.dspEnabled : false, bypass: bp }));
}
function dspSetChBypass(bp) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'setDspChannelBypass', ch: dspCh, bypass: bp }));
}
function dspAddStage(typeInt) {
    document.getElementById('dspAddMenu').classList.remove('open');
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'addDspStage', ch: dspCh, stageType: typeInt }));
}
function dspAddDCBlock() {
    dspToggleAddMenu();
    fetch('/api/dsp/crossover?ch=' + dspCh, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ freq: 10, type: 'bw2', role: 1 })
    })
    .then(r => r.json())
    .then(d => { if (d.success) showToast('DC Block added (10 Hz HPF)'); else showToast('Failed: ' + (d.message || ''), true); })
    .catch(err => showToast('Error: ' + err, true));
}
function dspRemoveStage(idx) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'removeDspStage', ch: dspCh, stage: idx }));
}
function dspMoveStage(from, to) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'reorderDspStage', ch: dspCh, from: from, to: to }));
}
function dspToggleAddMenu() {
    document.getElementById('dspAddMenu').classList.toggle('open');
}
function dspUpdateParam(idx, key, val) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        var msg = { type: 'updateDspStage', ch: dspCh, stage: idx };
        msg[key] = val;
        ws.send(JSON.stringify(msg));
    }
}
function dspToggleStageEnabled(idx, en) {
    dspUpdateParam(idx, 'enabled', en);
}

function dspSelectChannel(ch) {
    dspCh = ch;
    dspOpenStage = -1;
    peqSelectedBand = 0;
    dspRenderChannelTabs();
    peqRenderBandStrip();
    peqRenderBandDetail();
    peqUpdateToggleAllBtn();
    dspRenderStages();
    dspDrawFreqResponse();
}

function dspRenderChannelTabs() {
    var el = document.getElementById('dspChTabs');
    if (!el || !dspState) return;
    var html = '';
    for (var c = 0; c < DSP_MAX_CH; c++) {
        var ch = dspState.channels[c];
        var peqActive = 0, chainCount = 0;
        if (ch && ch.stages) {
            for (var i = 0; i < Math.min(DSP_PEQ_BANDS, ch.stages.length); i++) {
                if (ch.stages[i] && ch.stages[i].enabled) peqActive++;
            }
            chainCount = Math.max(0, (ch.stageCount || 0) - DSP_PEQ_BANDS);
        }
        var badge = peqActive + 'P' + (chainCount > 0 ? ' ' + chainCount + 'C' : '');
        html += '<button class="dsp-ch-tab' + (c === dspCh ? ' active' : '') + '" onclick="dspSelectChannel(' + c + ')">' + dspChLabel(c) + '<span class="badge">' + badge + '</span></button>';
    }
    el.innerHTML = html;
    var byp = document.getElementById('dspChBypassToggle');
    if (byp && dspState.channels[dspCh]) byp.checked = dspState.channels[dspCh].bypass;
}

function dspIsBiquad(t) { return t <= 11 || t === 19 || t === 20 || t === 21; }
function dspStageSummary(s) {
    var t = s.type;
    if (t === 21) return 'F0=' + (s.freq || 50).toFixed(0) + ' Q0=' + (s.Q || 0.707).toFixed(2) + ' Fp=' + (s.gain || 25).toFixed(0) + ' Qp=' + (s.Q2 || 0.5).toFixed(2);
    if (dspIsBiquad(t)) return (s.freq || 1000).toFixed(0) + ' Hz' + (s.gain ? ' ' + (s.gain > 0 ? '+' : '') + s.gain.toFixed(1) + ' dB' : '') + ' Q=' + (s.Q || 0.707).toFixed(2);
    if (t === 12) return s.thresholdDb.toFixed(1) + ' dBFS ' + s.ratio.toFixed(0) + ':1';
    if (t === 13) return s.numTaps + ' taps';
    if (t === 14) return (s.gainDb > 0 ? '+' : '') + s.gainDb.toFixed(1) + ' dB';
    if (t === 15) return s.delaySamples + ' smp';
    if (t === 16) return s.inverted ? 'Inverted' : 'Normal';
    if (t === 17) return s.muted ? 'Muted' : 'Active';
    if (t === 18) return s.thresholdDb.toFixed(1) + ' dBFS ' + s.ratio.toFixed(1) + ':1';
    if (t === 24) return (s.thresholdDb || -40).toFixed(0) + ' dB ' + (s.ratio || 1).toFixed(0) + ':1';
    if (t === 25) return 'B' + (s.bassGain || 0).toFixed(0) + ' M' + (s.midGain || 0).toFixed(0) + ' T' + (s.trebleGain || 0).toFixed(0);
    if (t === 26) return (s.currentTempC || 25).toFixed(0) + '\u00B0C GR=' + (s.gr || 0).toFixed(1);
    if (t === 27) return 'W=' + (s.width || 100).toFixed(0) + '%';
    if (t === 28) return 'Ref=' + (s.referenceLevelDb || 85).toFixed(0) + ' Cur=' + (s.currentLevelDb || 75).toFixed(0);
    if (t === 29) return (s.frequency || 80).toFixed(0) + ' Hz mix=' + (s.mix || 50).toFixed(0) + '%';
    if (t === 30) return (s.numBands || 3) + ' bands';
    return '';
}

function dspParamSliders(idx, s) {
    var t = s.type;
    var h = '';
    if (t === 21) {
        h += dspSlider(idx, 'freq', 'F0 (Speaker Fs)', s.freq || 50, 20, 200, 1, 'Hz');
        h += dspSlider(idx, 'Q', 'Q0 (Speaker Qts)', s.Q || 0.707, 0.1, 2.0, 0.01, '');
        h += dspSlider(idx, 'gain', 'Fp (Target Fs)', s.gain || 25, 10, 100, 1, 'Hz');
        h += dspSlider(idx, 'Q2', 'Qp (Target Qts)', s.Q2 || 0.5, 0.1, 2.0, 0.01, '');
    } else if (t <= 11 || t === 19 || t === 20) {
        h += dspSlider(idx, 'freq', 'Frequency', s.freq || 1000, 5, 20000, 1, 'Hz');
        if (t === 4 || t === 5 || t === 6) h += dspSlider(idx, 'gain', 'Gain', s.gain || 0, -24, 24, 0.5, 'dB');
        if (t !== 19 && t !== 20) h += dspSlider(idx, 'Q', 'Q Factor', s.Q || 0.707, 0.1, 20, 0.01, '');
    } else if (t === 18) {
        var cHook = ';dspDrawCompressorGraph(' + idx + ')';
        h += '<div class="comp-graph-wrap"><canvas id="compCanvas_' + idx + '" height="180"></canvas></div>';
        h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -60, 0, 0.5, 'dBFS', cHook);
        h += dspSlider(idx, 'ratio', 'Ratio', s.ratio, 1, 100, 0.5, ':1', cHook);
        h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
        h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 1000, 1, 'ms');
        h += dspSlider(idx, 'kneeDb', 'Knee', s.kneeDb, 0, 24, 0.5, 'dB', cHook);
        h += dspSlider(idx, 'makeupGainDb', 'Makeup', s.makeupGainDb, 0, 24, 0.5, 'dB', cHook);
        var gr = s.gr !== undefined ? s.gr : 0;
        h += '<div class="comp-gr-wrap"><label>GR</label><div class="comp-gr-track"><div class="comp-gr-fill" id="compGr_' + idx + '" style="width:' + Math.min(100, Math.abs(gr) / 24 * 100).toFixed(1) + '%"></div></div><span class="comp-gr-val" id="compGrVal_' + idx + '">' + gr.toFixed(1) + ' dB</span></div>';
    } else if (t === 12) {
        h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -60, 0, 0.5, 'dBFS');
        h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
        h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 1000, 1, 'ms');
        h += dspSlider(idx, 'ratio', 'Ratio', s.ratio, 1, 100, 0.5, ':1');
        if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
    } else if (t === 14) {
        h += dspSlider(idx, 'gainDb', 'Gain', s.gainDb, -60, 24, 0.5, 'dB');
    } else if (t === 15) {
        h += dspSlider(idx, 'delaySamples', 'Delay', s.delaySamples, 0, 4800, 1, 'smp');
        var ms = (s.delaySamples / (dspState.sampleRate || 48000) * 1000).toFixed(2);
        h += '<div class="dsp-param"><label>Time</label><span class="dsp-val">' + ms + ' ms</span></div>';
    } else if (t === 16) {
        h += '<div class="dsp-param"><label>Invert</label><label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (s.inverted ? 'checked' : '') + ' onchange="dspUpdateParam(' + idx + ',\'inverted\',this.checked)"><span class="slider round"></span></label></div>';
    } else if (t === 17) {
        h += '<div class="dsp-param"><label>Mute</label><label class="switch" style="transform:scale(0.75);"><input type="checkbox" ' + (s.muted ? 'checked' : '') + ' onchange="dspUpdateParam(' + idx + ',\'muted\',this.checked)"><span class="slider round"></span></label></div>';
    } else if (t === 13) {
        h += '<div class="dsp-param"><label>Taps</label><span class="dsp-val">' + (s.numTaps || 0) + '</span></div>';
    } else if (t === 24) {
        h += dspSlider(idx, 'thresholdDb', 'Threshold', s.thresholdDb, -80, 0, 0.5, 'dB');
        h += dspSlider(idx, 'attackMs', 'Attack', s.attackMs, 0.1, 100, 0.1, 'ms');
        h += dspSlider(idx, 'holdMs', 'Hold', s.holdMs, 0, 500, 1, 'ms');
        h += dspSlider(idx, 'releaseMs', 'Release', s.releaseMs, 1, 2000, 1, 'ms');
        h += dspSlider(idx, 'ratio', 'Ratio (1=gate)', s.ratio, 1, 20, 0.5, ':1');
        h += dspSlider(idx, 'rangeDb', 'Range', s.rangeDb, -80, 0, 0.5, 'dB');
        if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
    } else if (t === 25) {
        h += dspSlider(idx, 'bassGain', 'Bass (100 Hz)', s.bassGain, -12, 12, 0.5, 'dB');
        h += dspSlider(idx, 'midGain', 'Mid (1 kHz)', s.midGain, -12, 12, 0.5, 'dB');
        h += dspSlider(idx, 'trebleGain', 'Treble (10 kHz)', s.trebleGain, -12, 12, 0.5, 'dB');
    } else if (t === 26) {
        h += dspSlider(idx, 'powerRatingW', 'Power Rating', s.powerRatingW, 1, 1000, 1, 'W');
        h += dspSlider(idx, 'impedanceOhms', 'Impedance', s.impedanceOhms, 2, 32, 0.5, '\u03A9');
        h += dspSlider(idx, 'thermalTauMs', 'Thermal \u03C4', s.thermalTauMs, 100, 10000, 100, 'ms');
        h += dspSlider(idx, 'excursionLimitMm', 'Xmax', s.excursionLimitMm, 0.5, 30, 0.5, 'mm');
        h += dspSlider(idx, 'driverDiameterMm', 'Driver \u00D8', s.driverDiameterMm, 25, 460, 1, 'mm');
        h += dspSlider(idx, 'maxTempC', 'Max Temp', s.maxTempC, 50, 300, 5, '\u00B0C');
        h += '<div class="dsp-param"><label>Temp</label><span class="dsp-val">' + (s.currentTempC || 25).toFixed(1) + ' \u00B0C</span></div>';
        if (s.gr !== undefined) h += '<div class="dsp-param"><label>GR</label><span class="dsp-val" style="color:var(--error)">' + s.gr.toFixed(1) + ' dB</span></div>';
    } else if (t === 27) {
        h += dspSlider(idx, 'width', 'Width', s.width, 0, 200, 1, '%');
        h += dspSlider(idx, 'centerGainDb', 'Center Gain', s.centerGainDb, -12, 12, 0.5, 'dB');
    } else if (t === 28) {
        h += dspSlider(idx, 'referenceLevelDb', 'Reference Level', s.referenceLevelDb, 60, 100, 1, 'dB SPL');
        h += dspSlider(idx, 'currentLevelDb', 'Current Level', s.currentLevelDb, 20, 100, 1, 'dB SPL');
        h += dspSlider(idx, 'amount', 'Amount', s.amount, 0, 100, 1, '%');
    } else if (t === 29) {
        h += dspSlider(idx, 'frequency', 'Crossover Freq', s.frequency, 20, 200, 1, 'Hz');
        h += dspSlider(idx, 'harmonicGainDb', 'Harmonic Gain', s.harmonicGainDb, -12, 12, 0.5, 'dB');
        h += dspSlider(idx, 'mix', 'Mix', s.mix, 0, 100, 1, '%');
        h += '<div class="dsp-param"><label>Harmonics</label><select class="form-input" style="width:auto;padding:2px 6px;" onchange="dspUpdateParam(' + idx + ',\'order\',parseInt(this.value))">';
        h += '<option value="0"' + (s.order===0?' selected':'') + '>2nd</option>';
        h += '<option value="1"' + (s.order===1?' selected':'') + '>3rd</option>';
        h += '<option value="2"' + (s.order===2?' selected':'') + '>Both</option>';
        h += '</select></div>';
    } else if (t === 30) {
        h += '<div class="dsp-param"><label>Bands</label><select class="form-input" style="width:auto;padding:2px 6px;" onchange="dspUpdateParam(' + idx + ',\'numBands\',parseInt(this.value))">';
        h += '<option value="2"' + (s.numBands===2?' selected':'') + '>2</option>';
        h += '<option value="3"' + ((s.numBands||3)===3?' selected':'') + '>3</option>';
        h += '<option value="4"' + (s.numBands===4?' selected':'') + '>4</option>';
        h += '</select></div>';
    }
    return h;
}

function dspParamSync(idx, key, val, min, max, step) {
    val = Math.min(max, Math.max(min, parseFloat(val) || 0));
    var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
    var id = 'dsp_' + idx + '_' + key;
    var sl = document.getElementById(id + '_s');
    var ni = document.getElementById(id + '_n');
    if (sl) sl.value = val;
    dspDrawCompressorGraph(idx);
    if (ni) ni.value = val.toFixed(dec);
    dspUpdateParam(idx, key, val);
}
function dspParamStep(idx, key, delta, min, max, step) {
    var id = 'dsp_' + idx + '_' + key;
    var sl = document.getElementById(id + '_s');
    var cur = sl ? parseFloat(sl.value) : 0;
    dspParamSync(idx, key, cur + delta, min, max, step);
}
function dspSlider(idx, key, label, val, min, max, step, unit, extraOninput) {
    var numVal = parseFloat(val) || 0;
    var dec = step < 1 ? (step < 0.1 ? 2 : 1) : 0;
    var id = 'dsp_' + idx + '_' + key;
    var oninp = 'document.getElementById(\'' + id + '_n\').value=parseFloat(this.value).toFixed(' + dec + ')' + (extraOninput || '');
    return '<div class="dsp-param"><label>' + label + '</label>' +
        '<button class="dsp-step-btn" onclick="dspParamStep(' + idx + ',\'' + key + '\',' + (-step) + ',' + min + ',' + max + ',' + step + ')" title="Decrease">&lsaquo;</button>' +
        '<input type="range" id="' + id + '_s" min="' + min + '" max="' + max + '" step="' + step + '" value="' + numVal + '" ' +
        'oninput="' + oninp + '" ' +
        'onchange="dspParamSync(' + idx + ',\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
        '<button class="dsp-step-btn" onclick="dspParamStep(' + idx + ',\'' + key + '\',' + step + ',' + min + ',' + max + ',' + step + ')" title="Increase">&rsaquo;</button>' +
        '<input type="number" class="dsp-num-input" id="' + id + '_n" value="' + numVal.toFixed(dec) + '" min="' + min + '" max="' + max + '" step="' + step + '" ' +
        'onchange="dspParamSync(' + idx + ',\'' + key + '\',parseFloat(this.value),' + min + ',' + max + ',' + step + ')">' +
        '<span class="dsp-unit">' + unit + '</span></div>';
}

function dspDrawCompressorGraph(idx) {
    var canvas = document.getElementById('compCanvas_' + idx);
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var getVal = function(key, def) {
        var el = document.getElementById('dsp_' + idx + '_' + key + '_s');
        return el ? parseFloat(el.value) : def;
    };
    var threshold = getVal('thresholdDb', -20);
    var ratio = getVal('ratio', 4);
    var knee = getVal('kneeDb', 6);
    var makeup = getVal('makeupGainDb', 0);
    var dpr = window.devicePixelRatio || 1;
    var rect = canvas.getBoundingClientRect();
    var w = rect.width || 300;
    var h = 180;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    canvas.style.height = h + 'px';
    ctx.scale(dpr, dpr);
    var pad = { l: 34, r: 8, t: 10, b: 22 };
    var gw = w - pad.l - pad.r;
    var gh = h - pad.t - pad.b;
    var dbMin = -60, dbMax = 0;
    var xOf = function(db) { return pad.l + (db - dbMin) / (dbMax - dbMin) * gw; };
    var yOf = function(db) { return pad.t + (1 - (db - dbMin) / (dbMax - dbMin)) * gh; };
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.fillRect(pad.l, pad.t, gw, gh);
    ctx.strokeStyle = 'rgba(255,255,255,0.08)';
    ctx.lineWidth = 0.5;
    var grid = [-48, -36, -24, -12];
    for (var g = 0; g < grid.length; g++) {
        ctx.beginPath(); ctx.moveTo(xOf(grid[g]), pad.t); ctx.lineTo(xOf(grid[g]), pad.t + gh); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(pad.l, yOf(grid[g])); ctx.lineTo(pad.l + gw, yOf(grid[g])); ctx.stroke();
    }
    ctx.fillStyle = 'rgba(255,255,255,0.35)';
    ctx.font = '9px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    for (var g = 0; g < grid.length; g++) ctx.fillText(grid[g], xOf(grid[g]), pad.t + gh + 4);
    ctx.fillText('0', xOf(0), pad.t + gh + 4);
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (var g = 0; g < grid.length; g++) ctx.fillText(grid[g], pad.l - 4, yOf(grid[g]));
    ctx.fillText('0', pad.l - 4, yOf(0));
    ctx.setLineDash([4, 4]);
    ctx.strokeStyle = 'rgba(255,255,255,0.2)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(xOf(dbMin), yOf(dbMin)); ctx.lineTo(xOf(dbMax), yOf(dbMax)); ctx.stroke();
    ctx.setLineDash([]);
    ctx.setLineDash([3, 3]);
    ctx.strokeStyle = 'rgba(255,255,255,0.3)';
    ctx.lineWidth = 1;
    if (threshold >= dbMin && threshold <= dbMax) {
        ctx.beginPath(); ctx.moveTo(xOf(threshold), pad.t); ctx.lineTo(xOf(threshold), pad.t + gh); ctx.stroke();
    }
    ctx.setLineDash([]);
    ctx.strokeStyle = '#FF9800';
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    var first = true;
    for (var inDb = dbMin; inDb <= dbMax; inDb += 0.5) {
        var overDb = inDb - threshold;
        var halfK = knee / 2;
        var grDb = 0;
        if (knee > 0 && overDb > -halfK && overDb < halfK) {
            grDb = (1 - 1 / ratio) * (overDb + halfK) * (overDb + halfK) / (2 * knee);
        } else if (overDb >= halfK) {
            grDb = overDb * (1 - 1 / ratio);
        }
        var outDb = inDb - grDb + makeup;
        var py = yOf(Math.max(dbMin, Math.min(dbMax, outDb)));
        if (first) { ctx.moveTo(xOf(inDb), py); first = false; } else ctx.lineTo(xOf(inDb), py);
    }
    ctx.stroke();
    ctx.fillStyle = 'rgba(255,255,255,0.4)';
    ctx.font = '9px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    ctx.fillText('Input (dBFS)', pad.l + gw / 2, h - 1);
    ctx.save();
    ctx.translate(9, pad.t + gh / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textBaseline = 'bottom';
    ctx.fillText('Output', 0, 0);
    ctx.restore();
}

function dspStageColor(t) {
    // Dynamics: Limiter(12), Compressor(18), Noise Gate(24), Multiband Comp(30)
    if (t===12||t===18||t===24||t===30) return '#e6a817';
    // Tone Shaping: Tone Controls(25), Loudness Comp(28), Bass Enhance(29)
    if (t===25||t===28||t===29) return '#43a047';
    // Stereo / Protection: Stereo Width(27), Speaker Protection(26)
    if (t===26||t===27) return '#8e24aa';
    // Utility: FIR(13), Gain(14), Delay(15), Polarity(16), Mute(17)
    if (t>=13&&t<=17) return '#757575';
    // Analysis / Other: Decimator(22), Convolution(23)
    if (t===22||t===23) return '#ef6c00';
    // Crossover / Filters: all EQ/filter types (0-11, 19-21)
    return '#1e88e5';
}

function dspRenderStages() {
    if (!dspState || !dspState.channels[dspCh]) return;
    var ch = dspState.channels[dspCh];
    var list = document.getElementById('dspStageList');
    var title = document.getElementById('dspStageTitleText');
    var chainCount = Math.max(0, (ch.stageCount || 0) - DSP_PEQ_BANDS);
    if (title) title.textContent = 'Additional Processing (' + chainCount + ')';
    if (!list) return;
    var html = '';
    var stages = ch.stages || [];
    for (var i = DSP_PEQ_BANDS; i < stages.length; i++) {
        var s = stages[i];
        var typeName = DSP_TYPES[s.type] || 'Unknown';
        var label = s.label || typeName;
        var open = (i === dspOpenStage);
        html += '<div class="dsp-stage-card' + (!s.enabled ? ' disabled' : '') + '">';
        html += '<div class="dsp-stage-header" onclick="dspOpenStage=' + (open ? -1 : i) + ';dspRenderStages();dspDrawFreqResponse();">';
        html += '<span class="dsp-stage-type" style="background:' + dspStageColor(s.type) + '">' + typeName + '</span>';
        html += '<span class="dsp-stage-name">' + label + '</span>';
        html += '<span class="dsp-stage-info">' + dspStageSummary(s) + '</span>';
        html += '<div class="dsp-stage-actions" onclick="event.stopPropagation()">';
        html += '<label class="switch" style="transform:scale(0.6);margin:0;"><input type="checkbox" ' + (s.enabled ? 'checked' : '') + ' onchange="dspToggleStageEnabled(' + i + ',this.checked)"><span class="slider round"></span></label>';
        if (i > DSP_PEQ_BANDS) html += '<button onclick="dspMoveStage(' + i + ',' + (i-1) + ')" title="Move up"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg></button>';
        if (i < stages.length - 1) html += '<button onclick="dspMoveStage(' + i + ',' + (i+1) + ')" title="Move down"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg></button>';
        html += '<button class="del" onclick="dspRemoveStage(' + i + ')" title="Delete"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg></button>';
        html += '</div></div>';
        html += '<div class="dsp-stage-body' + (open ? ' open' : '') + '">' + (open ? dspParamSliders(i, s) : '') + '</div>';
        html += '</div>';
    }
    list.innerHTML = html;
    if (dspOpenStage >= DSP_PEQ_BANDS && stages[dspOpenStage] && stages[dspOpenStage].type === 18) {
        requestAnimationFrame(function() { dspDrawCompressorGraph(dspOpenStage); });
    }
}

function dspHandleState(d) {
    dspState = d;
    var enTgl = document.getElementById('dspEnableToggle');
    var bpTgl = document.getElementById('dspBypassToggle');
    if (enTgl) enTgl.checked = d.dspEnabled;
    if (bpTgl) bpTgl.checked = d.globalBypass;
    var sr = document.getElementById('dspSampleRate');
    if (sr) sr.textContent = (d.sampleRate || 48000) + ' Hz';
    dspRenderPresetList(d.presets || [], d.presetIndex != null ? d.presetIndex : -1);
    dspRenderChannelTabs();
    peqRenderBandStrip();
    peqRenderBandDetail();
    peqUpdateToggleAllBtn();
    dspRenderStages();
    dspDrawFreqResponse();
}

// ===== DSP Config Presets =====
var _dspLastActivePreset = -1;

function dspRenderPresetList(presets, activeIndex) {
    var list = document.getElementById('dspPresetList');
    var count = document.getElementById('dspPresetCount');
    var status = document.getElementById('dspPresetStatus');
    var saveBtn = document.getElementById('dspSavePresetBtn');
    if (!list) return;

    if (activeIndex >= 0) _dspLastActivePreset = activeIndex;

    var html = '';
    var anyExists = false;
    for (var i = 0; i < presets.length; i++) {
        var p = presets[i];
        if (!p.exists) continue;
        anyExists = true;
        var isActive = (activeIndex === p.index);
        var eName = (p.name || ('Slot ' + (p.index + 1))).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
        html += '<div class="dsp-preset-item' + (isActive ? ' active' : '') + '" onclick="dspLoadPreset(' + p.index + ')">';
        html += '<span class="preset-name">' + eName + '</span>';
        html += '<div class="dsp-stage-actions">';
        html += '<button onclick="event.stopPropagation();dspRenamePresetDialog(' + p.index + ')" title="Rename"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg></button>';
        html += '<button class="del" onclick="event.stopPropagation();dspDeletePresetConfirm(' + p.index + ')" title="Delete"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg></button>';
        html += '</div></div>';
    }
    list.innerHTML = html;
    if (count) count.textContent = presets.filter(function(p){ return p.exists; }).length;

    // Status badge
    if (status) {
        if (activeIndex >= 0) {
            status.textContent = 'Saved';
            status.style.background = 'var(--success)';
            status.style.color = '#fff';
            status.style.display = 'inline';
        } else if (anyExists) {
            status.textContent = 'Modified';
            status.style.background = 'var(--error)';
            status.style.color = '#fff';
            status.style.display = 'inline';
        } else {
            status.style.display = 'none';
        }
    }
    // Save button visible when modified and presets exist
    if (saveBtn) saveBtn.style.display = (activeIndex === -1 && anyExists) ? '' : 'none';
}
function dspLoadPreset(slot) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'loadDspPreset', slot: slot }));
}
function dspSaveCurrentPreset() {
    if (_dspLastActivePreset >= 0) {
        // Overwrite the last active preset
        var presets = dspState && dspState.presets ? dspState.presets : [];
        var preset = presets.find(function(p) { return p.index === _dspLastActivePreset; });
        var name = preset ? preset.name : ('Preset ' + (_dspLastActivePreset + 1));
        if (ws && ws.readyState === WebSocket.OPEN)
            ws.send(JSON.stringify({ type: 'saveDspPreset', slot: _dspLastActivePreset, name: name }));
    } else {
        // No previous preset — act like Add Preset
        dspShowAddPresetDialog();
    }
}
function dspShowAddPresetDialog() {
    var name = prompt('Preset name (max 20 chars):', '');
    if (!name) return;
    name = name.substring(0, 20);
    // Backend will auto-assign next available slot
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'saveDspPreset', slot: -1, name: name }));
}
function dspRenamePresetDialog(slot) {
    var presets = dspState && dspState.presets ? dspState.presets : [];
    var current = presets.find(function(p) { return p.index === slot; });
    var oldName = current ? current.name : '';
    var name = prompt('Rename preset:', oldName);
    if (!name || name === oldName) return;
    name = name.substring(0, 20);
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'renameDspPreset', slot: slot, name: name }));
}
function dspDeletePresetConfirm(slot) {
    var presets = dspState && dspState.presets ? dspState.presets : [];
    var preset = presets.find(function(p) { return p.index === slot; });
    if (!preset) return;
    if (!confirm('Delete preset "' + preset.name + '"?')) return;
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({ type: 'deleteDspPreset', slot: slot }));
}

function dspHandleMetrics(d) {
    var cpuText = document.getElementById('dspCpuText');
    var cpuBar = document.getElementById('dspCpuBar');
    if (cpuText) cpuText.textContent = (d.cpuLoad || 0).toFixed(1) + '%';
    if (cpuBar) cpuBar.style.width = Math.min(d.cpuLoad || 0, 100) + '%';
}

// ===== DSP Import/Export =====
function dspImportApo() {
    dspImportMode = 'apo';
    document.getElementById('dspFileInput').accept = '.txt';
    document.getElementById('dspFileInput').click();
}
function dspImportJson() {
    dspImportMode = 'json';
    document.getElementById('dspFileInput').accept = '.json';
    document.getElementById('dspFileInput').click();
}
function dspHandleFileImport(ev) {
    var file = ev.target.files[0];
    if (!file) return;
    var reader = new FileReader();
    reader.onload = function(e) {
        var url = dspImportMode === 'apo' ? '/api/dsp/import/apo?ch=' + dspCh : '/api/dsp';
        var method = dspImportMode === 'apo' ? 'POST' : 'PUT';
        fetch(url, { method: method, headers: { 'Content-Type': dspImportMode === 'json' ? 'application/json' : 'text/plain' }, body: e.target.result })
            .then(r => r.json())
            .then(d => { if (d.success) showToast('Import successful'); else showToast('Import failed: ' + (d.message || ''), true); })
            .catch(err => showToast('Import error: ' + err, true));
    };
    reader.readAsText(file);
    ev.target.value = '';
}
function dspExportApo() {
    window.open('/api/dsp/export/apo?ch=' + dspCh, '_blank');
}
function dspExportJson() {
    window.open('/api/dsp/export/json', '_blank');
}

// ===== DSP Crossover Presets =====
function dspShowCrossoverModal() {
    dspToggleAddMenu();
    var existing = document.getElementById('crossoverModal');
    if (existing) existing.remove();
    var modal = document.createElement('div');
    modal.id = 'crossoverModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = '<div class="modal">' +
        '<div class="modal-title">Crossover Preset</div>' +
        '<div class="form-group"><label class="form-label">Crossover Frequency (Hz)</label>' +
        '<input type="number" class="form-input" id="modalXoverFreq" value="2000" min="20" max="20000"></div>' +
        '<div class="form-group"><label class="form-label">Slope</label>' +
        '<select class="form-input" id="modalXoverType">' +
        '<optgroup label="Butterworth">' +
        '<option value="bw1">BW1 — 1st order (6 dB/oct)</option>' +
        '<option value="bw2">BW2 — 2nd order (12 dB/oct)</option>' +
        '<option value="bw3">BW3 — 3rd order (18 dB/oct)</option>' +
        '<option value="bw4">BW4 — 4th order (24 dB/oct)</option>' +
        '<option value="bw5">BW5 — 5th order (30 dB/oct)</option>' +
        '<option value="bw6">BW6 — 6th order (36 dB/oct)</option>' +
        '<option value="bw7">BW7 — 7th order (42 dB/oct)</option>' +
        '<option value="bw8">BW8 — 8th order (48 dB/oct)</option>' +
        '<option value="bw9">BW9 — 9th order (54 dB/oct)</option>' +
        '<option value="bw10">BW10 — 10th order (60 dB/oct)</option>' +
        '<option value="bw11">BW11 — 11th order (66 dB/oct)</option>' +
        '<option value="bw12">BW12 — 12th order (72 dB/oct)</option>' +
        '</optgroup>' +
        '<optgroup label="Linkwitz-Riley">' +
        '<option value="lr2">LR2 — 2nd order (12 dB/oct)</option>' +
        '<option value="lr4" selected>LR4 — 4th order (24 dB/oct)</option>' +
        '<option value="lr6">LR6 — 6th order (36 dB/oct)</option>' +
        '<option value="lr8">LR8 — 8th order (48 dB/oct)</option>' +
        '<option value="lr12">LR12 — 12th order (72 dB/oct)</option>' +
        '<option value="lr16">LR16 — 16th order (96 dB/oct)</option>' +
        '<option value="lr24">LR24 — 24th order (144 dB/oct)</option>' +
        '</optgroup>' +
        '<optgroup label="Bessel (flat group delay)">' +
        '<option value="bessel2">Bessel 2nd order (12 dB/oct)</option>' +
        '<option value="bessel4">Bessel 4th order (24 dB/oct)</option>' +
        '<option value="bessel6">Bessel 6th order (36 dB/oct)</option>' +
        '<option value="bessel8">Bessel 8th order (48 dB/oct)</option>' +
        '</optgroup>' +
        '</select></div>' +
        '<div class="form-group"><label class="form-label">Role</label>' +
        '<select class="form-input" id="modalXoverRole">' +
        '<option value="0">Low Pass (LPF)</option>' +
        '<option value="1">High Pass (HPF)</option>' +
        '</select></div>' +
        '<div class="modal-actions"><button class="btn btn-secondary" onclick="closeCrossoverModal()">Cancel</button>' +
        '<button class="btn btn-primary" onclick="dspApplyCrossover()">Apply to Channel</button></div></div>';
    modal.addEventListener('click', function(e) { if (e.target === modal) closeCrossoverModal(); });
    document.body.appendChild(modal);
}
function closeCrossoverModal() {
    var m = document.getElementById('crossoverModal');
    if (m) m.remove();
}
function dspApplyCrossover() {
    var freq = parseInt(document.getElementById('modalXoverFreq').value) || 2000;
    var type = document.getElementById('modalXoverType').value;
    var role = parseInt(document.getElementById('modalXoverRole').value);
    fetch('/api/dsp/crossover?ch=' + dspCh, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ freq: freq, type: type, role: role })
    })
    .then(r => r.json())
    .then(d => { if (d.success) showToast('Crossover applied'); else showToast('Failed: ' + (d.message || ''), true); })
    .catch(err => showToast('Error: ' + err, true));
    closeCrossoverModal();
}

// ===== Baffle Step =====
function dspShowBaffleModal() {
    dspToggleAddMenu();
    var existing = document.getElementById('baffleModal');
    if (existing) existing.remove();
    var modal = document.createElement('div');
    modal.id = 'baffleModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = '<div class="modal">' +
        '<div class="modal-title">Baffle Step Correction</div>' +
        '<div class="form-group"><label class="form-label">Baffle Width (mm)</label>' +
        '<input type="number" class="form-input" id="modalBaffleWidth" value="250" min="50" max="600" step="1"></div>' +
        '<div id="modalBafflePreview" style="font-size:12px;color:var(--text-secondary);margin-bottom:12px;">Estimated: ~437 Hz, +6.0 dB high shelf</div>' +
        '<div class="modal-actions"><button class="btn btn-secondary" onclick="closeBaffleModal()">Cancel</button>' +
        '<button class="btn btn-primary" onclick="applyBaffleStep()">Apply to Channel</button></div></div>';
    modal.addEventListener('click', function(e) { if (e.target === modal) closeBaffleModal(); });
    document.body.appendChild(modal);
    document.getElementById('modalBaffleWidth').addEventListener('input', function() {
        var w = parseInt(this.value) || 250;
        var f = 343000 / (3.14159 * w);
        document.getElementById('modalBafflePreview').textContent = 'Estimated: ~' + f.toFixed(0) + ' Hz, +6.0 dB high shelf';
    });
}
function closeBaffleModal() {
    var m = document.getElementById('baffleModal');
    if (m) m.remove();
}
function applyBaffleStep() {
    var w = parseInt(document.getElementById('modalBaffleWidth').value) || 250;
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({type:'applyBaffleStep', ch:dspCh, baffleWidthMm:w}));
    showToast('Baffle step applied');
    closeBaffleModal();
}

// ===== THD Measurement =====
function dspShowThdModal() {
    dspToggleAddMenu();
    var existing = document.getElementById('thdModal');
    if (existing) existing.remove();
    var modal = document.createElement('div');
    modal.id = 'thdModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = '<div class="modal">' +
        '<div class="modal-title">THD+N Measurement</div>' +
        '<div class="form-group"><label class="form-label">Test Frequency</label>' +
        '<select class="form-input" id="thdFreq"><option value="100">100 Hz</option>' +
        '<option value="1000" selected>1 kHz</option><option value="10000">10 kHz</option></select></div>' +
        '<div class="form-group"><label class="form-label">Averages</label>' +
        '<select class="form-input" id="thdAverages"><option value="4">4 frames</option>' +
        '<option value="8" selected>8 frames</option><option value="16">16 frames</option></select></div>' +
        '<div style="display:flex;gap:8px;margin-bottom:8px;">' +
        '<button class="btn btn-primary" id="thdStartBtn" onclick="thdStart()">Start</button>' +
        '<button class="btn btn-outline" id="thdStopBtn" onclick="thdStop()" style="display:none">Stop</button></div>' +
        '<div id="thdResult" style="display:none;font-size:13px;">' +
        '<div class="info-row"><span class="info-label">THD+N</span><span class="info-value" id="thdPercent">\u2014</span></div>' +
        '<div class="info-row"><span class="info-label">THD+N (dB)</span><span class="info-value" id="thdDb">\u2014</span></div>' +
        '<div class="info-row"><span class="info-label">Fundamental</span><span class="info-value" id="thdFund">\u2014</span></div>' +
        '<div class="info-row"><span class="info-label">Progress</span><span class="info-value" id="thdProgress">\u2014</span></div>' +
        '<div id="thdHarmonics" style="margin-top:8px;">' +
        '<div style="font-weight:600;margin-bottom:4px;font-size:12px;">Harmonics (dB rel. fundamental)</div>' +
        '<div id="thdHarmBars" style="display:flex;gap:2px;height:60px;align-items:flex-end;"></div></div></div>' +
        '<div class="modal-actions" style="margin-top:12px;"><button class="btn btn-secondary" onclick="closeThdModal()">Close</button></div></div>';
    modal.addEventListener('click', function(e) { if (e.target === modal) closeThdModal(); });
    document.body.appendChild(modal);
}
function closeThdModal() {
    thdStop();
    var m = document.getElementById('thdModal');
    if (m) m.remove();
}
function thdStart() {
    var freq = parseInt(document.getElementById('thdFreq').value) || 1000;
    var avg = parseInt(document.getElementById('thdAverages').value) || 8;
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({type:'startThdMeasurement', freq:freq, averages:avg}));
    var sb = document.getElementById('thdStartBtn');
    var stb = document.getElementById('thdStopBtn');
    var tr = document.getElementById('thdResult');
    if (sb) sb.style.display = 'none';
    if (stb) stb.style.display = '';
    if (tr) tr.style.display = '';
}
function thdStop() {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({type:'stopThdMeasurement'}));
    var sb = document.getElementById('thdStartBtn');
    var stb = document.getElementById('thdStopBtn');
    if (sb) sb.style.display = '';
    if (stb) stb.style.display = 'none';
}
function thdUpdateResult(d) {
    if (!d) return;
    var el;
    el = document.getElementById('thdPercent');
    if (el) el.textContent = d.valid ? d.thdPlusNPercent.toFixed(3) + '%' : '\u2014';
    el = document.getElementById('thdDb');
    if (el) el.textContent = d.valid ? d.thdPlusNDb.toFixed(1) + ' dB' : '\u2014';
    el = document.getElementById('thdFund');
    if (el) el.textContent = d.valid ? d.fundamentalDbfs.toFixed(1) + ' dBFS' : '\u2014';
    el = document.getElementById('thdProgress');
    if (el) el.textContent = d.framesProcessed + '/' + d.framesTarget;
    if (d.valid && d.harmonicLevels) {
        var bars = document.getElementById('thdHarmBars');
        if (bars) {
            var html = '';
            for (var h = 0; h < d.harmonicLevels.length; h++) {
                var lev = d.harmonicLevels[h];
                var pct = Math.max(2, Math.min(100, (120 + lev) / 120 * 100));
                html += '<div style="flex:1;display:flex;flex-direction:column;align-items:center;">';
                html += '<div style="width:100%;background:var(--primary);border-radius:2px;height:' + pct + '%" title="' + lev.toFixed(1) + ' dB"></div>';
                html += '<span style="font-size:10px;margin-top:2px;">' + (h+2) + '</span></div>';
            }
            bars.innerHTML = html;
        }
    }
    if (d.valid && !d.measuring) {
        var sb = document.getElementById('thdStartBtn');
        var stb = document.getElementById('thdStopBtn');
        if (sb) sb.style.display = '';
        if (stb) stb.style.display = 'none';
    }
}
