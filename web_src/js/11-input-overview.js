// 11-input-overview.js — Input Channel Overview Panel

var inputLaneEnabled = [true, true, false, false];
var inputLaneLevels  = [null, null, null, null];

function updateInputOverview() {
    var dots   = ['laneDot0','laneDot1','laneDot2','laneDot3'];
    var levels = ['laneLevel0','laneLevel1','laneLevel2','laneLevel3'];

    for (var i = 0; i < 4; i++) {
        var dot = document.getElementById(dots[i]);
        var lvl = document.getElementById(levels[i]);
        if (!dot) continue;

        var dbVal = inputLaneLevels[i];
        var enabled = inputLaneEnabled[i];

        var cb = document.getElementById('laneEnable' + i);
        if (cb) cb.checked = enabled;

        if (!enabled) {
            dot.className = 'lane-status-dot';
            if (lvl) lvl.textContent = '\u2014';
        } else {
            var dbNum = (typeof dbVal === 'number') ? dbVal : -99;
            if (lvl) lvl.textContent = dbNum > -90 ? dbNum.toFixed(1) + ' dBFS' : '\u2014';
            dot.className = 'lane-status-dot' +
                (dbNum > -65 ? ' active' : dbNum > -80 ? ' low' : ' idle');
        }
    }
}

function overviewUpdateAdc(adcIdx, vuL, vuR) {
    // Called by updateLevelMeters() with per-ADC VU values (linear 0-1)
    if (adcIdx < 0 || adcIdx > 1) return;
    var peak = Math.max(vuL || 0, vuR || 0);
    inputLaneLevels[adcIdx] = peak > 0 ? (20 * Math.log10(peak)).toFixed(1) * 1 : -99;
}

function overviewApplyAdcEnabled(adcIdx, enabled) {
    if (adcIdx < 0 || adcIdx > 1) return;
    inputLaneEnabled[adcIdx] = enabled;
    updateInputOverview();
}

function overviewApplySigGenState(d) {
    inputLaneEnabled[2] = !!(d && d.enabled);
    inputLaneLevels[2]  = (d && d.enabled && d.amplitude) ? (20 * Math.log10(d.amplitude / 32767)).toFixed(1) * 1 : null;
    updateInputOverview();
}

function overviewApplyUsbState(d) {
    inputLaneEnabled[3] = !!(d && d.enabled);
    // USB VU level (active when streaming with VU data)
    if (d && d.streaming && d.vuL !== undefined && d.vuL > -90) {
        inputLaneLevels[3] = Math.max(d.vuL, d.vuR);
    } else {
        inputLaneLevels[3] = null;
    }
    updateInputOverview();
}

function toggleSigGenLane(enabled) {
    if (enabled === undefined) enabled = !inputLaneEnabled[2];
    wsSend('setSignalGen', { enabled: !!enabled });
}
