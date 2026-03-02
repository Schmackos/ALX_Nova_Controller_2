// 11-input-overview.js — Input Channel Overview Panel

var inputLaneEnabled = [true, true, false, false];
var inputLaneLevels  = [null, null, null, null];

function updateInputOverview() {
    // Update from current VU data (called by audioAnimLoop / WS handler)
    var dots  = ['laneDot0','laneDot1','laneDot2','laneDot3'];
    var levels = ['laneLevel0','laneLevel1','laneLevel2','laneLevel3'];

    for (var i = 0; i < 4; i++) {
        var dot = document.getElementById(dots[i]);
        var lvl = document.getElementById(levels[i]);
        var btn = dot && dot.parentElement.querySelector('button');
        if (!dot) continue;

        var dbVal = inputLaneLevels[i];
        var enabled = inputLaneEnabled[i];

        // Button label
        if (btn) btn.textContent = enabled ? 'Enabled' : 'Enable';

        if (!enabled || dbVal === null) {
            dot.className = 'lane-status-dot';
            if (lvl) lvl.textContent = '—';
        } else {
            var dbNum = typeof dbVal === 'number' ? dbVal : -99;
            if (lvl) lvl.textContent = dbNum > -90 ? dbNum.toFixed(1) + ' dBFS' : '—';
            dot.className = 'lane-status-dot' + (dbNum > -65 ? ' active' : dbNum > -80 ? ' low' : '');
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
    inputLaneLevels[3]  = null; // USB level not exposed yet
    updateInputOverview();
}

function toggleSigGenLane() {
    var newState = !inputLaneEnabled[2];
    wsSend('setSigGenEnabled', { enabled: newState });
}
