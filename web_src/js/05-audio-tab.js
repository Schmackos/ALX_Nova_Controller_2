        // ===== Audio Tab Controller =====
        // Unified Audio tab with sub-views: Inputs | Matrix | Outputs | SigGen
        // Dynamically populated from HAL device channel map.

        // Channel map state (received from firmware via WS)
        let audioChannelMap = null;
        let audioSubView = 'inputs';  // 'inputs' | 'matrix' | 'outputs' | 'siggen'

        // Per-channel VU state for inputs (lane-indexed) and outputs (sink-indexed)
        let inputVuCurrent = [], inputVuTarget = [];
        let outputVuCurrent = [], outputVuTarget = [];
        let audioTabAnimId = null;

        // ===== Channel Map Handler =====
        function handleAudioChannelMap(data) {
            audioChannelMap = data;

            // Resize shared audio arrays (waveform, spectrum, VU) to match input count
            resizeAudioArrays(data.inputs ? data.inputs.length : 0);

            // Resize VU arrays to match channel count
            while (inputVuCurrent.length < (data.inputs || []).length) {
                inputVuCurrent.push([0, 0]);
                inputVuTarget.push([0, 0]);
            }
            while (outputVuCurrent.length < (data.outputs || []).length * 2) {
                outputVuCurrent.push(0);
                outputVuTarget.push(0);
            }

            // Re-render current sub-view if audio tab is active
            if (currentActiveTab === 'audio') {
                renderAudioSubView();
            }
        }

        // ===== Sub-View Navigation =====
        function switchAudioSubView(view) {
            audioSubView = view;
            // Update sub-nav buttons
            document.querySelectorAll('.audio-subnav-btn').forEach(function(btn) {
                btn.classList.toggle('active', btn.dataset.view === view);
            });
            // Update sub-view panels
            document.querySelectorAll('.audio-subview').forEach(function(panel) {
                panel.classList.toggle('active', panel.id === 'audio-sv-' + view);
            });

            // Subscribe to audio stream when inputs or outputs sub-view is active
            if ((view === 'inputs' || view === 'outputs') && ws && ws.readyState === WebSocket.OPEN) {
                if (!audioSubscribed) {
                    audioSubscribed = true;
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                }
            }

            renderAudioSubView();
        }

        function renderAudioSubView() {
            if (!audioChannelMap) return;
            switch (audioSubView) {
                case 'inputs':  renderInputStrips(); break;
                case 'matrix':  renderMatrixGrid(); break;
                case 'outputs': renderOutputStrips(); break;
                case 'siggen':  renderSigGenView(); break;
            }
        }

        // ===== Input Channel Strips =====
        function renderInputStrips() {
            var container = document.getElementById('audio-inputs-container');
            if (!container || !audioChannelMap) return;

            var inputs = audioChannelMap.inputs || [];
            if (container.dataset.rendered === String(inputs.length)) return;  // Already rendered

            var html = '';
            for (var i = 0; i < inputs.length; i++) {
                var inp = inputs[i];
                var statusClass = inp.ready ? 'status-ok' : 'status-off';
                var statusText = inp.ready ? 'OK' : 'Offline';

                html += '<div class="channel-strip" data-lane="' + inp.lane + '">';
                html += '  <div class="channel-strip-header">';
                html += '    <span class="channel-device-name">' + escapeHtml(inp.deviceName) + '</span>';
                html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
                html += '  </div>';

                // Stereo VU meters
                html += '  <div class="channel-vu-pair">';
                html += '    <div class="channel-vu-wrapper">';
                html += '      <canvas class="channel-vu-canvas" id="inputVu' + inp.lane + 'L" width="24" height="120"></canvas>';
                html += '      <div class="channel-vu-label">L</div>';
                html += '    </div>';
                html += '    <div class="channel-vu-wrapper">';
                html += '      <canvas class="channel-vu-canvas" id="inputVu' + inp.lane + 'R" width="24" height="120"></canvas>';
                html += '      <div class="channel-vu-label">R</div>';
                html += '    </div>';
                html += '    <div class="channel-vu-readout" id="inputVuReadout' + inp.lane + '">-- dB</div>';
                html += '  </div>';

                // Gain slider
                html += '  <div class="channel-control-row">';
                html += '    <label class="channel-control-label">Gain</label>';
                html += '    <input type="range" class="channel-gain-slider" id="inputGain' + inp.lane + '" min="-72" max="12" step="0.5" value="0"';
                html += '      oninput="onInputGainChange(' + inp.lane + ',this.value)">';
                html += '    <span class="channel-gain-value" id="inputGainVal' + inp.lane + '">0.0 dB</span>';
                html += '  </div>';

                // Mute / Phase / Solo buttons
                html += '  <div class="channel-button-row">';
                html += '    <button class="channel-btn" id="inputMute' + inp.lane + '" onclick="toggleInputMute(' + inp.lane + ')">Mute</button>';
                html += '    <button class="channel-btn" id="inputPhase' + inp.lane + '" onclick="toggleInputPhase(' + inp.lane + ')">Phase</button>';
                html += '  </div>';

                // PEQ button
                html += '  <div class="channel-dsp-row">';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openInputPeq(' + inp.lane + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
                html += '      PEQ';
                html += '    </button>';
                html += '  </div>';

                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = String(inputs.length);
        }

        // ===== Output Channel Strips =====
        function renderOutputStrips() {
            var container = document.getElementById('audio-outputs-container');
            if (!container || !audioChannelMap) return;

            var outputs = audioChannelMap.outputs || [];
            if (container.dataset.rendered === String(outputs.length)) return;

            var html = '';
            for (var i = 0; i < outputs.length; i++) {
                var out = outputs[i];
                var statusClass = out.ready ? 'status-ok' : 'status-off';
                var statusText = out.ready ? 'OK' : 'Offline';
                var hasHwVol = (out.capabilities & 1);  // HAL_CAP_HW_VOLUME
                var hasHwMute = (out.capabilities & 4);  // HAL_CAP_MUTE

                html += '<div class="channel-strip channel-strip-output" data-sink="' + out.index + '">';
                html += '  <div class="channel-strip-header">';
                html += '    <span class="channel-device-name">' + escapeHtml(out.name) + '</span>';
                html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
                html += '  </div>';
                html += '  <div class="channel-strip-sub">Ch ' + out.firstChannel + '-' + (out.firstChannel + out.channels - 1) + '</div>';

                // VU meters
                html += '  <div class="channel-vu-pair">';
                for (var ch = 0; ch < out.channels && ch < 2; ch++) {
                    var label = out.channels > 1 ? (ch === 0 ? 'L' : 'R') : '';
                    html += '    <div class="channel-vu-wrapper">';
                    html += '      <canvas class="channel-vu-canvas" id="outputVu' + out.index + 'c' + ch + '" width="24" height="120"></canvas>';
                    if (label) html += '      <div class="channel-vu-label">' + label + '</div>';
                    html += '    </div>';
                }
                html += '    <div class="channel-vu-readout" id="outputVuReadout' + out.index + '">-- dB</div>';
                html += '  </div>';

                // Gain / HW Volume
                if (hasHwVol) {
                    html += '  <div class="channel-control-row">';
                    html += '    <label class="channel-control-label">HW Vol</label>';
                    html += '    <input type="range" class="channel-gain-slider" id="outputHwVol' + out.index + '" min="0" max="100" step="1" value="80"';
                    html += '      oninput="onOutputHwVolChange(' + out.index + ',this.value)">';
                    html += '    <span class="channel-gain-value" id="outputHwVolVal' + out.index + '">80%</span>';
                    html += '  </div>';
                }

                html += '  <div class="channel-control-row">';
                html += '    <label class="channel-control-label">Gain</label>';
                html += '    <input type="range" class="channel-gain-slider" id="outputGain' + out.index + '" min="-72" max="12" step="0.5" value="0"';
                html += '      oninput="onOutputGainChange(' + out.index + ',this.value)">';
                html += '    <span class="channel-gain-value" id="outputGainVal' + out.index + '">0.0 dB</span>';
                html += '  </div>';

                // Mute / Phase / Solo
                html += '  <div class="channel-button-row">';
                html += '    <button class="channel-btn" id="outputMute' + out.index + '" onclick="toggleOutputMute(' + out.index + ')">' + (hasHwMute ? 'HW Mute' : 'Mute') + '</button>';
                html += '    <button class="channel-btn" id="outputPhase' + out.index + '" onclick="toggleOutputPhase(' + out.index + ')">Phase</button>';
                html += '  </div>';

                // DSP controls
                html += '  <div class="channel-dsp-section">';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputPeq(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
                html += '      PEQ 10-band</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputCrossover(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M16,11.78L20.24,4.45L21.97,5.45L16.74,14.5L10.23,10.75L5.46,19H22V21H2V3H4V17.54L9.5,8L16,11.78Z"/></svg>';
                html += '      Crossover</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputCompressor(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M5,4V7H10.5V19H13.5V7H19V4H5Z"/></svg>';
                html += '      Compressor</button>';
                html += '    <button class="channel-btn channel-btn-wide" onclick="openOutputLimiter(' + out.firstChannel + ')">';
                html += '      <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M2,12A10,10 0 0,1 12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22A10,10 0 0,1 2,12M4,12A8,8 0 0,0 12,20A8,8 0 0,0 20,12A8,8 0 0,0 12,4A8,8 0 0,0 4,12M7,12L12,7V17L7,12Z"/></svg>';
                html += '      Limiter</button>';

                // Delay
                html += '    <div class="channel-control-row" style="margin-top:6px;">';
                html += '      <label class="channel-control-label">Delay</label>';
                html += '      <input type="number" class="channel-delay-input" id="outputDelay' + out.firstChannel + '" min="0" max="10" step="0.01" value="0.00"';
                html += '        onchange="onOutputDelayChange(' + out.firstChannel + ',this.value)">';
                html += '      <span class="channel-gain-value">ms</span>';
                html += '    </div>';
                html += '  </div>';

                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = String(outputs.length);
        }

        // ===== Matrix Grid =====
        function renderMatrixGrid() {
            var container = document.getElementById('audio-matrix-container');
            if (!container || !audioChannelMap) return;

            var matSize = audioChannelMap.matrixInputs || 8;
            var inputs = audioChannelMap.inputs || [];
            var outputs = audioChannelMap.outputs || [];
            var matrixGains = audioChannelMap.matrix || [];

            // Build column headers from output sinks
            var colLabels = [];
            for (var o = 0; o < matSize; o++) {
                var label = 'OUT ' + (o + 1);
                for (var si = 0; si < outputs.length; si++) {
                    var sk = outputs[si];
                    if (o >= sk.firstChannel && o < sk.firstChannel + sk.channels) {
                        var chOff = o - sk.firstChannel;
                        label = sk.name + (sk.channels > 1 ? (chOff === 0 ? ' L' : ' R') : '');
                        break;
                    }
                }
                colLabels.push(label);
            }

            // Build row labels from input lanes
            var rowLabels = [];
            for (var r = 0; r < matSize; r++) {
                var laneIdx = Math.floor(r / 2);
                var ch = r % 2;
                if (laneIdx < inputs.length) {
                    rowLabels.push(inputs[laneIdx].deviceName + (ch === 0 ? ' L' : ' R'));
                } else {
                    rowLabels.push('IN ' + (r + 1));
                }
            }

            var html = '<table class="matrix-table"><thead><tr><th></th>';
            for (var c = 0; c < matSize; c++) {
                html += '<th class="matrix-col-hdr">' + escapeHtml(colLabels[c]) + '</th>';
            }
            html += '</tr></thead><tbody>';

            for (var row = 0; row < matSize; row++) {
                html += '<tr><td class="matrix-row-hdr">' + escapeHtml(rowLabels[row]) + '</td>';
                for (var col = 0; col < matSize; col++) {
                    var gain = (matrixGains[col] && matrixGains[col][row] !== undefined) ? parseFloat(matrixGains[col][row]) : 0;
                    var active = gain > 0.0001 || gain < -0.0001;
                    var displayVal = active ? (gain >= 1.0 ? '+' + (20 * Math.log10(gain)).toFixed(1) : (20 * Math.log10(Math.max(gain, 0.0001))).toFixed(1)) : '--';
                    var cellClass = 'matrix-cell' + (active ? ' matrix-active' : '');
                    html += '<td class="' + cellClass + '" data-out="' + col + '" data-in="' + row + '" onclick="onMatrixCellClick(' + col + ',' + row + ')">';
                    html += displayVal;
                    html += '</td>';
                }
                html += '</tr>';
            }
            html += '</tbody></table>';

            // Quick presets
            html += '<div class="matrix-presets">';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixPreset1to1()">1:1 Pass</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixPresetClear()">Clear All</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixSave()">Save</button>';
            html += '  <button class="btn btn-secondary btn-sm" onclick="matrixLoad()">Load</button>';
            html += '</div>';

            container.innerHTML = html;
        }

        // ===== Matrix Cell Click — Popup Gain Slider =====
        function onMatrixCellClick(outCh, inCh) {
            var currentGain = 0;
            if (audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                currentGain = parseFloat(audioChannelMap.matrix[outCh][inCh]) || 0;
            }
            var currentDb = currentGain > 0.0001 ? (20 * Math.log10(currentGain)).toFixed(1) : '-72.0';

            var popup = document.getElementById('matrixGainPopup');
            if (!popup) {
                popup = document.createElement('div');
                popup.id = 'matrixGainPopup';
                popup.className = 'matrix-gain-popup';
                document.body.appendChild(popup);
            }

            popup.innerHTML = '<div class="matrix-gain-popup-inner">' +
                '<label>OUT ' + (outCh + 1) + ' \u2190 IN ' + (inCh + 1) + '</label>' +
                '<input type="range" id="matrixGainSlider" min="-72" max="12" step="0.5" value="' + currentDb + '" oninput="onMatrixGainSlide(' + outCh + ',' + inCh + ',this.value)">' +
                '<span id="matrixGainDbVal">' + currentDb + ' dB</span>' +
                '<div style="display:flex;gap:4px;margin-top:6px;">' +
                '<button class="btn btn-sm btn-primary" onclick="setMatrixGainDb(' + outCh + ',' + inCh + ',0);closeMatrixPopup()">0 dB</button>' +
                '<button class="btn btn-sm btn-secondary" onclick="setMatrixGainDb(' + outCh + ',' + inCh + ',-72);closeMatrixPopup()">Off</button>' +
                '<button class="btn btn-sm btn-secondary" onclick="closeMatrixPopup()">Close</button>' +
                '</div></div>';
            popup.style.display = 'block';
        }

        function onMatrixGainSlide(outCh, inCh, dbVal) {
            var label = document.getElementById('matrixGainDbVal');
            if (label) label.textContent = parseFloat(dbVal).toFixed(1) + ' dB';
            setMatrixGainDb(outCh, inCh, parseFloat(dbVal));
        }

        function closeMatrixPopup() {
            var popup = document.getElementById('matrixGainPopup');
            if (popup) popup.style.display = 'none';
        }

        function setMatrixGainDb(outCh, inCh, db) {
            fetch('/api/pipeline/matrix/cell', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ out: outCh, in: inCh, gainDb: db })
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                if (d.status === 'ok' && audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                    audioChannelMap.matrix[outCh][inCh] = d.gainLinear;
                    renderMatrixGrid();
                }
            })
            .catch(function() {});
            // Optimistic local update for immediate UI feedback
            if (audioChannelMap && audioChannelMap.matrix && audioChannelMap.matrix[outCh]) {
                audioChannelMap.matrix[outCh][inCh] = Math.pow(10, db / 20);
            }
            renderMatrixGrid();
        }

        // Matrix presets — use existing REST endpoint
        function matrixPreset1to1() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var i = 0; i < size; i++) setMatrixGainDb(i, i, 0);
        }
        function matrixPresetClear() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var o = 0; o < size; o++)
                for (var i = 0; i < size; i++)
                    setMatrixGainDb(o, i, -96);
        }
        function matrixSave() {
            fetch('/api/pipeline/matrix/save', { method: 'POST' })
                .then(function() { showToast('Matrix saved', 'success'); })
                .catch(function() { showToast('Save failed', 'error'); });
        }
        function matrixLoad() {
            fetch('/api/pipeline/matrix/load', { method: 'POST' })
                .then(function() {
                    showToast('Matrix loaded', 'success');
                    // Refresh channel map to get updated matrix
                    fetch('/api/pipeline/matrix').then(function(r) { return r.json(); }).then(function(d) {
                        if (audioChannelMap) audioChannelMap.matrix = d.matrix;
                        renderMatrixGrid();
                    });
                })
                .catch(function() { showToast('Load failed', 'error'); });
        }

        // ===== Signal Generator Sub-View =====
        function renderSigGenView() {
            // Signal gen sub-view delegates to existing siggen controls (13-signal-gen.js)
            // The HTML is statically in the siggen sub-view panel
        }

        // ===== Input Controls =====
        function onInputGainChange(lane, val) {
            var label = document.getElementById('inputGainVal' + lane);
            if (label) label.textContent = parseFloat(val).toFixed(1) + ' dB';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputGain', lane: lane, db: parseFloat(val) }));
            }
        }

        function toggleInputMute(lane) {
            var btn = document.getElementById('inputMute' + lane);
            if (!btn) return;
            var muted = !btn.classList.contains('active');
            btn.classList.toggle('active', muted);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputMute', lane: lane, muted: muted }));
            }
        }

        function toggleInputPhase(lane) {
            var btn = document.getElementById('inputPhase' + lane);
            if (!btn) return;
            var inverted = !btn.classList.contains('active');
            btn.classList.toggle('active', inverted);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputPhase', lane: lane, inverted: inverted }));
            }
        }

        // ===== Output Controls =====
        function onOutputGainChange(sinkIdx, val) {
            var label = document.getElementById('outputGainVal' + sinkIdx);
            if (label) label.textContent = parseFloat(val).toFixed(1) + ' dB';
            // Output gain maps to per-output DSP gain stage
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputGain', channel: out.firstChannel, db: parseFloat(val) }));
            }
        }

        function onOutputHwVolChange(sinkIdx, val) {
            var label = document.getElementById('outputHwVolVal' + sinkIdx);
            if (label) label.textContent = val + '%';
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputHwVolume', channel: out.firstChannel, volume: parseInt(val) }));
            }
        }

        function toggleOutputMute(sinkIdx) {
            var btn = document.getElementById('outputMute' + sinkIdx);
            if (!btn) return;
            var muted = !btn.classList.contains('active');
            btn.classList.toggle('active', muted);
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputMute', channel: out.firstChannel, muted: muted }));
            }
        }

        function toggleOutputPhase(sinkIdx) {
            var btn = document.getElementById('outputPhase' + sinkIdx);
            if (!btn) return;
            var inverted = !btn.classList.contains('active');
            btn.classList.toggle('active', inverted);
            var out = audioChannelMap && audioChannelMap.outputs ? audioChannelMap.outputs[sinkIdx] : null;
            if (out && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputPhase', channel: out.firstChannel, inverted: inverted }));
            }
        }

        function onOutputDelayChange(channel, val) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setOutputDelay', channel: channel, ms: parseFloat(val) }));
            }
        }

        // PEQ / DSP overlay openers defined in 06-peq-overlay.js:
        // openInputPeq, openOutputPeq, openOutputCrossover, openOutputCompressor, openOutputLimiter

        // ===== VU Meter Drawing for Channel Strips =====
        function drawChannelVu(canvasId, value) {
            var canvas = document.getElementById(canvasId);
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var w = canvas.width, h = canvas.height;
            ctx.clearRect(0, 0, w, h);

            // Background
            ctx.fillStyle = 'var(--bg-card)';
            ctx.fillRect(0, 0, w, h);

            // VU bar (bottom-up)
            var pct = Math.max(0, Math.min(1, (value + 60) / 60));  // -60dB to 0dB range
            var barH = Math.round(pct * h);
            if (barH > 0) {
                var grad = ctx.createLinearGradient(0, h, 0, 0);
                grad.addColorStop(0, '#4CAF50');
                grad.addColorStop(0.7, '#FFC107');
                grad.addColorStop(1.0, '#F44336');
                ctx.fillStyle = grad;
                ctx.fillRect(2, h - barH, w - 4, barH);
            }
        }

        // ===== Audio Levels Handler (extends existing audioLevels route) =====
        function audioTabUpdateLevels(data) {
            if (!audioChannelMap || currentActiveTab !== 'audio') return;

            if (audioSubView === 'inputs' && data.adc) {
                for (var a = 0; a < data.adc.length; a++) {
                    var ad = data.adc[a];
                    drawChannelVu('inputVu' + a + 'L', ad.vu1 || -90);
                    drawChannelVu('inputVu' + a + 'R', ad.vu2 || -90);
                    var readout = document.getElementById('inputVuReadout' + a);
                    if (readout) {
                        var dbText = (ad.dBFS || -90).toFixed(1) + ' dB';
                        if (ad.vrms1 !== undefined) {
                            var avgVrms = ((ad.vrms1 || 0) + (ad.vrms2 || 0)) / 2;
                            dbText += ' | ' + (avgVrms < 0.001 ? '0.000' : avgVrms.toFixed(3)) + ' Vrms';
                        }
                        readout.textContent = dbText;
                    }
                }
            }
            // Output sink VU meters
            if (data.sinks && audioSubView === 'outputs') {
                for (var s = 0; s < data.sinks.length; s++) {
                    var sk = data.sinks[s];
                    drawChannelVu('outputVu' + s + 'c0', sk.vuL || -90);
                    drawChannelVu('outputVu' + s + 'c1', sk.vuR || -90);
                    readout = document.getElementById('outputVuReadout' + s);
                    if (readout) {
                        var avg = ((sk.vuL || -90) + (sk.vuR || -90)) / 2;
                        readout.textContent = avg.toFixed(1) + ' dB';
                    }
                }
            }
        }

        function escapeHtml(str) {
            if (!str) return '';
            return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }
