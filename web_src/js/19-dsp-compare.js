// 19-dsp-compare.js — Multi-channel DSP frequency response compare

var dspCompareMode = false;

// Colors for each channel in compare mode
var DSP_COMPARE_COLORS = ['#FF9800', '#2196F3', '#4CAF50', '#E91E63'];

function toggleDspCompare() {
    dspCompareMode = !dspCompareMode;
    var btn = document.getElementById('dspCompareBtn');
    if (btn) {
        btn.classList.toggle('active', dspCompareMode);
        btn.textContent = dspCompareMode ? 'Compare ON' : 'Compare';
    }
    dspDrawFreqResponse();
}

function dspCompareDrawAllChannels(canvas, ctx, w, h, sampleRate) {
    // Draw frequency response for each channel in a distinct color
    if (!dspState || !dspState.channels) return;
    var numCh = dspState.channels.length;
    for (var ch = 0; ch < numCh; ch++) {
        var color = DSP_COMPARE_COLORS[ch % DSP_COMPARE_COLORS.length];
        dspDrawChannelResponse(ctx, w, h, ch, color, 0.7, sampleRate);
    }
    // Draw legend
    dspDrawCompareLegend(ctx, w, h, numCh);
}

function dspDrawChannelResponse(ctx, w, h, ch, color, alpha, sampleRate) {
    // Compute and draw the combined EQ+chain response for a single channel
    var stages = dspState.channels[ch] && dspState.channels[ch].stages;
    if (!stages) return;
    var nPoints = w;
    var fMin = 20, fMax = 20000;
    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    var firstPoint = true;
    for (var i = 0; i < nPoints; i++) {
        var t = i / (nPoints - 1);
        var freq = fMin * Math.pow(fMax / fMin, t);
        var magDb = 0;
        for (var s = 0; s < stages.length; s++) {
            var stage = stages[s];
            if (!stage || !stage.enabled) continue;
            magDb += dspStageMagDb(stage, freq, sampleRate || 48000);
        }
        var yNorm = 0.5 - magDb / 40; // ±20 dB range
        var y = Math.max(0, Math.min(h, yNorm * h));
        if (firstPoint) { ctx.moveTo(i, y); firstPoint = false; }
        else ctx.lineTo(i, y);
    }
    ctx.stroke();
    ctx.restore();
}

function dspDrawCompareLegend(ctx, w, h, numCh) {
    if (!dspState || !dspState.channels) return;
    var x = w - 10, y = 16;
    ctx.save();
    for (var ch = 0; ch < numCh; ch++) {
        var label = (dspState.channels[ch] && dspState.channels[ch].name) || ('Ch ' + (ch + 1));
        var color = DSP_COMPARE_COLORS[ch % DSP_COMPARE_COLORS.length];
        ctx.fillStyle = color;
        ctx.font = 'bold 11px sans-serif';
        ctx.textAlign = 'right';
        ctx.fillText(label, x, y + ch * 16);
    }
    ctx.restore();
}
