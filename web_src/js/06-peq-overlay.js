        // ===== PEQ / DSP Overlay Editor =====
        // Full-screen overlay for editing per-channel PEQ, crossover, compressor, limiter.

        // Biquad magnitude response: returns dB at frequency f (Hz) given coefficients [b0,b1,b2,a1,a2]
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
        function dspComputeCoeffs(type, freq, gain, Q, fs) {
            var fn = freq / fs;
            if (fn < 0.0001) fn = 0.0001;
            if (fn > 0.4999) fn = 0.4999;
            var w0 = 2 * Math.PI * fn;
            var cosW = Math.cos(w0), sinW = Math.sin(w0);
            if (Q <= 0) Q = 0.707;
            var alpha = sinW / (2 * Q);
            var A, b0, b1, b2, a0, a1, a2, sq, wt, n;
            switch (type) {
                case 0: b1 = 1 - cosW; b0 = b1 / 2; b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 1: b1 = -(1 + cosW); b0 = -b1 / 2; b2 = b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 2: b0 = sinW / 2; b1 = 0; b2 = -b0; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 3: b0 = 1; b1 = -2 * cosW; b2 = 1; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 4: A = Math.pow(10, gain / 40); b0 = 1 + alpha * A; b1 = -2 * cosW; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cosW; a2 = 1 - alpha / A; break;
                case 5: A = Math.pow(10, gain / 40); sq = 2 * Math.sqrt(A) * alpha; b0 = A * ((A+1)-(A-1)*cosW+sq); b1 = 2*A*((A-1)-(A+1)*cosW); b2 = A*((A+1)-(A-1)*cosW-sq); a0 = (A+1)+(A-1)*cosW+sq; a1 = -2*((A-1)+(A+1)*cosW); a2 = (A+1)+(A-1)*cosW-sq; break;
                case 6: A = Math.pow(10, gain / 40); sq = 2 * Math.sqrt(A) * alpha; b0 = A*((A+1)+(A-1)*cosW+sq); b1 = -2*A*((A-1)+(A+1)*cosW); b2 = A*((A+1)+(A-1)*cosW-sq); a0 = (A+1)-(A-1)*cosW+sq; a1 = 2*((A-1)-(A+1)*cosW); a2 = (A+1)-(A-1)*cosW-sq; break;
                case 7: case 8: b0 = 1 - alpha; b1 = -2 * cosW; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 9: b0 = -(1 - alpha); b1 = 2 * cosW; b2 = -(1 + alpha); a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 10: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cosW; a2 = 1 - alpha; break;
                case 19: wt = Math.tan(Math.PI * fn); n = 1 / (1 + wt); return [wt * n, wt * n, 0, (wt - 1) * n, 0];
                case 20: wt = Math.tan(Math.PI * fn); n = 1 / (1 + wt); return [n, -n, 0, (wt - 1) * n, 0];
                default: return [1, 0, 0, 0, 0];
            }
            var inv = 1 / a0;
            return [b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv];
        }

        let peqOverlayActive = false;
        let peqOverlayTarget = null;  // {type:'input'|'output', channel:int}
        let peqOverlayBands = [];     // [{type, freq, gain, Q, enabled}]
        let peqOverlayFs = 48000;

        // Filter type names matching firmware DspStageType enum
        var PEQ_TYPES = [
            {id: 0,  name: 'LPF'},
            {id: 1,  name: 'HPF'},
            {id: 2,  name: 'BPF'},
            {id: 3,  name: 'Notch'},
            {id: 4,  name: 'Peak'},
            {id: 5,  name: 'Lo Shelf'},
            {id: 6,  name: 'Hi Shelf'},
            {id: 7,  name: 'AP 360'},
            {id: 8,  name: 'AP 360'},
            {id: 9,  name: 'AP 180'},
            {id: 10, name: 'BPF0'},
            {id: 19, name: 'LPF1'},
            {id: 20, name: 'HPF1'}
        ];

        function peqTypeName(typeId) {
            var t = PEQ_TYPES.find(function(x) { return x.id === typeId; });
            return t ? t.name : 'PEQ';
        }

        // ===== Open PEQ Overlay =====
        function openPeqOverlay(target, bands, fs) {
            peqOverlayTarget = target;
            peqOverlayBands = bands || [];
            peqOverlayFs = fs || 48000;
            peqOverlayActive = true;

            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
                peqOverlayInitDelegation(overlay);
            }

            var title = target.type === 'input' ? 'Input PEQ — Lane ' + target.channel : 'Output PEQ — Ch ' + target.channel;
            var maxBands = target.type === 'input' ? 6 : 10;

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">' + title + '</span>';
            html += '  <button class="peq-overlay-close" data-action="peq-close">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';

            // Frequency response graph
            html += '<div class="peq-graph-wrap">';
            html += '  <canvas id="peqOverlayCanvas" class="peq-overlay-canvas"></canvas>';
            html += '</div>';

            // Band table
            html += '<div class="peq-band-table-wrap">';
            html += '  <table class="peq-band-table">';
            html += '    <thead><tr><th>#</th><th>Type</th><th>Freq</th><th>Gain</th><th>Q</th><th>On</th><th></th></tr></thead>';
            html += '    <tbody id="peqBandRows">';
            for (var i = 0; i < peqOverlayBands.length; i++) {
                html += peqBandRowHtml(i);
            }
            html += '    </tbody>';
            html += '  </table>';
            html += '</div>';

            // Actions
            html += '<div class="peq-overlay-actions">';
            if (peqOverlayBands.length < maxBands) {
                html += '  <button class="btn btn-sm btn-primary" data-action="peq-add-band">Add Band</button>';
            }
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-reset-all">Reset All</button>';
            html += '  <button class="btn btn-sm btn-primary" data-action="peq-apply">Apply</button>';
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';

            // Draw initial graph
            setTimeout(peqDrawGraph, 50);
        }

        function peqBandRowHtml(idx) {
            var b = peqOverlayBands[idx];
            var typeOptions = '';
            for (var t = 0; t < PEQ_TYPES.length; t++) {
                var pt = PEQ_TYPES[t];
                typeOptions += '<option value="' + pt.id + '"' + (pt.id === b.type ? ' selected' : '') + '>' + pt.name + '</option>';
            }
            var html = '<tr data-band="' + idx + '">';
            html += '<td>' + (idx + 1) + '</td>';
            html += '<td><select class="peq-input peq-type-sel" data-action="peq-update-band" data-band="' + idx + '" data-field="type" data-parse="int">' + typeOptions + '</select></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.freq || 1000) + '" min="20" max="20000" step="1" data-action="peq-update-band" data-band="' + idx + '" data-field="freq" data-parse="float"></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.gain || 0).toFixed(1) + '" min="-24" max="24" step="0.5" data-action="peq-update-band" data-band="' + idx + '" data-field="gain" data-parse="float"></td>';
            html += '<td><input type="number" class="peq-input" value="' + (b.Q || 0.707).toFixed(3) + '" min="0.1" max="30" step="0.01" data-action="peq-update-band" data-band="' + idx + '" data-field="Q" data-parse="float"></td>';
            html += '<td><input type="checkbox"' + (b.enabled !== false ? ' checked' : '') + ' data-action="peq-update-band" data-band="' + idx + '" data-field="enabled" data-parse="bool"></td>';
            html += '<td><button class="channel-btn" data-action="peq-remove-band" data-band="' + idx + '" style="padding:2px 6px;min-width:0">';
            html += '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg>';
            html += '</button></td>';
            html += '</tr>';
            return html;
        }

        function peqUpdateBand(idx, field, value) {
            if (idx >= 0 && idx < peqOverlayBands.length) {
                peqOverlayBands[idx][field] = value;
                peqDrawGraph();
            }
        }

        function peqAddBand() {
            var maxBands = peqOverlayTarget && peqOverlayTarget.type === 'input' ? 6 : 10;
            if (peqOverlayBands.length >= maxBands) return;
            peqOverlayBands.push({ type: 4, freq: 1000, gain: 0, Q: 1.41, enabled: true });
            // Re-render table
            var tbody = document.getElementById('peqBandRows');
            if (tbody) tbody.innerHTML += peqBandRowHtml(peqOverlayBands.length - 1);
            peqDrawGraph();
        }

        function peqRemoveBand(idx) {
            peqOverlayBands.splice(idx, 1);
            // Re-render entire table (index shift)
            var tbody = document.getElementById('peqBandRows');
            if (tbody) {
                var html = '';
                for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                tbody.innerHTML = html;
            }
            peqDrawGraph();
        }

        function peqResetAll() {
            peqOverlayBands = [];
            var tbody = document.getElementById('peqBandRows');
            if (tbody) tbody.innerHTML = '';
            peqDrawGraph();
        }

        function closePeqOverlay() {
            peqOverlayActive = false;
            var overlay = document.getElementById('peqOverlay');
            if (overlay) overlay.style.display = 'none';
        }

        // ===== Apply PEQ changes to firmware =====
        function peqApply() {
            if (!peqOverlayTarget) return;
            var target = peqOverlayTarget;
            var bands = peqOverlayBands;

            if (target.type === 'output') {
                // Apply via output DSP REST API
                // First get current config, then update biquad stages
                apiFetch('/api/output/dsp?ch=' + target.channel)
                    .then(function(r) { return r.json(); })
                    .then(function(cfg) {
                        // Build updated stages: keep non-biquad stages, replace biquads with new bands
                        var stages = [];
                        // Keep existing non-biquad stages
                        if (cfg.stages) {
                            for (var s = 0; s < cfg.stages.length; s++) {
                                var st = cfg.stages[s];
                                // Biquad types are 0-10, 19-20
                                var isBiquad = st.type <= 10 || st.type === 19 || st.type === 20;
                                if (!isBiquad) stages.push(st);
                            }
                        }
                        // Add PEQ bands as biquad stages
                        for (var i = 0; i < bands.length; i++) {
                            stages.push({
                                enabled: bands[i].enabled !== false,
                                type: bands[i].type,
                                frequency: bands[i].freq,
                                gain: bands[i].gain,
                                Q: bands[i].Q
                            });
                        }

                        return apiFetch('/api/output/dsp', {
                            method: 'PUT',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({
                                ch: target.channel,
                                bypass: cfg.bypass || false,
                                stages: stages
                            })
                        });
                    })
                    .then(function() {
                        showToast('PEQ applied to Output Ch ' + target.channel, 'success');
                        closePeqOverlay();
                    })
                    .catch(function(err) { showToast('PEQ apply failed: ' + err, 'error'); });
            } else if (target.type === 'input') {
                // Apply via DSP pipeline WS commands
                // Input PEQ uses the per-input DSP (dsp_pipeline.h), channels 0-3
                var ch = target.channel;
                // Remove all existing biquad stages, then add new ones
                // This is simplified — a full implementation would diff and patch
                for (var i = 0; i < bands.length; i++) {
                    var b = bands[i];
                    if (ws && ws.readyState === WebSocket.OPEN) {
                        ws.send(JSON.stringify({
                            type: 'addDspStage', ch: ch, stageType: b.type,
                            frequency: b.freq, gain: b.gain, Q: b.Q
                        }));
                    }
                }
                showToast('PEQ applied to Input Lane ' + ch, 'success');
                closePeqOverlay();
            }
        }

        // ===== Draw Frequency Response Graph =====
        function peqDrawGraph() {
            var canvas = document.getElementById('peqOverlayCanvas');
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var rect = canvas.parentElement.getBoundingClientRect();
            canvas.width = Math.max(rect.width, 300);
            canvas.height = Math.max(Math.min(rect.height, 300), 180);
            var w = canvas.width, h = canvas.height;
            var fs = peqOverlayFs;

            // Styling
            var isDark = document.body.classList.contains('night-mode');
            var bgColor = isDark ? '#1E1E1E' : '#FFFFFF';
            var gridColor = isDark ? '#333' : '#E0E0E0';
            var textColor = isDark ? '#888' : '#999';
            var combinedColor = '#FF9800';

            // Clear
            ctx.fillStyle = bgColor;
            ctx.fillRect(0, 0, w, h);

            // Axes
            var padL = 40, padR = 10, padT = 15, padB = 25;
            var gW = w - padL - padR, gH = h - padT - padB;
            var dbMin = -18, dbMax = 18;

            // Frequency range: 20Hz to 20kHz (log scale)
            var fMin = 20, fMax = 20000;
            function fToX(f) { return padL + gW * (Math.log10(f / fMin) / Math.log10(fMax / fMin)); }
            function dbToY(db) { return padT + gH * (1 - (db - dbMin) / (dbMax - dbMin)); }

            // Grid lines
            ctx.strokeStyle = gridColor;
            ctx.lineWidth = 0.5;
            ctx.font = '10px monospace';
            ctx.fillStyle = textColor;

            // dB grid
            for (var db = dbMin; db <= dbMax; db += 6) {
                var y = dbToY(db);
                ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(w - padR, y); ctx.stroke();
                ctx.fillText(db + '', 2, y + 3);
            }

            // Frequency grid
            var freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
            for (var fi = 0; fi < freqs.length; fi++) {
                var x = fToX(freqs[fi]);
                ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, h - padB); ctx.stroke();
                var label = freqs[fi] >= 1000 ? (freqs[fi] / 1000) + 'k' : freqs[fi] + '';
                ctx.fillText(label, x - 6, h - 5);
            }

            // 0dB reference line
            ctx.strokeStyle = isDark ? '#555' : '#BBB';
            ctx.lineWidth = 1;
            ctx.beginPath(); ctx.moveTo(padL, dbToY(0)); ctx.lineTo(w - padR, dbToY(0)); ctx.stroke();

            // Per-band curves
            var bandColors = ['#F44336', '#2196F3', '#4CAF50', '#FFC107', '#9C27B0', '#00BCD4', '#FF5722', '#607D8B', '#E91E63', '#3F51B5'];
            var numPoints = Math.max(gW, 200);

            var bi, p, f, coeffs;
            for (bi = 0; bi < peqOverlayBands.length; bi++) {
                var band = peqOverlayBands[bi];
                if (band.enabled === false) continue;
                coeffs = dspComputeCoeffs(band.type, band.freq || 1000, band.gain || 0, band.Q || 0.707, fs);

                ctx.strokeStyle = bandColors[bi % bandColors.length] + '55';
                ctx.lineWidth = 1;
                ctx.beginPath();
                for (p = 0; p <= numPoints; p++) {
                    f = fMin * Math.pow(fMax / fMin, p / numPoints);
                    var mag = dspBiquadMagDb(coeffs, f, fs);
                    x = fToX(f); y = dbToY(Math.max(dbMin, Math.min(dbMax, mag)));
                    if (p === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }

            // Combined response
            ctx.strokeStyle = combinedColor;
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (p = 0; p <= numPoints; p++) {
                f = fMin * Math.pow(fMax / fMin, p / numPoints);
                var totalDb = 0;
                for (bi = 0; bi < peqOverlayBands.length; bi++) {
                    if (peqOverlayBands[bi].enabled === false) continue;
                    coeffs = dspComputeCoeffs(peqOverlayBands[bi].type, peqOverlayBands[bi].freq || 1000,
                        peqOverlayBands[bi].gain || 0, peqOverlayBands[bi].Q || 0.707, fs);
                    totalDb += dspBiquadMagDb(coeffs, f, fs);
                }
                x = fToX(f); y = dbToY(Math.max(dbMin, Math.min(dbMax, totalDb)));
                if (p === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        // ===== Wire up overlay openers from 05-audio-tab.js =====
        // Override the stubs defined in 05-audio-tab.js
        function openInputPeq(lane) {
            // Fetch current DSP state for this input channel
            // Input DSP uses dsp_pipeline channels (lane 0=ch0, lane 1=ch1, etc.)
            // For now, open with empty bands — user adds bands
            openPeqOverlay({ type: 'input', channel: lane }, [], peqOverlayFs);
        }

        function openOutputPeq(channel) {
            // Fetch current output DSP config and extract biquad stages
            apiFetch('/api/output/dsp?ch=' + channel)
                .then(function(r) { return r.json(); })
                .then(function(cfg) {
                    var bands = [];
                    if (cfg.stages) {
                        for (var s = 0; s < cfg.stages.length; s++) {
                            var st = cfg.stages[s];
                            var isBiquad = st.type <= 10 || st.type === 19 || st.type === 20;
                            if (isBiquad) {
                                bands.push({
                                    type: st.type,
                                    freq: st.frequency || 1000,
                                    gain: st.gain || 0,
                                    Q: st.Q || 0.707,
                                    enabled: st.enabled !== false
                                });
                            }
                        }
                    }
                    peqOverlayFs = cfg.sampleRate || 48000;
                    openPeqOverlay({ type: 'output', channel: channel }, bands, peqOverlayFs);
                })
                .catch(function() {
                    openPeqOverlay({ type: 'output', channel: channel }, [], 48000);
                });
        }

        // ===== Crossover Overlay =====
        function openOutputCrossover(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
                peqOverlayInitDelegation(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Crossover — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" data-action="peq-close">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div class="peq-graph-wrap"><canvas id="peqOverlayCanvas" class="peq-overlay-canvas"></canvas></div>';
            html += '<div style="padding:16px;">';
            html += '  <div class="channel-control-row" style="margin-bottom:8px;">';
            html += '    <label class="channel-control-label" style="min-width:80px">Type</label>';
            html += '    <select class="form-input" id="xoverType" style="max-width:200px">';
            html += '      <option value="lr2">Linkwitz-Riley 2nd (12dB/oct)</option>';
            html += '      <option value="lr4" selected>Linkwitz-Riley 4th (24dB/oct)</option>';
            html += '      <option value="lr8">Linkwitz-Riley 8th (48dB/oct)</option>';
            html += '      <option value="bw6">Butterworth 1st (6dB/oct)</option>';
            html += '      <option value="bw12">Butterworth 2nd (12dB/oct)</option>';
            html += '      <option value="bw18">Butterworth 3rd (18dB/oct)</option>';
            html += '      <option value="bw24">Butterworth 4th (24dB/oct)</option>';
            html += '    </select>';
            html += '  </div>';
            html += '  <div class="channel-control-row" style="margin-bottom:8px;">';
            html += '    <label class="channel-control-label" style="min-width:80px">Frequency</label>';
            html += '    <input type="number" class="form-input" id="xoverFreq" value="80" min="20" max="20000" step="1" style="max-width:120px">';
            html += '    <span class="channel-gain-value">Hz</span>';
            html += '  </div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="xover-apply" data-channel="' + channel + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyXover(channel) {
            var type = document.getElementById('xoverType').value;
            var freq = parseFloat(document.getElementById('xoverFreq').value) || 80;
            var orderMap = { lr2: 2, lr4: 4, lr8: 8, bw6: 1, bw12: 2, bw18: 3, bw24: 4 };
            var order = orderMap[type] || 4;

            // Use the output DSP crossover REST endpoint
            apiFetch('/api/output/dsp/crossover', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ subCh: channel, mainCh: channel + 1, freqHz: freq, order: order })
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                showToast('Crossover applied (' + type + ' @ ' + freq + ' Hz)', 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Crossover failed', 'error'); });
        }

        // ===== Compressor Overlay =====
        function openOutputCompressor(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
                peqOverlayInitDelegation(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Compressor — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" data-action="peq-close">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'compThreshold', -40, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'compRatio', 1, 20, 4, 0.5, ':1');
            html += peqControlRow('Attack', 'compAttack', 0.1, 200, 10, 0.1, 'ms');
            html += peqControlRow('Release', 'compRelease', 10, 2000, 100, 1, 'ms');
            html += peqControlRow('Knee', 'compKnee', 0, 20, 6, 0.5, 'dB');
            html += peqControlRow('Makeup', 'compMakeup', 0, 24, 0, 0.5, 'dB');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="compressor-apply" data-channel="' + channel + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyCompressor(channel) {
            var params = {
                thresholdDb: parseFloat(document.getElementById('compThreshold').value),
                ratio: parseFloat(document.getElementById('compRatio').value),
                attackMs: parseFloat(document.getElementById('compAttack').value),
                releaseMs: parseFloat(document.getElementById('compRelease').value),
                kneeDb: parseFloat(document.getElementById('compKnee').value),
                makeupGainDb: parseFloat(document.getElementById('compMakeup').value)
            };

            // Add compressor stage via REST API
            apiFetch('/api/output/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ch: channel, type: 14 })  // DSP_COMPRESSOR = 14
            })
            .then(function() {
                showToast('Compressor applied to Ch ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Compressor failed', 'error'); });
        }

        // ===== Limiter Overlay =====
        function openOutputLimiter(channel) {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
                peqOverlayInitDelegation(overlay);
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">Limiter — Ch ' + channel + '</span>';
            html += '  <button class="peq-overlay-close" data-action="peq-close">';
            html += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            html += '  </button>';
            html += '</div>';
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'limThreshold', -40, 0, -3, 0.5, 'dBFS');
            html += peqControlRow('Attack', 'limAttack', 0.01, 50, 0.1, 0.01, 'ms');
            html += peqControlRow('Release', 'limRelease', 1, 1000, 50, 1, 'ms');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="limiter-apply" data-channel="' + channel + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyLimiter(channel) {
            apiFetch('/api/output/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ch: channel, type: 11 })  // DSP_LIMITER = 11
            })
            .then(function() {
                showToast('Limiter applied to Ch ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Limiter failed', 'error'); });
        }

        // Helper: control row for dynamics overlays
        function peqControlRow(label, id, min, max, defaultVal, step, unit) {
            return '<div class="channel-control-row" style="margin-bottom:8px;">' +
                '<label class="channel-control-label" style="min-width:80px">' + label + '</label>' +
                '<input type="range" class="channel-gain-slider" id="' + id + '" min="' + min + '" max="' + max + '" step="' + step + '" value="' + defaultVal + '" data-action="peq-control-update" data-val-id="' + id + 'Val">' +
                '<span class="channel-gain-value" id="' + id + 'Val">' + defaultVal + ' ' + unit + '</span>' +
                '</div>';
        }

        // ===== Event Delegation for PEQ Overlay =====
        // Set up once on the overlay element; handles all dynamic child elements
        function peqOverlayInitDelegation(overlay) {
            if (overlay.dataset.delegationInit) return;
            overlay.dataset.delegationInit = '1';

            overlay.addEventListener('click', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;

                if (action === 'peq-close') {
                    closePeqOverlay();
                } else if (action === 'peq-add-band') {
                    peqAddBand();
                } else if (action === 'peq-reset-all') {
                    peqResetAll();
                } else if (action === 'peq-apply') {
                    peqApply();
                } else if (action === 'peq-remove-band') {
                    peqRemoveBand(parseInt(el.dataset.band));
                } else if (action === 'xover-apply') {
                    applyXover(parseInt(el.dataset.channel));
                } else if (action === 'compressor-apply') {
                    applyCompressor(parseInt(el.dataset.channel));
                } else if (action === 'limiter-apply') {
                    applyLimiter(parseInt(el.dataset.channel));
                } else if (action === 'matrix-gain-set0') {
                    setMatrixGainDb(parseInt(el.dataset.out), parseInt(el.dataset.in), 0);
                    closeMatrixPopup();
                } else if (action === 'matrix-gain-setoff') {
                    setMatrixGainDb(parseInt(el.dataset.out), parseInt(el.dataset.in), -72);
                    closeMatrixPopup();
                } else if (action === 'matrix-popup-close') {
                    closeMatrixPopup();
                }
            });

            overlay.addEventListener('change', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;

                if (action === 'peq-update-band') {
                    var bandIdx = parseInt(el.dataset.band);
                    var field = el.dataset.field;
                    var parse = el.dataset.parse;
                    var value;
                    if (parse === 'int') {
                        value = parseInt(el.value);
                    } else if (parse === 'float') {
                        value = parseFloat(el.value);
                    } else if (parse === 'bool') {
                        value = el.checked;
                    } else {
                        value = el.value;
                    }
                    peqUpdateBand(bandIdx, field, value);
                }
            });

            overlay.addEventListener('input', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;

                if (action === 'peq-control-update') {
                    var valEl = document.getElementById(el.dataset.valId);
                    if (valEl) valEl.textContent = el.value;
                } else if (action === 'peq-update-band') {
                    // For number inputs, update in real-time on input too
                    var bandIdx = parseInt(el.dataset.band);
                    var field = el.dataset.field;
                    var parse = el.dataset.parse;
                    var value;
                    if (parse === 'int') {
                        value = parseInt(el.value);
                    } else if (parse === 'float') {
                        value = parseFloat(el.value);
                    } else {
                        value = el.value;
                    }
                    if (!isNaN(value)) peqUpdateBand(bandIdx, field, value);
                }
            });
        }
