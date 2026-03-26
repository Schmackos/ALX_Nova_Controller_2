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

        // ===== A/B Compare State =====
        // _peqAbSlot: 'A' or 'B' — which set is currently active (being edited)
        // _peqBandsA: bands stored as slot A (populated when A/B is first activated)
        // _peqBandsB: bands stored as slot B (starts empty)
        var _peqAbSlot = 'A';
        var _peqBandsA = null;   // null = A/B not yet activated
        var _peqBandsB = [];

        // Graph coordinate constants — shared between draw and pointer handlers
        var PEQ_F_MIN = 5;
        var PEQ_F_MAX = 20000;
        var PEQ_DB_MIN = -18;
        var PEQ_DB_MAX = 18;
        var PEQ_PAD_L = 40;
        var PEQ_PAD_R = 10;
        var PEQ_PAD_T = 15;
        var PEQ_PAD_B = 25;

        // Control point drag state
        var _peqDragBand = -1;      // band index being dragged (-1 = none)
        var _peqDragMode = '';      // 'freq-gain' | 'Q'
        var _peqDragStartX = 0;
        var _peqDragStartY = 0;
        var _peqDragStartFreq = 0;
        var _peqDragStartGain = 0;
        var _peqDragStartQ = 0;

        // Coordinate transforms (hoisted so pointer handlers can use them)
        function peqFToX(f, gW) {
            return PEQ_PAD_L + gW * (Math.log10(f / PEQ_F_MIN) / Math.log10(PEQ_F_MAX / PEQ_F_MIN));
        }
        function peqXToF(x, gW) {
            var ratio = (x - PEQ_PAD_L) / gW;
            return Math.max(PEQ_F_MIN, Math.min(PEQ_F_MAX, PEQ_F_MIN * Math.pow(PEQ_F_MAX / PEQ_F_MIN, ratio)));
        }
        function peqDbToY(db, gH) {
            return PEQ_PAD_T + gH * (1 - (db - PEQ_DB_MIN) / (PEQ_DB_MAX - PEQ_DB_MIN));
        }
        function peqYToDb(y, gH) {
            return Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX,
                PEQ_DB_MIN + (PEQ_DB_MAX - PEQ_DB_MIN) * (1 - (y - PEQ_PAD_T) / gH)));
        }

        // Hit-test a canvas pixel point against all band control points.
        // Returns band index or -1. Uses device-pixel-aware coords.
        function peqHitTestBand(cx, cy, canvas) {
            var dpr = window.devicePixelRatio || 1;
            var w = canvas.width, h = canvas.height;
            var gW = w - (PEQ_PAD_L + PEQ_PAD_R) * dpr;
            var gH = h - (PEQ_PAD_T + PEQ_PAD_B) * dpr;
            var pL = PEQ_PAD_L * dpr, pT = PEQ_PAD_T * dpr;
            var hitRadius = 12 * dpr;

            for (var i = peqOverlayBands.length - 1; i >= 0; i--) {
                var b = peqOverlayBands[i];
                if (b.enabled === false) continue;
                var bx = pL + gW * (Math.log10((b.freq || 1000) / PEQ_F_MIN) / Math.log10(PEQ_F_MAX / PEQ_F_MIN));
                var by = pT + gH * (1 - ((b.gain || 0) - PEQ_DB_MIN) / (PEQ_DB_MAX - PEQ_DB_MIN));
                if (Math.abs(cx - bx) < hitRadius && Math.abs(cy - by) < hitRadius) return i;
            }
            return -1;
        }

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

            // Reset A/B state when opening overlay fresh
            _peqAbSlot = 'A';
            _peqBandsA = null;
            _peqBandsB = [];

            // Build copy-from-channel options from audioChannelMap
            var copyOptions = '<option value="">-- Select channel --</option>';
            if (audioChannelMap) {
                var srcList = target.type === 'input' ? (audioChannelMap.inputs || []) : (audioChannelMap.outputs || []);
                for (var ci = 0; ci < srcList.length; ci++) {
                    var srcChan = target.type === 'input' ? srcList[ci].lane : srcList[ci].firstChannel;
                    if (srcChan === target.channel) continue;
                    var srcLabel = target.type === 'input'
                        ? 'Lane ' + srcChan + ' (' + escapeHtml(srcList[ci].deviceName || 'Input') + ')'
                        : 'Ch ' + srcChan + ' (' + escapeHtml(srcList[ci].name || 'Output') + ')';
                    copyOptions += '<option value="' + srcChan + '">' + srcLabel + '</option>';
                }
            }

            var html = '<div class="peq-overlay-header">';
            html += '  <span class="peq-overlay-title">' + title + '</span>';
            html += '  <div class="peq-overlay-header-tools">';
            html += '    <div class="peq-ab-toggle" title="A/B compare: store two EQ curves and switch between them">';
            html += '      <button class="peq-ab-btn active" id="peqAbBtnA" data-action="peq-ab-select" data-slot="A">A</button>';
            html += '      <button class="peq-ab-btn" id="peqAbBtnB" data-action="peq-ab-select" data-slot="B">B</button>';
            html += '    </div>';
            html += '    <select class="peq-copy-select" id="peqCopySelect" data-action="peq-copy-channel" title="Copy EQ bands from another channel">';
            html += '      ' + copyOptions;
            html += '    </select>';
            html += '  </div>';
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
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-preset-save" title="Save EQ curve as preset">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M17,3H7A2,2 0 0,0 5,5V21L12,18L19,21V5C19,3.89 18.1,3 17,3Z"/></svg>';
            html += '    Save Preset';
            html += '  </button>';
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-preset-load" title="Load a saved PEQ preset">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M19,20H4C2.89,20 2,19.1 2,18V6C2,4.89 2.89,4 4,4H10L12,6H19A2,2 0 0,1 21,8H21V18C21,19.1 20.11,20 19,20Z"/></svg>';
            html += '    Load Preset';
            html += '  </button>';
            html += '  <label class="btn btn-sm btn-secondary peq-import-label" title="Import REW/APO filter text">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2M18,20H6V4H13V9H18V20M13,13V18H11V13H8L12,9L16,13H13Z"/></svg>';
            html += '    Import REW';
            html += '    <input type="file" accept=".txt" data-action="peq-rew-import" style="display:none">';
            html += '  </label>';
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-rew-export">Export REW</button>';
            html += '  <button class="btn btn-sm btn-primary" data-action="peq-apply">Apply</button>';
            html += '  <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '</div>';

            overlay.innerHTML = html;
            overlay.style.display = 'flex';

            // Draw initial graph and attach drag handlers
            setTimeout(function() {
                peqDrawGraph();
                peqInitDragHandlers(overlay);
            }, 50);
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
            html += '<td><input type="number" class="peq-input" value="' + (b.freq || 1000) + '" min="5" max="20000" step="1" data-action="peq-update-band" data-band="' + idx + '" data-field="freq" data-parse="float"></td>';
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
            _peqDragBand = -1;
            _peqBandsA = null;
            _peqBandsB = [];
            _peqAbSlot = 'A';
            var overlay = document.getElementById('peqOverlay');
            if (overlay) overlay.style.display = 'none';
        }

        // ===== A/B Compare =====
        // On first activation: saves current bands as A, initialises B as empty.
        // Subsequent clicks swap the active slot and reload bands.

        function peqAbSelect(slot) {
            if (!peqOverlayActive) return;

            if (_peqBandsA === null) {
                // First time activating A/B — capture current bands into A
                _peqBandsA = peqOverlayBands.slice();
                _peqBandsB = [];
            }

            // Save current editing set into the slot we are leaving
            if (_peqAbSlot === 'A') {
                _peqBandsA = peqOverlayBands.slice();
            } else {
                _peqBandsB = peqOverlayBands.slice();
            }

            _peqAbSlot = slot;

            // Load the new slot
            peqOverlayBands = (slot === 'A' ? _peqBandsA : _peqBandsB).slice();

            // Update button styles
            var btnA = document.getElementById('peqAbBtnA');
            var btnB = document.getElementById('peqAbBtnB');
            if (btnA) btnA.classList.toggle('active', slot === 'A');
            if (btnB) btnB.classList.toggle('active', slot === 'B');

            // Re-render table and graph
            var tbody = document.getElementById('peqBandRows');
            if (tbody) {
                var html = '';
                for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                tbody.innerHTML = html;
            }
            peqDrawGraph();
        }

        // ===== Copy from Channel =====
        // Fetches the DSP channel config from the firmware and loads its PEQ bands.

        function peqCopyFromChannel(srcChannel) {
            if (srcChannel === '' || srcChannel === null || srcChannel === undefined) return;
            var ch = parseInt(srcChannel, 10);
            if (isNaN(ch)) return;

            apiFetch('/api/dsp/channel?channel=' + ch)
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (!data.success) { showToast('Copy failed: ' + (data.message || 'error'), 'error'); return; }
                    // Extract biquad (PEQ) stages from the stage list
                    var stages = data.stages || [];
                    var bands = [];
                    for (var s = 0; s < stages.length; s++) {
                        var st = stages[s];
                        // Include all biquad types (0-10, 19, 20) as bands
                        var typeId = typeof st.type === 'number' ? st.type : parseInt(st.type, 10);
                        if (typeId >= 0 && typeId <= 10 || typeId === 19 || typeId === 20) {
                            bands.push({
                                type: typeId,
                                freq: (st.params && st.params.frequency) || 1000,
                                gain: (st.params && st.params.gain) || 0,
                                Q: (st.params && st.params.Q) || 0.707,
                                enabled: st.enabled !== false
                            });
                        }
                    }

                    if (bands.length === 0) {
                        showToast('No PEQ bands found on channel ' + ch, 'info');
                        return;
                    }

                    peqOverlayBands = bands;
                    var tbody = document.getElementById('peqBandRows');
                    if (tbody) {
                        var html = '';
                        for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                        tbody.innerHTML = html;
                    }
                    peqDrawGraph();
                    showToast('Copied ' + bands.length + ' band(s) from Ch ' + ch, 'success');

                    // Reset copy dropdown
                    var sel = document.getElementById('peqCopySelect');
                    if (sel) sel.value = '';
                })
                .catch(function() { showToast('Copy failed', 'error'); });
        }

        // ===== Drag-on-graph interaction =====
        // Attaches pointer/touch event handlers to the canvas for dragging control points.
        function peqInitDragHandlers(overlay) {
            var canvas = document.getElementById('peqOverlayCanvas');
            if (!canvas || canvas.dataset.dragInit) return;
            canvas.dataset.dragInit = '1';

            function getCanvasXY(e) {
                var rect = canvas.getBoundingClientRect();
                var dpr = window.devicePixelRatio || 1;
                var cx, cy;
                if (e.touches && e.touches.length > 0) {
                    cx = (e.touches[0].clientX - rect.left) * dpr;
                    cy = (e.touches[0].clientY - rect.top) * dpr;
                } else {
                    cx = (e.clientX - rect.left) * dpr;
                    cy = (e.clientY - rect.top) * dpr;
                }
                return { x: cx, y: cy };
            }

            function onPointerDown(e) {
                var pt = getCanvasXY(e);
                var hitBand = peqHitTestBand(pt.x, pt.y, canvas);
                if (hitBand < 0) return;
                e.preventDefault();
                _peqDragBand = hitBand;
                _peqDragMode = 'freq-gain';
                _peqDragStartX = pt.x;
                _peqDragStartY = pt.y;
                _peqDragStartFreq = peqOverlayBands[hitBand].freq || 1000;
                _peqDragStartGain = peqOverlayBands[hitBand].gain || 0;
                _peqDragStartQ = peqOverlayBands[hitBand].Q || 0.707;
                peqDrawGraph();
            }

            function onPointerMove(e) {
                if (_peqDragBand < 0) return;
                e.preventDefault();
                var pt = getCanvasXY(e);
                var dpr = window.devicePixelRatio || 1;
                var w = canvas.width, h = canvas.height;
                var gW = w - (PEQ_PAD_L + PEQ_PAD_R) * dpr;
                var gH = h - (PEQ_PAD_T + PEQ_PAD_B) * dpr;

                if (_peqDragMode === 'freq-gain') {
                    // Map pixel delta to log-freq and dB
                    var startFx = (PEQ_PAD_L * dpr) + gW * (Math.log10(_peqDragStartFreq / PEQ_F_MIN) / Math.log10(PEQ_F_MAX / PEQ_F_MIN));
                    var newFx = startFx + (pt.x - _peqDragStartX);
                    var ratio = (newFx - PEQ_PAD_L * dpr) / gW;
                    var newFreq = Math.max(PEQ_F_MIN, Math.min(PEQ_F_MAX,
                        PEQ_F_MIN * Math.pow(PEQ_F_MAX / PEQ_F_MIN, ratio)));

                    var startDy = (PEQ_PAD_T * dpr) + gH * (1 - (_peqDragStartGain - PEQ_DB_MIN) / (PEQ_DB_MAX - PEQ_DB_MIN));
                    var newDy = startDy + (pt.y - _peqDragStartY);
                    var newGain = Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX,
                        PEQ_DB_MIN + (PEQ_DB_MAX - PEQ_DB_MIN) * (1 - (newDy - PEQ_PAD_T * dpr) / gH)));

                    peqOverlayBands[_peqDragBand].freq = Math.round(newFreq * 10) / 10;
                    peqOverlayBands[_peqDragBand].gain = Math.round(newGain * 10) / 10;

                    // Sync table inputs
                    peqSyncBandToTable(_peqDragBand);
                    peqDrawGraph();
                }
            }

            function onPointerUp(e) {
                if (_peqDragBand >= 0) {
                    _peqDragBand = -1;
                    peqDrawGraph();
                }
            }

            // Mouse events
            canvas.addEventListener('mousedown', onPointerDown);
            canvas.addEventListener('mousemove', onPointerMove);
            canvas.addEventListener('mouseup', onPointerUp);
            canvas.addEventListener('mouseleave', onPointerUp);

            // Touch events
            canvas.addEventListener('touchstart', onPointerDown, { passive: false });
            canvas.addEventListener('touchmove', onPointerMove, { passive: false });
            canvas.addEventListener('touchend', onPointerUp);

            // Wheel on control point: adjust Q
            canvas.addEventListener('wheel', function(e) {
                var pt = getCanvasXY(e);
                var hitBand = peqHitTestBand(pt.x, pt.y, canvas);
                if (hitBand < 0) return;
                e.preventDefault();
                var delta = e.deltaY > 0 ? -0.1 : 0.1;
                var newQ = Math.max(0.1, Math.min(30, (peqOverlayBands[hitBand].Q || 0.707) + delta));
                peqOverlayBands[hitBand].Q = Math.round(newQ * 1000) / 1000;
                peqSyncBandToTable(hitBand);
                peqDrawGraph();
            }, { passive: false });

            // Click on empty graph area: add band at that position
            canvas.addEventListener('click', function(e) {
                if (_peqDragBand >= 0) return;  // was drag, not click
                var pt = getCanvasXY(e);
                var hitBand = peqHitTestBand(pt.x, pt.y, canvas);
                if (hitBand >= 0) return;  // hit existing band

                // Check within graph area
                var dpr = window.devicePixelRatio || 1;
                var w = canvas.width, h = canvas.height;
                var gW = w - (PEQ_PAD_L + PEQ_PAD_R) * dpr;
                var gH = h - (PEQ_PAD_T + PEQ_PAD_B) * dpr;
                var inGraph = pt.x >= PEQ_PAD_L * dpr && pt.x <= w - PEQ_PAD_R * dpr &&
                              pt.y >= PEQ_PAD_T * dpr && pt.y <= h - PEQ_PAD_B * dpr;
                if (!inGraph) return;

                var maxBands = peqOverlayTarget && peqOverlayTarget.type === 'input' ? 6 : 10;
                if (peqOverlayBands.length >= maxBands) return;

                var ratio = (pt.x - PEQ_PAD_L * dpr) / gW;
                var newFreq = Math.max(PEQ_F_MIN, Math.min(PEQ_F_MAX,
                    PEQ_F_MIN * Math.pow(PEQ_F_MAX / PEQ_F_MIN, ratio)));
                var newGain = Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX,
                    PEQ_DB_MIN + (PEQ_DB_MAX - PEQ_DB_MIN) * (1 - (pt.y - PEQ_PAD_T * dpr) / gH)));

                peqOverlayBands.push({
                    type: 4, // Peak
                    freq: Math.round(newFreq),
                    gain: Math.round(newGain * 10) / 10,
                    Q: 1.41,
                    enabled: true
                });
                var tbody = document.getElementById('peqBandRows');
                if (tbody) tbody.innerHTML += peqBandRowHtml(peqOverlayBands.length - 1);
                peqDrawGraph();
            });

            // Set drag cursor on hover
            canvas.addEventListener('mousemove', function(e) {
                var pt = getCanvasXY(e);
                var hitBand = peqHitTestBand(pt.x, pt.y, canvas);
                canvas.style.cursor = hitBand >= 0 ? 'grab' : 'crosshair';
            });
        }

        // Sync a band's freq/gain/Q back to the table inputs (after drag)
        function peqSyncBandToTable(idx) {
            var row = document.querySelector('#peqBandRows tr[data-band="' + idx + '"]');
            if (!row) return;
            var b = peqOverlayBands[idx];
            var freqInput = row.querySelector('[data-field="freq"]');
            var gainInput = row.querySelector('[data-field="gain"]');
            var qInput = row.querySelector('[data-field="Q"]');
            if (freqInput) freqInput.value = b.freq;
            if (gainInput) gainInput.value = (b.gain || 0).toFixed(1);
            if (qInput) qInput.value = (b.Q || 0.707).toFixed(3);
        }

        // ===== REW / APO Filter Import =====
        // Parses REW/EqualizerAPO filter export format:
        // Filter 1: ON PK Fc 1000 Hz Gain 3.0 dB Q 2.00
        // Filter 1: ON LSC Fc 80 Hz Gain -3.0 dB Q 0.707
        // Also handles APO format: Filter 1: ON PeakEQ Fc 1000 Hz Gain 3.0 dB BW Oct 0.5
        function peqImportREW(text) {
            var lines = text.split('\n');
            var imported = [];
            var rewTypeMap = {
                'PK': 4, 'PeakEQ': 4, 'PEAK': 4,
                'LP': 0, 'LPQ': 0, 'LPF': 0,
                'HP': 1, 'HPQ': 1, 'HPF': 1,
                'BP': 2, 'BPF': 2,
                'NO': 3, 'NOTCH': 3,
                'LSC': 5, 'LS': 5, 'LowShelf': 5, 'Low Shelf': 5,
                'HSC': 6, 'HS': 6, 'HighShelf': 6, 'High Shelf': 6,
                'AP': 7, 'APF': 7
            };

            for (var li = 0; li < lines.length; li++) {
                var line = lines[li].trim();
                if (!line || line.startsWith('#') || line.startsWith('*')) continue;

                // Match: Filter N: ON <type> Fc <f> Hz Gain <g> dB Q <q>
                var m = line.match(/Filter\s+\d+:\s+ON\s+(\S+)\s+Fc\s+([\d.]+)\s+Hz\s+Gain\s+([+-]?[\d.]+)\s+dB(?:\s+Q\s+([\d.]+))?/i);
                if (!m) {
                    // Try APO BW/Oct format
                    m = line.match(/Filter\s+\d+:\s+ON\s+(\S+)\s+Fc\s+([\d.]+)\s+Hz\s+Gain\s+([+-]?[\d.]+)\s+dB(?:\s+BW\s+Oct\s+([\d.]+))?/i);
                    if (m && m[4]) {
                        // Convert BW in octaves to Q: Q = 1 / (2 * sinh(ln(2)/2 * BW))
                        var bw = parseFloat(m[4]);
                        var qFromBw = 1 / (2 * Math.sinh(Math.LN2 / 2 * bw));
                        m[4] = qFromBw.toFixed(3);
                    }
                }
                if (!m) continue;

                var typeStr = m[1];
                var typeId = rewTypeMap[typeStr];
                if (typeId === undefined) continue;

                imported.push({
                    type: typeId,
                    freq: Math.max(PEQ_F_MIN, Math.min(PEQ_F_MAX, parseFloat(m[2]))),
                    gain: Math.max(-24, Math.min(24, parseFloat(m[3]))),
                    Q: m[4] ? Math.max(0.1, Math.min(30, parseFloat(m[4]))) : 0.707,
                    enabled: true
                });
            }
            return imported;
        }

        // Exports current bands as REW/APO filter text
        function peqExportREW() {
            var rewTypeMap = ['LPF', 'HPF', 'BPF', 'Notch', 'PK', 'LS', 'HS', 'APF', 'APF', 'APF', 'BPF', 'None', 'None',
                'None', 'None', 'None', 'None', 'None', 'None', 'LPF', 'HPF'];
            var lines = ['# Generated by ALX Nova Controller'];
            for (var i = 0; i < peqOverlayBands.length; i++) {
                var b = peqOverlayBands[i];
                var typeStr = rewTypeMap[b.type] || 'PK';
                var onOff = b.enabled !== false ? 'ON' : 'OFF';
                lines.push('Filter ' + (i + 1) + ': ' + onOff + ' ' + typeStr +
                    ' Fc ' + (b.freq || 1000).toFixed(0) + ' Hz' +
                    ' Gain ' + (b.gain || 0).toFixed(1) + ' dB' +
                    ' Q ' + (b.Q || 0.707).toFixed(3));
            }
            return lines.join('\n');
        }

        function peqHandleRewImport(file) {
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function(e) {
                var imported = peqImportREW(e.target.result);
                if (imported.length === 0) {
                    showToast('No valid REW filters found in file', 'warning');
                    return;
                }
                var maxBands = peqOverlayTarget && peqOverlayTarget.type === 'input' ? 6 : 10;
                peqOverlayBands = imported.slice(0, maxBands);
                var tbody = document.getElementById('peqBandRows');
                if (tbody) {
                    var html = '';
                    for (var i = 0; i < peqOverlayBands.length; i++) html += peqBandRowHtml(i);
                    tbody.innerHTML = html;
                }
                peqDrawGraph();
                showToast(imported.length + ' filter' + (imported.length !== 1 ? 's' : '') + ' imported', 'success');
            };
            reader.readAsText(file);
        }

        function peqHandleRewExport() {
            var text = peqExportREW();
            var blob = new Blob([text], { type: 'text/plain' });
            var url = URL.createObjectURL(blob);
            var a = document.createElement('a');
            a.href = url;
            a.download = 'peq-filters.txt';
            a.click();
            URL.revokeObjectURL(url);
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

            // DPR-aware resize (from 06-canvas-helpers.js)
            resizeCanvasIfNeeded(canvas);

            var ctx = canvas.getContext('2d');
            var dpr = window.devicePixelRatio || 1;
            var w = canvas.width, h = canvas.height;
            if (w === 0 || h === 0) return;
            var fs = peqOverlayFs;

            // Scale everything by DPR
            var pL = PEQ_PAD_L * dpr, pR = PEQ_PAD_R * dpr;
            var pT = PEQ_PAD_T * dpr, pB = PEQ_PAD_B * dpr;
            var gW = w - pL - pR, gH = h - pT - pB;

            // Local coord helpers (pixel-space, DPR-scaled)
            function fx(f) { return pL + gW * (Math.log10(f / PEQ_F_MIN) / Math.log10(PEQ_F_MAX / PEQ_F_MIN)); }
            function dy(db) { return pT + gH * (1 - (db - PEQ_DB_MIN) / (PEQ_DB_MAX - PEQ_DB_MIN)); }

            // Styling
            var isDark = document.body.classList.contains('night-mode');
            var bgColor = isDark ? '#1E1E1E' : '#FFFFFF';
            var gridColor = isDark ? '#333333' : '#E0E0E0';
            var textColor = isDark ? '#888888' : '#999999';
            var refLineColor = isDark ? '#555555' : '#BBBBBB';
            var combinedColor = '#FF9800';
            var bandColors = ['#F44336', '#2196F3', '#4CAF50', '#FFC107', '#9C27B0', '#00BCD4', '#FF5722', '#607D8B', '#E91E63', '#3F51B5'];

            // Clear
            ctx.fillStyle = bgColor;
            ctx.fillRect(0, 0, w, h);

            ctx.save();

            // dB grid lines
            ctx.strokeStyle = gridColor;
            ctx.lineWidth = 0.5 * dpr;
            ctx.font = (10 * dpr) + 'px monospace';
            ctx.fillStyle = textColor;
            ctx.textAlign = 'right';

            for (var db = PEQ_DB_MIN; db <= PEQ_DB_MAX; db += 6) {
                var y = dy(db);
                ctx.beginPath(); ctx.moveTo(pL, y); ctx.lineTo(w - pR, y); ctx.stroke();
                ctx.fillText(db + '', pL - 2 * dpr, y + 3 * dpr);
            }

            // Frequency grid lines
            ctx.textAlign = 'center';
            var freqs = [5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
            for (var fi = 0; fi < freqs.length; fi++) {
                var xf = fx(freqs[fi]);
                ctx.strokeStyle = gridColor;
                ctx.lineWidth = 0.5 * dpr;
                ctx.beginPath(); ctx.moveTo(xf, pT); ctx.lineTo(xf, h - pB); ctx.stroke();
                var flabel = freqs[fi] >= 1000 ? (freqs[fi] / 1000) + 'k' : freqs[fi] + '';
                ctx.fillStyle = textColor;
                ctx.fillText(flabel, xf, h - pB + 12 * dpr);
            }

            // 0dB reference line
            ctx.strokeStyle = refLineColor;
            ctx.lineWidth = 1 * dpr;
            ctx.beginPath(); ctx.moveTo(pL, dy(0)); ctx.lineTo(w - pR, dy(0)); ctx.stroke();

            // Graph clip region
            ctx.beginPath();
            ctx.rect(pL, pT, gW, gH);
            ctx.clip();

            var numPoints = Math.max(Math.floor(gW), 300);

            // Per-band curves (semi-transparent)
            for (var bi = 0; bi < peqOverlayBands.length; bi++) {
                var band = peqOverlayBands[bi];
                if (band.enabled === false) continue;
                var coeffs = dspComputeCoeffs(band.type, band.freq || 1000, band.gain || 0, band.Q || 0.707, fs);
                var color = bandColors[bi % bandColors.length];

                ctx.strokeStyle = color + '66';
                ctx.lineWidth = 1.5 * dpr;
                ctx.beginPath();
                for (var p = 0; p <= numPoints; p++) {
                    var f = PEQ_F_MIN * Math.pow(PEQ_F_MAX / PEQ_F_MIN, p / numPoints);
                    var mag = dspBiquadMagDb(coeffs, f, fs);
                    var px = fx(f);
                    var py = dy(Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX, mag)));
                    if (p === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
                }
                ctx.stroke();
            }

            // Combined response (bold orange)
            ctx.strokeStyle = combinedColor;
            ctx.lineWidth = 2.5 * dpr;
            ctx.beginPath();
            for (var p2 = 0; p2 <= numPoints; p2++) {
                var f2 = PEQ_F_MIN * Math.pow(PEQ_F_MAX / PEQ_F_MIN, p2 / numPoints);
                var totalDb = 0;
                for (var bi2 = 0; bi2 < peqOverlayBands.length; bi2++) {
                    if (peqOverlayBands[bi2].enabled === false) continue;
                    var c2 = dspComputeCoeffs(peqOverlayBands[bi2].type, peqOverlayBands[bi2].freq || 1000,
                        peqOverlayBands[bi2].gain || 0, peqOverlayBands[bi2].Q || 0.707, fs);
                    totalDb += dspBiquadMagDb(c2, f2, fs);
                }
                var x2 = fx(f2);
                var y2 = dy(Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX, totalDb)));
                if (p2 === 0) ctx.moveTo(x2, y2); else ctx.lineTo(x2, y2);
            }
            ctx.stroke();

            ctx.restore();

            // Control points (drawn outside clip)
            for (var ci = 0; ci < peqOverlayBands.length; ci++) {
                var cb = peqOverlayBands[ci];
                if (cb.enabled === false) continue;
                var cpx = fx(Math.max(PEQ_F_MIN, Math.min(PEQ_F_MAX, cb.freq || 1000)));
                var cpy = dy(Math.max(PEQ_DB_MIN, Math.min(PEQ_DB_MAX, cb.gain || 0)));
                var ccolor = bandColors[ci % bandColors.length];
                var isActive = (_peqDragBand === ci);
                var r = (isActive ? 9 : 7) * dpr;

                ctx.beginPath();
                ctx.arc(cpx, cpy, r, 0, 2 * Math.PI);
                ctx.fillStyle = ccolor;
                ctx.globalAlpha = isActive ? 1.0 : 0.85;
                ctx.fill();
                ctx.globalAlpha = 1.0;
                ctx.strokeStyle = isDark ? '#fff' : '#000';
                ctx.lineWidth = 1.5 * dpr;
                ctx.stroke();

                // Band number label inside circle
                ctx.fillStyle = '#fff';
                ctx.font = 'bold ' + (8 * dpr) + 'px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText((ci + 1) + '', cpx, cpy + 3 * dpr);
            }
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
                body: JSON.stringify({ ch: channel, type: 'COMPRESSOR', thresholdDb: params.thresholdDb, ratio: params.ratio, attackMs: params.attackMs, releaseMs: params.releaseMs, kneeDb: params.kneeDb, makeupGainDb: params.makeupGainDb })
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
            var params = {
                thresholdDb: parseFloat(document.getElementById('limThreshold').value),
                attackMs: parseFloat(document.getElementById('limAttack').value),
                releaseMs: parseFloat(document.getElementById('limRelease').value)
            };
            apiFetch('/api/output/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ch: channel, type: 'LIMITER', thresholdDb: params.thresholdDb, attackMs: params.attackMs, releaseMs: params.releaseMs })
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
                } else if (action === 'peq-rew-export') {
                    peqHandleRewExport();
                } else if (action === 'peq-preset-save') {
                    peqOverlayQuickSave();
                } else if (action === 'peq-preset-load') {
                    peqOverlayQuickLoad();
                } else if (action === 'peq-ab-select') {
                    peqAbSelect(el.dataset.slot);
                } else if (action === 'xover-apply') {
                    applyXover(parseInt(el.dataset.channel));
                } else if (action === 'compressor-apply') {
                    applyCompressor(parseInt(el.dataset.channel));
                } else if (action === 'limiter-apply') {
                    applyLimiter(parseInt(el.dataset.channel));
                } else if (action === 'noise-gate-apply') {
                    applyNoiseGate(parseInt(el.dataset.channel));
                } else if (action === 'linkwitz-apply') {
                    applyLinkwitz(parseInt(el.dataset.channel));
                } else if (action === 'multiband-apply') {
                    applyMultibandComp(parseInt(el.dataset.channel));
                } else if (action === 'biquad-apply') {
                    applyCustomBiquad(parseInt(el.dataset.channel));
                } else if (action === 'fir-apply') {
                    applyFirUpload(parseInt(el.dataset.channel));
                } else if (action === 'wav-ir-apply') {
                    applyWavIr(parseInt(el.dataset.channel));
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

                if (action === 'peq-copy-channel') {
                    peqCopyFromChannel(el.value);
                } else if (action === 'peq-rew-import') {
                    peqHandleRewImport(el.files && el.files[0]);
                    el.value = '';  // allow re-import of same file
                } else if (action === 'fir-file-select') {
                    var chanFir = parseInt(el.dataset.channel);
                    _pendingFirFile = el.files && el.files[0];
                    var nameEl = document.getElementById('firFileName');
                    if (nameEl && _pendingFirFile) nameEl.textContent = _pendingFirFile.name;
                    el.value = '';
                } else if (action === 'wav-ir-file-select') {
                    var chanWav = parseInt(el.dataset.channel);
                    _pendingWavFile = el.files && el.files[0];
                    var wavNameEl = document.getElementById('wavIrFileName');
                    if (wavNameEl && _pendingWavFile) wavNameEl.textContent = _pendingWavFile.name;
                    el.value = '';
                } else if (action === 'peq-update-band') {
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

        // ===== Shared overlay scaffold helper =====
        function _getOrCreateOverlay() {
            var overlay = document.getElementById('peqOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'peqOverlay';
                overlay.className = 'peq-overlay';
                document.body.appendChild(overlay);
                peqOverlayInitDelegation(overlay);
            }
            return overlay;
        }

        function _overlayHeader(title) {
            var h = '<div class="peq-overlay-header">';
            h += '  <span class="peq-overlay-title">' + escapeHtml(title) + '</span>';
            h += '  <button class="peq-overlay-close" data-action="peq-close">';
            h += '    <svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor" aria-hidden="true"><path d="M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"/></svg>';
            h += '  </button>';
            h += '</div>';
            return h;
        }

        // ===== Input Compressor overlay =====
        function openInputCompressor(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Compressor \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'compThreshold', -60, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'compRatio', 1, 20, 4, 0.5, ':1');
            html += peqControlRow('Attack', 'compAttack', 0.1, 200, 10, 0.1, 'ms');
            html += peqControlRow('Release', 'compRelease', 10, 2000, 100, 1, 'ms');
            html += peqControlRow('Knee', 'compKnee', 0, 20, 6, 0.5, 'dB');
            html += peqControlRow('Makeup', 'compMakeup', 0, 24, 0, 0.5, 'dB');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="compressor-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        // ===== Input Limiter overlay =====
        function openInputLimiter(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Limiter \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'limThreshold', -40, 0, -3, 0.5, 'dBFS');
            html += peqControlRow('Attack', 'limAttack', 0.01, 50, 0.1, 0.01, 'ms');
            html += peqControlRow('Release', 'limRelease', 1, 1000, 50, 1, 'ms');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="limiter-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        // ===== Noise Gate overlay =====
        // Pending FIR file to upload
        var _pendingNoiseGateCh = -1;

        function openInputNoiseGate(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Noise Gate \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += peqControlRow('Threshold', 'ngThreshold', -90, 0, -60, 0.5, 'dB');
            html += peqControlRow('Attack', 'ngAttack', 0.1, 200, 1, 0.1, 'ms');
            html += peqControlRow('Hold', 'ngHold', 0, 1000, 50, 1, 'ms');
            html += peqControlRow('Release', 'ngRelease', 1, 2000, 100, 1, 'ms');
            html += peqControlRow('Range', 'ngRange', -90, 0, -80, 1, 'dB');
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="noise-gate-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyNoiseGate(channel) {
            var params = {
                thresholdDb: parseFloat(document.getElementById('ngThreshold').value),
                attackMs: parseFloat(document.getElementById('ngAttack').value),
                holdMs: parseFloat(document.getElementById('ngHold').value),
                releaseMs: parseFloat(document.getElementById('ngRelease').value),
                rangeDb: parseFloat(document.getElementById('ngRange').value)
            };
            apiFetch('/api/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ channel: channel, type: 'NOISE_GATE', params: params })
            })
            .then(function() {
                showToast('Noise Gate applied to Lane ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Noise Gate apply failed', 'error'); });
        }

        // ===== Linkwitz Transform overlay =====
        function openInputLinkwitz(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Linkwitz Transform \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += '<p style="margin:0 0 12px;font-size:0.85em;color:var(--text-muted)">Transforms speaker response from (F0, Q0) to target (Fp, Qp)</p>';
            html += peqControlRow('Source F0', 'lkF0', 5, 200, 40, 1, 'Hz');
            html += peqControlRow('Source Q0', 'lkQ0', 0.3, 2.0, 0.7, 0.01, '');
            html += peqControlRow('Target Fp', 'lkFp', 5, 200, 20, 1, 'Hz');
            html += peqControlRow('Target Qp', 'lkQp', 0.3, 2.0, 0.5, 0.01, '');
            // Response graph canvas
            html += '  <canvas id="linkwitzCanvas" class="peq-overlay-canvas" style="height:120px;margin-top:8px;"></canvas>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="linkwitz-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyLinkwitz(channel) {
            var params = {
                f0Hz: parseFloat(document.getElementById('lkF0').value),
                q0: parseFloat(document.getElementById('lkQ0').value),
                fpHz: parseFloat(document.getElementById('lkFp').value),
                qp: parseFloat(document.getElementById('lkQp').value)
            };
            apiFetch('/api/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ channel: channel, type: 'LINKWITZ_TRANSFORM', params: params })
            })
            .then(function() {
                showToast('Linkwitz Transform applied to Lane ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Linkwitz Transform apply failed', 'error'); });
        }

        // ===== Multi-band Compressor overlay =====
        function openInputMultibandComp(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Multi-band Compressor \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:80px">Bands</label>';
            html += '  <select class="form-input" id="mbcNumBands" style="max-width:100px">';
            html += '    <option value="2">2 bands</option>';
            html += '    <option value="3" selected>3 bands</option>';
            html += '    <option value="4">4 bands</option>';
            html += '  </select>';
            html += '</div>';
            // Band 1 (Low)
            html += '<div style="border:1px solid var(--border);border-radius:4px;padding:8px;margin-bottom:8px;">';
            html += '<div style="font-size:0.8em;font-weight:600;margin-bottom:6px;">Band 1 (Low)</div>';
            html += peqControlRow('Xover', 'mbcXo1', 20, 2000, 200, 10, 'Hz');
            html += peqControlRow('Threshold', 'mbcThr1', -60, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'mbcRatio1', 1, 20, 4, 0.5, ':1');
            html += '</div>';
            // Band 2 (Mid)
            html += '<div style="border:1px solid var(--border);border-radius:4px;padding:8px;margin-bottom:8px;">';
            html += '<div style="font-size:0.8em;font-weight:600;margin-bottom:6px;">Band 2 (Mid)</div>';
            html += peqControlRow('Xover', 'mbcXo2', 200, 10000, 2000, 50, 'Hz');
            html += peqControlRow('Threshold', 'mbcThr2', -60, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'mbcRatio2', 1, 20, 4, 0.5, ':1');
            html += '</div>';
            // Band 3 (High)
            html += '<div style="border:1px solid var(--border);border-radius:4px;padding:8px;margin-bottom:8px;">';
            html += '<div style="font-size:0.8em;font-weight:600;margin-bottom:6px;">Band 3 (High)</div>';
            html += peqControlRow('Threshold', 'mbcThr3', -60, 0, -20, 0.5, 'dB');
            html += peqControlRow('Ratio', 'mbcRatio3', 1, 20, 4, 0.5, ':1');
            html += '</div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="multiband-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyMultibandComp(channel) {
            var numBands = parseInt(document.getElementById('mbcNumBands').value);
            var params = {
                numBands: numBands,
                crossovers: [
                    parseFloat(document.getElementById('mbcXo1').value),
                    parseFloat(document.getElementById('mbcXo2').value)
                ],
                thresholds: [
                    parseFloat(document.getElementById('mbcThr1').value),
                    parseFloat(document.getElementById('mbcThr2').value),
                    parseFloat(document.getElementById('mbcThr3').value)
                ],
                ratios: [
                    parseFloat(document.getElementById('mbcRatio1').value),
                    parseFloat(document.getElementById('mbcRatio2').value),
                    parseFloat(document.getElementById('mbcRatio3').value)
                ]
            };
            apiFetch('/api/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ channel: channel, type: 'MULTIBAND_COMP', params: params })
            })
            .then(function() {
                showToast('Multi-band Comp applied to Lane ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Multi-band Comp apply failed', 'error'); });
        }

        // ===== Custom Biquad overlay =====
        function openInputCustomBiquad(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('Custom Biquad \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += '<p style="margin:0 0 12px;font-size:0.85em;color:var(--text-muted)">Direct coefficient entry. Coefficients are normalized (a0=1).</p>';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:40px">b0</label>';
            html += '  <input type="number" class="form-input" id="bqB0" value="1" step="any" style="max-width:160px">';
            html += '</div>';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:40px">b1</label>';
            html += '  <input type="number" class="form-input" id="bqB1" value="0" step="any" style="max-width:160px">';
            html += '</div>';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:40px">b2</label>';
            html += '  <input type="number" class="form-input" id="bqB2" value="0" step="any" style="max-width:160px">';
            html += '</div>';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:40px">a1</label>';
            html += '  <input type="number" class="form-input" id="bqA1" value="0" step="any" style="max-width:160px">';
            html += '</div>';
            html += '<div class="channel-control-row" style="margin-bottom:8px;">';
            html += '  <label class="channel-control-label" style="min-width:40px">a2</label>';
            html += '  <input type="number" class="form-input" id="bqA2" value="0" step="any" style="max-width:160px">';
            html += '</div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="biquad-apply" data-channel="' + lane + '">Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
        }

        function applyCustomBiquad(channel) {
            var params = {
                b0: parseFloat(document.getElementById('bqB0').value),
                b1: parseFloat(document.getElementById('bqB1').value),
                b2: parseFloat(document.getElementById('bqB2').value),
                a1: parseFloat(document.getElementById('bqA1').value),
                a2: parseFloat(document.getElementById('bqA2').value)
            };
            apiFetch('/api/dsp/stage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ channel: channel, type: 'CUSTOM_BIQUAD', params: params })
            })
            .then(function() {
                showToast('Custom Biquad applied to Lane ' + channel, 'success');
                closePeqOverlay();
            })
            .catch(function() { showToast('Custom Biquad apply failed', 'error'); });
        }

        // ===== FIR Upload overlay =====
        var _pendingFirFile = null;

        function openInputFirUpload(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('FIR Filter Upload \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += '<p style="margin:0 0 12px;font-size:0.85em;color:var(--text-muted)">Upload FIR coefficients as a plain text file (.txt), one coefficient per line. Max 256 taps.</p>';
            html += '<div style="margin-bottom:12px;">';
            html += '  <label class="btn btn-sm btn-secondary" style="cursor:pointer">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2M18,20H6V4H13V9H18V20M13,13V18H11V13H8L12,9L16,13H13Z"/></svg>';
            html += '    Choose File';
            html += '    <input type="file" accept=".txt" data-action="fir-file-select" data-channel="' + lane + '" style="display:none">';
            html += '  </label>';
            html += '  <span id="firFileName" style="margin-left:8px;font-size:0.85em;color:var(--text-muted)">No file selected</span>';
            html += '</div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="fir-apply" data-channel="' + lane + '">Upload &amp; Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
            _pendingFirFile = null;
        }

        function applyFirUpload(channel) {
            if (!_pendingFirFile) {
                showToast('Please select a FIR coefficient file first', 'warning');
                return;
            }
            var reader = new FileReader();
            reader.onload = function(e) {
                var lines = e.target.result.split('\n');
                var coeffs = [];
                for (var i = 0; i < lines.length; i++) {
                    var v = parseFloat(lines[i].trim());
                    if (!isNaN(v)) coeffs.push(v);
                }
                if (coeffs.length === 0) {
                    showToast('No valid coefficients found in file', 'error');
                    return;
                }
                if (coeffs.length > 256) {
                    showToast('FIR file has ' + coeffs.length + ' taps — truncating to 256', 'warning');
                    coeffs = coeffs.slice(0, 256);
                }
                apiFetch('/api/dsp/stage', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ channel: channel, type: 'FIR', params: { taps: coeffs } })
                })
                .then(function() {
                    showToast('FIR filter loaded (' + coeffs.length + ' taps) on Lane ' + channel, 'success');
                    closePeqOverlay();
                })
                .catch(function() { showToast('FIR upload failed', 'error'); });
            };
            reader.readAsText(_pendingFirFile);
            _pendingFirFile = null;
        }

        // ===== WAV IR Upload overlay (Convolution) =====
        var _pendingWavFile = null;

        function openInputWavIr(lane) {
            var overlay = _getOrCreateOverlay();
            var html = _overlayHeader('WAV IR Upload \u2014 Input Lane ' + lane);
            html += '<div style="padding:16px;">';
            html += '<p style="margin:0 0 12px;font-size:0.85em;color:var(--text-muted)">Upload a WAV impulse response for convolution reverb. Max 24576 samples (~0.51s at 48kHz).</p>';
            html += '<div style="margin-bottom:12px;">';
            html += '  <label class="btn btn-sm btn-secondary" style="cursor:pointer">';
            html += '    <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M12,3V13.55C11.41,13.21 10.73,13 10,13A3,3 0 0,0 7,16A3,3 0 0,0 10,19A3,3 0 0,0 13,16V7H15V5H13V3H12M10,17A1,1 0 0,1 9,16A1,1 0 0,1 10,15A1,1 0 0,1 11,16A1,1 0 0,1 10,17Z"/></svg>';
            html += '    Choose WAV';
            html += '    <input type="file" accept=".wav" data-action="wav-ir-file-select" data-channel="' + lane + '" style="display:none">';
            html += '  </label>';
            html += '  <span id="wavIrFileName" style="margin-left:8px;font-size:0.85em;color:var(--text-muted)">No file selected</span>';
            html += '</div>';
            html += '  <div style="display:flex;gap:6px;margin-top:12px;">';
            html += '    <button class="btn btn-sm btn-primary" data-action="wav-ir-apply" data-channel="' + lane + '">Upload &amp; Apply</button>';
            html += '    <button class="btn btn-sm btn-secondary" data-action="peq-close">Cancel</button>';
            html += '  </div>';
            html += '</div>';
            overlay.innerHTML = html;
            overlay.style.display = 'flex';
            _pendingWavFile = null;
        }

        function applyWavIr(channel) {
            if (!_pendingWavFile) {
                showToast('Please select a WAV file first', 'warning');
                return;
            }
            var formData = new FormData();
            formData.append('file', _pendingWavFile);
            fetch('/api/v1/dsp/convolution/upload?ch=' + channel, {
                method: 'POST',
                body: formData
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                if (d.success) {
                    showToast('WAV IR loaded (' + (d.tapsLoaded || '?') + ' taps) on Lane ' + channel, 'success');
                    closePeqOverlay();
                } else {
                    showToast('WAV IR upload failed: ' + (d.message || 'unknown error'), 'error');
                }
            })
            .catch(function() { showToast('WAV IR upload failed', 'error'); });
            _pendingWavFile = null;
        }
