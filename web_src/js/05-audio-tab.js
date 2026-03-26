        // ===== Audio Tab Controller =====
        // Unified Audio tab with sub-views: Inputs | Matrix | Outputs | SigGen
        // Dynamically populated from HAL device channel map.

        // Channel map state (received from firmware via WS)
        var audioChannelMap = null;
        var audioSubView = 'inputs';  // 'inputs' | 'matrix' | 'outputs' | 'siggen'

        // Previous channel map snapshot for hot-plug detection
        var _audioChannelMapHash = '';

        // Per-channel VU state for inputs (lane-indexed) and outputs (sink-indexed)
        var inputVuCurrent = [], inputVuTarget = [];
        var outputVuCurrent = [], outputVuTarget = [];
        var audioTabAnimId = null;

        // Stereo link UI state: maps lane index to boolean
        var _stereoLinkState = {};

        // ===== Channel Map Key Hash =====
        function _audioChannelHash(data) {
            if (!data) return '';
            var parts = [];
            var inputs = data.inputs || [];
            var outputs = data.outputs || [];
            for (var i = 0; i < inputs.length; i++) {
                parts.push(inputs[i].lane + ':' + inputs[i].deviceName + ':' + inputs[i].manufacturer + ':' + (inputs[i].ready ? '1' : '0'));
            }
            for (var o = 0; o < outputs.length; o++) {
                parts.push(outputs[o].index + ':' + outputs[o].name + ':' + (outputs[o].ready ? '1' : '0'));
            }
            return parts.join('|');
        }

        // ===== Channel Map Handler =====
        function handleAudioChannelMap(data) {
            var newHash = _audioChannelHash(data);
            var prevHash = _audioChannelMapHash;
            audioChannelMap = data;
            _audioChannelMapHash = newHash;

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

            // Hot-plug toast: compare new vs previous hash
            if (prevHash !== '' && newHash !== prevHash) {
                showToast('Audio devices changed', 'info');
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
            if (!audioChannelMap) {
                _renderAudioEmptyState();
                return;
            }
            switch (audioSubView) {
                case 'inputs':  renderInputStrips(); break;
                case 'matrix':  renderMatrixGrid(); break;
                case 'outputs': renderOutputStrips(); break;
                case 'siggen':  renderSigGenView(); break;
            }
        }

        function _renderAudioEmptyState() {
            var emptyMsg = '<div class="empty-state">No input/output devices detected. Connect a mezzanine board or check the Devices tab.</div>';
            var ids = ['audio-inputs-container', 'audio-matrix-container', 'audio-outputs-container'];
            for (var i = 0; i < ids.length; i++) {
                var c = document.getElementById(ids[i]);
                if (c && !c.dataset.emptyRendered) {
                    c.innerHTML = emptyMsg;
                    c.dataset.emptyRendered = '1';
                }
            }
        }

        // ===== Device Grouping Helper =====
        // Groups an array of items by a key derived from each item.
        // Returns an array of { key, items[] } in encounter order.
        function _groupBy(items, keyFn) {
            var groups = [];
            var keyMap = {};
            for (var i = 0; i < items.length; i++) {
                var k = keyFn(items[i]);
                if (keyMap[k] === undefined) {
                    keyMap[k] = groups.length;
                    groups.push({ key: k, items: [] });
                }
                groups[keyMap[k]].items.push(items[i]);
            }
            return groups;
        }

        // ===== Input Channel Strips =====
        function renderInputStrips() {
            var container = document.getElementById('audio-inputs-container');
            if (!container || !audioChannelMap) return;
            audioTabInitDelegation();

            var inputs = audioChannelMap.inputs || [];
            if (inputs.length === 0) {
                container.innerHTML = '<div class="empty-state">No input/output devices detected. Connect a mezzanine board or check the Devices tab.</div>';
                container.dataset.rendered = '';
                return;
            }

            // Re-render guard: use content hash
            var hash = _audioChannelHash(audioChannelMap) + '|in';
            if (container.dataset.rendered === hash) return;

            // Group inputs by deviceName + manufacturer
            var groups = _groupBy(inputs, function(inp) {
                return (inp.deviceName || '') + '|' + (inp.manufacturer || '');
            });

            var html = '';
            for (var g = 0; g < groups.length; g++) {
                var grp = groups[g];
                var firstInp = grp.items[0];
                var grpReady = grp.items.some(function(inp) { return inp.ready; });
                var grpStatusClass = grpReady ? 'status-ok' : 'status-off';
                var grpStatusText = grpReady ? 'Ready' : 'Offline';
                var mfr = firstInp.manufacturer ? escapeHtml(firstInp.manufacturer) : '';

                html += '<div class="device-group">';
                html += '  <div class="device-group-header">';
                html += '    <div class="device-group-header-left">';
                html += '      <span class="device-group-name">' + escapeHtml(firstInp.deviceName || 'Unknown Device') + '</span>';
                if (mfr) html += '      <span class="device-group-manufacturer">' + mfr + '</span>';
                html += '    </div>';
                html += '    <span class="device-group-status channel-status ' + grpStatusClass + '">' + grpStatusText + '</span>';
                html += '  </div>';
                html += '  <div class="channel-strip-grid">';

                for (var i = 0; i < grp.items.length; i++) {
                    html += _buildInputStrip(grp.items[i]);
                }

                html += '  </div>';
                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = hash;
            delete container.dataset.emptyRendered;
        }

        function _buildInputStrip(inp) {
            var lane = inp.lane;
            var statusClass = inp.ready ? 'status-ok' : 'status-off';
            var statusText = inp.ready ? 'OK' : 'Offline';
            var hasPga = (inp.capabilities & 32);   // HAL_CAP_PGA_CONTROL
            var hasHpf = (inp.capabilities & 64);   // HAL_CAP_HPF_CONTROL
            var isStereoLinked = !!_stereoLinkState[lane];

            // Channel label from inputNames (L channel = lane*2)
            var chanLabel = inputNames[lane * 2] || ('In ' + (lane * 2 + 1));

            var html = '';
            html += '<div class="channel-strip" data-lane="' + lane + '">';
            html += '  <div class="channel-strip-header">';
            html += '    <span class="channel-device-name">' + escapeHtml(inp.deviceName || 'Unknown') + '</span>';
            html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
            html += '  </div>';
            html += '  <div class="channel-label-row">';
            html += '    <span class="channel-label" data-action="edit-channel-label" data-lane="' + lane + '" title="Click to rename">' + escapeHtml(chanLabel) + '</span>';
            html += '  </div>';

            // Stereo VU meters (vertical, segmented LED style)
            html += '  <div class="channel-vu-pair">';
            html += '    <div class="channel-vu-wrapper">';
            html += '      <canvas class="channel-vu-canvas" id="inputVu' + lane + 'L" width="24" height="120"></canvas>';
            html += '      <div class="channel-vu-label">L</div>';
            html += '    </div>';
            html += '    <div class="channel-vu-wrapper">';
            html += '      <canvas class="channel-vu-canvas" id="inputVu' + lane + 'R" width="24" height="120"></canvas>';
            html += '      <div class="channel-vu-label">R</div>';
            html += '    </div>';
            html += '    <div class="channel-vu-readout" id="inputVuReadout' + lane + '">-- dB</div>';
            html += '  </div>';

            // Gain slider
            html += '  <div class="channel-control-row">';
            html += '    <label class="channel-control-label">Gain</label>';
            html += '    <input type="range" class="channel-gain-slider" id="inputGain' + lane + '" min="-72" max="12" step="0.5" value="0"';
            html += '      data-action="input-gain" data-lane="' + lane + '">';
            html += '    <span class="channel-gain-value" id="inputGainVal' + lane + '">0.0 dB</span>';
            html += '  </div>';

            // Mute / Phase / Solo buttons
            html += '  <div class="channel-button-row">';
            html += '    <button class="channel-btn" id="inputMute' + lane + '" data-action="toggle-input-mute" data-lane="' + lane + '">Mute</button>';
            html += '    <button class="channel-btn" id="inputPhase' + lane + '" data-action="toggle-input-phase" data-lane="' + lane + '">\u00d8</button>';
            html += '    <button class="channel-btn" id="inputSolo' + lane + '" data-action="toggle-input-solo" data-lane="' + lane + '">Solo</button>';
            html += '  </div>';

            // Stereo link toggle
            html += '  <div class="channel-button-row">';
            html += '    <button class="channel-btn stereo-link-toggle' + (isStereoLinked ? ' active' : '') + '" data-action="toggle-stereo-link" data-lane="' + lane + '">';
            html += '      <svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M10.59,13.41C11,13.8 11,14.44 10.59,14.83C10.2,15.22 9.56,15.22 9.17,14.83C7.22,12.88 7.22,9.71 9.17,7.76V7.76L12.76,4.17C14.71,2.22 17.88,2.22 19.83,4.17C21.78,6.12 21.78,9.29 19.83,11.24L18.07,13C18.07,11.96 17.9,10.92 17.5,9.95L18.41,9C19.59,7.79 19.59,5.83 18.41,4.62C17.22,3.41 15.28,3.41 14.07,4.62L10.48,8.21C9.27,9.42 9.27,11.38 10.48,12.59M13.41,10.59C13.8,10.2 14.44,10.2 14.83,10.59C16.78,12.54 16.78,15.71 14.83,17.66V17.66L11.24,21.25C9.29,23.2 6.12,23.2 4.17,21.25C2.22,19.3 2.22,16.13 4.17,14.18L5.93,12.46C5.93,13.5 6.1,14.54 6.5,15.51L5.59,16.42C4.41,17.63 4.41,19.57 5.59,20.78C6.78,21.99 8.72,21.99 9.93,20.78L13.52,17.19C14.73,15.98 14.73,14.02 13.52,12.81C13.13,12.42 13.13,11.78 13.41,10.59Z"/></svg>';
            html += '      ' + (isStereoLinked ? 'Linked' : 'Link');
            html += '    </button>';
            html += '  </div>';

            // Optional: PGA gain control (bit 5 = 32)
            if (hasPga) {
                html += '  <div class="channel-control-row pga-control">';
                html += '    <label class="channel-control-label">PGA</label>';
                html += '    <input type="range" class="channel-gain-slider" id="inputPga' + lane + '" min="0" max="30" step="1" value="0"';
                html += '      data-action="input-pga" data-lane="' + lane + '">';
                html += '    <span class="channel-gain-value" id="inputPgaVal' + lane + '">0 dB</span>';
                html += '  </div>';
            }

            // Optional: HPF toggle (bit 6 = 64)
            if (hasHpf) {
                html += '  <div class="channel-button-row">';
                html += '    <button class="channel-btn channel-btn-wide hpf-toggle" id="inputHpf' + lane + '" data-action="toggle-input-hpf" data-lane="' + lane + '">HPF</button>';
                html += '  </div>';
            }

            // DSP section: summary line + expandable drawer
            html += '  <div class="channel-dsp-section" id="inputDspSection' + lane + '">';
            html += '    <div class="channel-dsp-summary" data-action="toggle-input-dsp-drawer" data-lane="' + lane + '">';
            html += '      <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
            html += '      <span class="dsp-summary-text" id="inputDspSummary' + lane + '">DSP</span>';
            html += '      <svg class="dsp-drawer-chevron" viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
            html += '    </div>';
            html += '    <div class="channel-dsp-drawer" id="inputDspDrawer' + lane + '" style="display:none">';
            // PEQ region
            html += '      <div class="dsp-drawer-region">';
            html += '        <div class="dsp-drawer-region-label">PEQ</div>';
            html += '        <button class="channel-btn channel-btn-wide" data-action="open-input-peq" data-lane="' + lane + '">';
            html += '          <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
            html += '          Edit PEQ</button>';
            html += '      </div>';
            // Chain region
            html += '      <div class="dsp-drawer-region">';
            html += '        <div class="dsp-drawer-region-label">Chain</div>';
            html += '        <div class="dsp-stage-list" id="inputStageList' + lane + '"></div>';
            html += '        <div class="dsp-add-stage-row">';
            html += '          <div class="dsp-add-stage-wrap" style="position:relative">';
            html += '            <button class="channel-btn" data-action="toggle-input-add-stage" data-lane="' + lane + '" title="Add DSP stage">';
            html += '              <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M19,13H13V19H11V13H5V11H11V5H13V11H19V13Z"/></svg> Add</button>';
            html += '            <div class="dsp-add-stage-menu" id="inputAddStageMenu' + lane + '" style="display:none">';
            html += '              <div class="dsp-add-stage-group-label">Common</div>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="compressor">Compressor</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="limiter">Limiter</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="noise-gate">Noise Gate</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="fir">FIR Filter</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="wav-ir">WAV IR (Convolution)</button>';
            html += '              <div class="dsp-add-stage-group-label">Advanced</div>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="linkwitz">Linkwitz Transform</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="multiband">Multi-band Comp</button>';
            html += '              <button class="dsp-add-stage-item" data-action="add-input-stage" data-lane="' + lane + '" data-type="biquad">Custom Biquad</button>';
            html += '            </div>';
            html += '          </div>';
            html += '        </div>';
            html += '      </div>';
            html += '    </div>';
            html += '  </div>';

            html += '</div>';
            return html;
        }

        // ===== Output Channel Strips =====
        function renderOutputStrips() {
            var container = document.getElementById('audio-outputs-container');
            if (!container || !audioChannelMap) return;
            audioTabInitDelegation();

            var outputs = audioChannelMap.outputs || [];
            if (outputs.length === 0) {
                container.innerHTML = '<div class="empty-state">No input/output devices detected. Connect a mezzanine board or check the Devices tab.</div>';
                container.dataset.rendered = '';
                return;
            }

            // Re-render guard: use content hash
            var hash = _audioChannelHash(audioChannelMap) + '|out';
            if (container.dataset.rendered === hash) return;

            // Group outputs by name (device name)
            var groups = _groupBy(outputs, function(out) {
                return out.name || '';
            });

            var html = '';
            for (var g = 0; g < groups.length; g++) {
                var grp = groups[g];
                var firstOut = grp.items[0];
                var grpReady = grp.items.some(function(out) { return out.ready; });
                var grpStatusClass = grpReady ? 'status-ok' : 'status-off';
                var grpStatusText = grpReady ? 'Ready' : 'Offline';

                // Capability badges
                var hasDsd = (firstOut.capabilities & 2048);   // HAL_CAP_DSD (bit 11)
                var hasDpll = (firstOut.capabilities & 32768); // HAL_CAP_DPLL (bit 15)

                html += '<div class="device-group">';
                html += '  <div class="device-group-header">';
                html += '    <div class="device-group-header-left">';
                html += '      <span class="device-group-name">' + escapeHtml(firstOut.name || 'Unknown Device') + '</span>';
                if (firstOut.manufacturer) html += '      <span class="device-group-manufacturer">' + escapeHtml(firstOut.manufacturer) + '</span>';
                html += '    </div>';
                html += '    <div class="device-group-badges">';
                if (hasDsd) html += '      <span class="capability-badge badge-dsd">DSD</span>';
                if (hasDpll) {
                    html += '      <span class="capability-badge badge-dpll">';
                    html += '        <svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M12,1L3,5V11C3,16.55 6.84,21.74 12,23C17.16,21.74 21,16.55 21,11V5L12,1M12,5A3,3 0 0,1 15,8A3,3 0 0,1 12,11A3,3 0 0,1 9,8A3,3 0 0,1 12,5M17.13,17C15.92,18.85 14.11,20.24 12,20.92C9.89,20.24 8.08,18.85 6.87,17C6.53,16.5 6.24,16 6,15.47C6,13.82 8.71,12.47 12,12.47C15.29,12.47 18,13.79 18,15.47C17.76,16 17.47,16.5 17.13,17Z"/></svg>';
                    html += '        DPLL';
                    html += '      </span>';
                }
                html += '      <span class="device-group-status channel-status ' + grpStatusClass + '">' + grpStatusText + '</span>';
                html += '    </div>';
                html += '  </div>';
                html += '  <div class="channel-strip-grid">';

                for (var i = 0; i < grp.items.length; i++) {
                    html += _buildOutputStrip(grp.items[i]);
                }

                html += '  </div>';
                html += '</div>';
            }

            container.innerHTML = html;
            container.dataset.rendered = hash;
            delete container.dataset.emptyRendered;
        }

        function _buildOutputStrip(out) {
            var idx = out.index;
            var statusClass = out.ready ? 'status-ok' : 'status-off';
            var statusText = out.ready ? 'OK' : 'Offline';
            var hasHwVol = (out.capabilities & 1);    // HAL_CAP_HW_VOLUME (bit 0)
            var hasHwMute = (out.capabilities & 4);   // HAL_CAP_MUTE (bit 2)

            var html = '';
            html += '<div class="channel-strip channel-strip-output" data-sink="' + idx + '">';
            html += '  <div class="channel-strip-header">';
            html += '    <span class="channel-device-name">' + escapeHtml(out.name || 'Output ' + idx) + '</span>';
            html += '    <span class="channel-status ' + statusClass + '">' + statusText + '</span>';
            html += '  </div>';
            html += '  <div class="channel-strip-sub">Ch ' + out.firstChannel + '-' + (out.firstChannel + out.channels - 1) + '</div>';

            // Post-matrix VU meters
            html += '  <div class="channel-vu-pair">';
            for (var ch = 0; ch < out.channels && ch < 2; ch++) {
                var label = out.channels > 1 ? (ch === 0 ? 'L' : 'R') : '';
                html += '    <div class="channel-vu-wrapper">';
                html += '      <canvas class="channel-vu-canvas" id="outputVu' + idx + 'c' + ch + '" width="24" height="120"></canvas>';
                if (label) html += '      <div class="channel-vu-label">' + label + '</div>';
                html += '    </div>';
            }
            html += '    <div class="channel-vu-readout" id="outputVuReadout' + idx + '">-- dB</div>';
            html += '  </div>';

            // HW Volume slider (if supported)
            if (hasHwVol) {
                html += '  <div class="channel-control-row">';
                html += '    <label class="channel-control-label">HW Vol</label>';
                html += '    <input type="range" class="channel-gain-slider" id="outputHwVol' + idx + '" min="0" max="100" step="1" value="80"';
                html += '      data-action="output-hw-vol" data-sink="' + idx + '">';
                html += '    <span class="channel-gain-value" id="outputHwVolVal' + idx + '">80%</span>';
                html += '  </div>';
            }

            // Gain slider
            html += '  <div class="channel-control-row">';
            html += '    <label class="channel-control-label">Gain</label>';
            html += '    <input type="range" class="channel-gain-slider" id="outputGain' + idx + '" min="-72" max="12" step="0.5" value="0"';
            html += '      data-action="output-gain" data-sink="' + idx + '">';
            html += '    <span class="channel-gain-value" id="outputGainVal' + idx + '">0.0 dB</span>';
            html += '  </div>';

            // Mute / Phase buttons
            html += '  <div class="channel-button-row">';
            html += '    <button class="channel-btn' + (hasHwMute ? ' hw-mute-label' : '') + '" id="outputMute' + idx + '" data-action="toggle-output-mute" data-sink="' + idx + '">' + (hasHwMute ? 'HW Mute' : 'Mute') + '</button>';
            html += '    <button class="channel-btn" id="outputPhase' + idx + '" data-action="toggle-output-phase" data-sink="' + idx + '">\u00d8</button>';
            html += '  </div>';

            // DSP section: summary line + expandable drawer
            html += '  <div class="channel-dsp-section" id="outputDspSection' + idx + '">';
            html += '    <div class="channel-dsp-summary" data-action="toggle-output-dsp-drawer" data-sink="' + idx + '">';
            html += '      <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
            html += '      <span class="dsp-summary-text" id="outputDspSummary' + idx + '">DSP</span>';
            html += '      <svg class="dsp-drawer-chevron" viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
            html += '    </div>';
            html += '    <div class="channel-dsp-drawer" id="outputDspDrawer' + idx + '" style="display:none">';
            html += '      <div class="dsp-drawer-region">';
            html += '        <button class="channel-btn channel-btn-wide" data-action="open-output-peq" data-channel="' + out.firstChannel + '">';
            html += '          <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M22,7L20,7V3H18V7L16,7V9H18V21H20V9H22V7M14,13L12,13V3H10V13L8,13V15H10V21H12V15H14V13M6,17L4,17V3H2V17L0,17V19H2V21H4V19H6V17Z"/></svg>';
            html += '          PEQ</button>';
            html += '        <button class="channel-btn channel-btn-wide" data-action="open-output-crossover" data-channel="' + out.firstChannel + '">';
            html += '          <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M16,11.78L20.24,4.45L21.97,5.45L16.74,14.5L10.23,10.75L5.46,19H22V21H2V3H4V17.54L9.5,8L16,11.78Z"/></svg>';
            html += '          Crossover</button>';
            html += '        <button class="channel-btn channel-btn-wide" data-action="open-output-compressor" data-channel="' + out.firstChannel + '">';
            html += '          <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M5,4V7H10.5V19H13.5V7H19V4H5Z"/></svg>';
            html += '          Compressor</button>';
            html += '        <button class="channel-btn channel-btn-wide" data-action="open-output-limiter" data-channel="' + out.firstChannel + '">';
            html += '          <svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M2,12A10,10 0 0,1 12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22A10,10 0 0,1 2,12M4,12A8,8 0 0,0 12,20A8,8 0 0,0 20,12A8,8 0 0,0 12,4A8,8 0 0,0 4,12M7,12L12,7V17L7,12Z"/></svg>';
            html += '          Limiter</button>';
            html += '      </div>';
            html += '    </div>';
            html += '  </div>';

            // Delay input (always visible below DSP section)
            html += '  <div class="channel-control-row" style="margin-top:6px;">';
            html += '    <label class="channel-control-label">Delay</label>';
            html += '    <input type="number" class="channel-delay-input" id="outputDelay' + out.firstChannel + '" min="0" max="10" step="0.01" value="0.00"';
            html += '      data-action="output-delay" data-channel="' + out.firstChannel + '">';
            html += '    <span class="channel-gain-value">ms</span>';
            html += '  </div>';

            html += '</div>';
            return html;
        }

        // ===== Matrix Grid =====
        // Builds per-column metadata: { label, deviceName, deviceSpanStart, deviceSpanLen }
        function _buildMatrixColMeta(matSize, outputs) {
            var cols = [];
            for (var o = 0; o < matSize; o++) {
                var label = 'OUT ' + (o + 1);
                var deviceName = '';
                for (var si = 0; si < outputs.length; si++) {
                    var sk = outputs[si];
                    if (o >= sk.firstChannel && o < sk.firstChannel + sk.channels) {
                        var chOff = o - sk.firstChannel;
                        label = (sk.channels > 1 ? (chOff === 0 ? 'L' : 'R') : 'Ch');
                        deviceName = sk.name || '';
                        break;
                    }
                }
                cols.push({ label: label, deviceName: deviceName });
            }
            // Annotate device span boundaries
            var lastDev = null;
            for (var i = 0; i < cols.length; i++) {
                if (cols[i].deviceName !== lastDev) {
                    cols[i].spanStart = true;
                    // Count span length
                    var spanLen = 1;
                    while (i + spanLen < cols.length && cols[i + spanLen].deviceName === cols[i].deviceName) spanLen++;
                    cols[i].spanLen = spanLen;
                    lastDev = cols[i].deviceName;
                } else {
                    cols[i].spanStart = false;
                    cols[i].spanLen = 0;
                }
            }
            return cols;
        }

        // Builds per-row metadata: { label, deviceName, matrixCh }
        function _buildMatrixRowMeta(matSize, inputs) {
            // Build a mapping from matrixCh -> input lane info
            var chToInput = {};
            for (var li = 0; li < inputs.length; li++) {
                var inp = inputs[li];
                var baseCh = inp.matrixCh !== undefined ? inp.matrixCh : li * 2;
                var numCh = inp.channels || 2;
                for (var c = 0; c < numCh; c++) {
                    chToInput[baseCh + c] = { inp: inp, chOffset: c };
                }
            }
            var rows = [];
            for (var r = 0; r < matSize; r++) {
                var info = chToInput[r];
                if (info) {
                    var chLabel = info.inp.channels > 1 ? (info.chOffset === 0 ? 'L' : 'R') : '';
                    rows.push({ label: chLabel, deviceName: info.inp.deviceName || ('IN ' + (r + 1)), matrixCh: r });
                } else {
                    rows.push({ label: '', deviceName: 'IN ' + (r + 1), matrixCh: r });
                }
            }
            // Annotate device span boundaries for row group headers
            var lastRowDev = null;
            for (var j = 0; j < rows.length; j++) {
                if (rows[j].deviceName !== lastRowDev) {
                    rows[j].groupStart = true;
                    var rowSpanLen = 1;
                    while (j + rowSpanLen < rows.length && rows[j + rowSpanLen].deviceName === rows[j].deviceName) rowSpanLen++;
                    rows[j].groupSpan = rowSpanLen;
                    lastRowDev = rows[j].deviceName;
                } else {
                    rows[j].groupStart = false;
                    rows[j].groupSpan = 0;
                }
            }
            return rows;
        }

        function renderMatrixGrid() {
            var container = document.getElementById('audio-matrix-container');
            if (!container || !audioChannelMap) return;

            // Re-render guard
            var hash = _audioChannelHash(audioChannelMap) + '|matrix';
            if (container.dataset.rendered === hash) return;

            var matSize = audioChannelMap.matrixInputs || 8;
            var inputs = audioChannelMap.inputs || [];
            var outputs = audioChannelMap.outputs || [];
            var matrixGains = audioChannelMap.matrix || [];

            var cols = _buildMatrixColMeta(matSize, outputs);
            var rows = _buildMatrixRowMeta(matSize, inputs);

            var html = '<div class="matrix-scroll-wrap"><table class="matrix-table">';

            // thead: two header rows — device group row + channel label row
            html += '<thead>';

            // Row 1: corner + OUTPUTS axis label + device group spans
            html += '<tr>';
            html += '<th class="matrix-corner" rowspan="2">INPUTS \\ OUTPUTS</th>';
            for (var c = 0; c < matSize; c++) {
                if (cols[c].spanStart) {
                    html += '<th class="matrix-dev-hdr" colspan="' + cols[c].spanLen + '">' + escapeHtml(cols[c].deviceName || 'OUT') + '</th>';
                }
            }
            html += '</tr>';

            // Row 2: per-channel labels (L/R/Ch)
            html += '<tr>';
            for (var c2 = 0; c2 < matSize; c2++) {
                html += '<th class="matrix-col-hdr" title="' + escapeHtml(cols[c2].deviceName + ' ' + cols[c2].label) + '">' + escapeHtml(cols[c2].label) + '</th>';
            }
            html += '</tr>';

            html += '</thead>';

            // tbody: one row per matrix input channel
            html += '<tbody>';
            for (var row = 0; row < matSize; row++) {
                var rowMeta = rows[row];
                html += '<tr>';

                // Device group cell (merged rows)
                if (rowMeta.groupStart) {
                    html += '<td class="matrix-row-dev-hdr" rowspan="' + rowMeta.groupSpan + '">' + escapeHtml(rowMeta.deviceName) + '</td>';
                }
                // Channel label cell (always)
                // We use a separate <td> for channel L/R label alongside group cell
                // but only if this row's group started — otherwise we only have channel cell
                // Actually: group cell takes one column, channel label takes another
                // Structure: [group-dev] [chan-label] [cells...]
                // But rowspan means we can't nest. Use class approach:
                // When groupStart: td.matrix-row-dev-hdr rowspan=N, then td.matrix-row-ch
                // When not groupStart: td.matrix-row-ch only
                html += '<td class="matrix-row-hdr">' + escapeHtml(rowMeta.label) + '</td>';

                for (var col = 0; col < matSize; col++) {
                    var gain = (matrixGains[col] && matrixGains[col][row] !== undefined) ? parseFloat(matrixGains[col][row]) : 0;
                    var active = gain > 0.0001 || gain < -0.0001;
                    var displayVal = active ? (gain >= 1.0 ? '+' + (20 * Math.log10(gain)).toFixed(1) : (20 * Math.log10(Math.max(gain, 0.0001))).toFixed(1)) : '--';
                    var cellClass = 'matrix-cell' + (active ? ' matrix-active' : '');
                    html += '<td class="' + cellClass + '" data-out="' + col + '" data-in="' + row + '" data-action="matrix-cell" tabindex="0" role="button" aria-label="Route IN ' + (row + 1) + ' to OUT ' + (col + 1) + ': ' + (active ? displayVal + ' dB' : 'off') + '">' + displayVal + '</td>';
                }
                html += '</tr>';
            }
            html += '</tbody></table></div>';

            // Quick presets
            html += '<div class="matrix-presets">';
            html += '  <button class="btn btn-secondary btn-sm" data-action="matrix-preset-1to1">1:1 Pass</button>';
            html += '  <button class="btn btn-secondary btn-sm" data-action="matrix-preset-stereo-all">Stereo\u2192All</button>';
            html += '  <button class="btn btn-secondary btn-sm" data-action="matrix-preset-clear">Clear All</button>';
            html += '  <button class="btn btn-secondary btn-sm" data-action="matrix-save">Save</button>';
            html += '  <button class="btn btn-secondary btn-sm" data-action="matrix-load">Load</button>';
            html += '</div>';

            container.innerHTML = html;
            container.dataset.rendered = hash;
            delete container.dataset.emptyRendered;
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

            popup.innerHTML = '<div class="matrix-gain-popup-inner" data-popup-out="' + outCh + '" data-popup-in="' + inCh + '">' +
                '<label>OUT ' + (outCh + 1) + ' \u2190 IN ' + (inCh + 1) + '</label>' +
                '<input type="range" id="matrixGainSlider" min="-72" max="12" step="0.5" value="' + currentDb + '" data-action="matrix-gain-slide" data-out="' + outCh + '" data-in="' + inCh + '">' +
                '<span id="matrixGainDbVal">' + currentDb + ' dB</span>' +
                '<div style="display:flex;gap:4px;margin-top:6px;">' +
                '<button class="btn btn-sm btn-primary" data-action="matrix-gain-set0" data-out="' + outCh + '" data-in="' + inCh + '">0 dB</button>' +
                '<button class="btn btn-sm btn-secondary" data-action="matrix-gain-setoff" data-out="' + outCh + '" data-in="' + inCh + '">Off</button>' +
                '<button class="btn btn-sm btn-secondary" data-action="matrix-popup-close">Close</button>' +
                '</div></div>';
            popup.style.display = 'block';

            // Ensure popup has event delegation (popup is outside #audio, so handle on it directly)
            if (!popup.dataset.delegationInit) {
                popup.dataset.delegationInit = '1';
                popup.addEventListener('click', function(e) {
                    var el = e.target.closest('[data-action]');
                    if (!el) return;
                    var action = el.dataset.action;
                    if (action === 'matrix-gain-set0') {
                        setMatrixGainDb(parseInt(el.dataset.out), parseInt(el.dataset.in), 0);
                        closeMatrixPopup();
                    } else if (action === 'matrix-gain-setoff') {
                        setMatrixGainDb(parseInt(el.dataset.out), parseInt(el.dataset.in), -72);
                        closeMatrixPopup();
                    } else if (action === 'matrix-popup-close') {
                        closeMatrixPopup();
                    }
                });
                popup.addEventListener('input', function(e) {
                    var el = e.target.closest('[data-action="matrix-gain-slide"]');
                    if (el) onMatrixGainSlide(parseInt(el.dataset.out), parseInt(el.dataset.in), el.value);
                });
            }
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
            apiFetch('/api/pipeline/matrix/cell', {
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
            .catch(function(err) { console.warn('[Audio] Matrix cell update failed:', err); });
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
        // Stereo->All: routes stereo pair (IN 0/1) to every output pair at 0 dB.
        // Each output pair gets IN0->L and IN1->R.
        function matrixPresetStereoAll() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var o = 0; o < size; o++) {
                // Clear entire column first
                for (var i = 0; i < size; i++) setMatrixGainDb(o, i, -96);
                // Route IN0 to even outputs, IN1 to odd outputs
                setMatrixGainDb(o, o % 2, 0);
            }
        }
        function matrixPresetClear() {
            var size = (audioChannelMap && audioChannelMap.matrixInputs) || 8;
            for (var o = 0; o < size; o++)
                for (var i = 0; i < size; i++)
                    setMatrixGainDb(o, i, -96);
        }
        function matrixSave() {
            apiFetch('/api/pipeline/matrix/save', { method: 'POST' })
                .then(function() { showToast('Matrix saved', 'success'); })
                .catch(function() { showToast('Save failed', 'error'); });
        }
        function matrixLoad() {
            apiFetch('/api/pipeline/matrix/load', { method: 'POST' })
                .then(function() {
                    showToast('Matrix loaded', 'success');
                    // Refresh channel map to get updated matrix
                    apiFetch('/api/pipeline/matrix').then(function(r) { return r.json(); }).then(function(d) {
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
            // Mirror to stereo-linked pair if active
            if (_stereoLinkState[lane]) {
                var pairLane = (lane % 2 === 0) ? lane + 1 : lane - 1;
                var pairSlider = document.getElementById('inputGain' + pairLane);
                var pairLabel = document.getElementById('inputGainVal' + pairLane);
                if (pairSlider) { pairSlider.value = val; }
                if (pairLabel) { pairLabel.textContent = parseFloat(val).toFixed(1) + ' dB'; }
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'setInputGain', lane: pairLane, db: parseFloat(val) }));
                }
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
            // Mirror to stereo-linked pair
            if (_stereoLinkState[lane]) {
                var pairLane = (lane % 2 === 0) ? lane + 1 : lane - 1;
                var pairBtn = document.getElementById('inputMute' + pairLane);
                if (pairBtn) pairBtn.classList.toggle('active', muted);
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'setInputMute', lane: pairLane, muted: muted }));
                }
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
            // Mirror to stereo-linked pair
            if (_stereoLinkState[lane]) {
                var pairLane = (lane % 2 === 0) ? lane + 1 : lane - 1;
                var pairBtn = document.getElementById('inputPhase' + pairLane);
                if (pairBtn) pairBtn.classList.toggle('active', inverted);
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'setInputPhase', lane: pairLane, inverted: inverted }));
                }
            }
        }

        function onInputPgaChange(lane, val) {
            var label = document.getElementById('inputPgaVal' + lane);
            if (label) label.textContent = parseFloat(val).toFixed(0) + ' dB';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputPga', lane: lane, gain: parseFloat(val) }));
            }
        }

        function toggleInputHpf(lane) {
            var btn = document.getElementById('inputHpf' + lane);
            if (!btn) return;
            var enabled = !btn.classList.contains('active');
            btn.classList.toggle('active', enabled);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputHpf', lane: lane, enabled: enabled }));
            }
        }

        function toggleInputSolo(lane) {
            // UI-only solo: mute all other lanes visually
            var btn = document.getElementById('inputSolo' + lane);
            if (!btn) return;
            var soloed = !btn.classList.contains('active');
            btn.classList.toggle('active', soloed);
            // Visual-only: no WS command for solo
        }

        function toggleStereoLink(lane) {
            _stereoLinkState[lane] = !_stereoLinkState[lane];
            // Re-render the strip to update the button state
            // (cheaper to just re-render since we already have the hash guard)
            var hash = _audioChannelHash(audioChannelMap) + '|in';
            var container = document.getElementById('audio-inputs-container');
            if (container) {
                delete container.dataset.rendered;
                renderInputStrips();
            }
        }

        // ===== Inline Channel Label Edit =====
        function startChannelLabelEdit(lane) {
            var el = document.querySelector('.channel-label[data-lane="' + lane + '"]');
            if (!el || el.isContentEditable) return;
            el.contentEditable = 'true';
            el.focus();
            // Select all text
            var range = document.createRange();
            range.selectNodeContents(el);
            var sel = window.getSelection();
            if (sel) { sel.removeAllRanges(); sel.addRange(range); }

            function commit() {
                el.contentEditable = 'false';
                var newLabel = el.textContent.trim() || ('In ' + (lane * 2 + 1));
                el.textContent = newLabel;
                // Update inputNames array
                inputNames[lane * 2] = newLabel;
                // Send full setInputNames array via WS
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'setInputNames', names: inputNames.slice() }));
                }
                el.removeEventListener('blur', commit);
                el.removeEventListener('keydown', onKey);
            }
            function onKey(e) {
                if (e.key === 'Enter') { e.preventDefault(); commit(); }
                if (e.key === 'Escape') {
                    el.contentEditable = 'false';
                    // Restore original label
                    el.textContent = inputNames[lane * 2] || ('In ' + (lane * 2 + 1));
                    el.removeEventListener('blur', commit);
                    el.removeEventListener('keydown', onKey);
                }
            }
            el.addEventListener('blur', commit);
            el.addEventListener('keydown', onKey);
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
            if (out && out.halSlot !== undefined && out.halSlot !== 255) {
                apiFetch('/api/hal/devices', {
                    method: 'PUT',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ slot: out.halSlot, volume: parseInt(val) })
                }).catch(function() { showToast('HW volume update failed', 'error'); });
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

        // ===== DSP Drawer Toggle =====
        function toggleInputDspDrawer(lane) {
            var drawer = document.getElementById('inputDspDrawer' + lane);
            if (!drawer) return;
            var open = drawer.style.display === 'none' || drawer.style.display === '';
            drawer.style.display = open ? 'block' : 'none';
            var section = document.getElementById('inputDspSection' + lane);
            if (section) section.classList.toggle('dsp-drawer-open', open);
        }

        function toggleOutputDspDrawer(sinkIdx) {
            var drawer = document.getElementById('outputDspDrawer' + sinkIdx);
            if (!drawer) return;
            var open = drawer.style.display === 'none' || drawer.style.display === '';
            drawer.style.display = open ? 'block' : 'none';
            var section = document.getElementById('outputDspSection' + sinkIdx);
            if (section) section.classList.toggle('dsp-drawer-open', open);
        }

        function toggleInputAddStageMenu(lane) {
            var menu = document.getElementById('inputAddStageMenu' + lane);
            if (!menu) return;
            var open = menu.style.display === 'none' || menu.style.display === '';
            menu.style.display = open ? 'block' : 'none';

            if (open) {
                // Close menu when clicking outside
                function onOutsideClick(e) {
                    if (!menu.contains(e.target)) {
                        menu.style.display = 'none';
                        document.removeEventListener('click', onOutsideClick);
                    }
                }
                setTimeout(function() {
                    document.addEventListener('click', onOutsideClick);
                }, 10);
            }
        }

        // Called when user picks a stage type from the add-stage dropdown
        function onAddInputStage(lane, type) {
            var menu = document.getElementById('inputAddStageMenu' + lane);
            if (menu) menu.style.display = 'none';

            if (type === 'compressor') {
                openInputCompressor(lane);
            } else if (type === 'limiter') {
                openInputLimiter(lane);
            } else if (type === 'noise-gate') {
                openInputNoiseGate(lane);
            } else if (type === 'fir') {
                openInputFirUpload(lane);
            } else if (type === 'wav-ir') {
                openInputWavIr(lane);
            } else if (type === 'linkwitz') {
                openInputLinkwitz(lane);
            } else if (type === 'multiband') {
                openInputMultibandComp(lane);
            } else if (type === 'biquad') {
                openInputCustomBiquad(lane);
            }
        }

        // PEQ / DSP overlay openers defined in 06-peq-overlay.js:
        // openInputPeq, openOutputPeq, openOutputCrossover, openOutputCompressor, openOutputLimiter
        // openInputCompressor, openInputLimiter, openInputNoiseGate, openInputFirUpload,
        // openInputWavIr, openInputLinkwitz, openInputMultibandComp, openInputCustomBiquad

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

            // Segmented LED VU bar (bottom-up)
            var pct = Math.max(0, Math.min(1, (value + 60) / 60));  // -60dB to 0dB range
            var numSegs = 20;
            var segH = Math.floor((h - numSegs) / numSegs);
            var activeSeg = Math.round(pct * numSegs);
            for (var s = 0; s < numSegs; s++) {
                var y = h - (s + 1) * (segH + 1);
                var ratio = s / numSegs;
                if (s < activeSeg) {
                    if (ratio > 0.85) {
                        ctx.fillStyle = '#F44336';
                    } else if (ratio > 0.65) {
                        ctx.fillStyle = '#FFC107';
                    } else {
                        ctx.fillStyle = '#4CAF50';
                    }
                } else {
                    ctx.fillStyle = 'rgba(255,255,255,0.06)';
                }
                ctx.fillRect(2, y, w - 4, segH);
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
            // SNR/SFDR readout from audioLevels broadcast
            if (data.adcSnrDb || data.adcSfdrDb) {
                updateSnrSfdr(data.adcSnrDb, data.adcSfdrDb);
            }

            // Output sink VU meters
            if (data.sinks && audioSubView === 'outputs') {
                for (var s = 0; s < data.sinks.length; s++) {
                    var sk = data.sinks[s];
                    drawChannelVu('outputVu' + s + 'c0', sk.vuL || -90);
                    drawChannelVu('outputVu' + s + 'c1', sk.vuR || -90);
                    var outReadout = document.getElementById('outputVuReadout' + s);
                    if (outReadout) {
                        var avg = ((sk.vuL || -90) + (sk.vuR || -90)) / 2;
                        outReadout.textContent = avg.toFixed(1) + ' dB';
                    }
                }
            }
        }

        // ===== Event Delegation for Audio Tab =====
        // Handles all data-action events from dynamically generated channel strips and matrix
        function audioTabInitDelegation() {
            var audioTab = document.getElementById('audio');
            if (!audioTab || audioTab.dataset.delegationInit) return;
            audioTab.dataset.delegationInit = '1';

            // Delegated click handler
            audioTab.addEventListener('click', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;

                if (action === 'toggle-input-mute') {
                    toggleInputMute(parseInt(el.dataset.lane));
                } else if (action === 'toggle-input-phase') {
                    toggleInputPhase(parseInt(el.dataset.lane));
                } else if (action === 'toggle-input-solo') {
                    toggleInputSolo(parseInt(el.dataset.lane));
                } else if (action === 'toggle-input-hpf') {
                    toggleInputHpf(parseInt(el.dataset.lane));
                } else if (action === 'toggle-stereo-link') {
                    toggleStereoLink(parseInt(el.dataset.lane));
                } else if (action === 'edit-channel-label') {
                    startChannelLabelEdit(parseInt(el.dataset.lane));
                } else if (action === 'open-input-peq') {
                    openInputPeq(parseInt(el.dataset.lane));
                } else if (action === 'toggle-input-dsp-drawer') {
                    toggleInputDspDrawer(parseInt(el.dataset.lane));
                } else if (action === 'toggle-output-dsp-drawer') {
                    toggleOutputDspDrawer(parseInt(el.dataset.sink));
                } else if (action === 'toggle-input-add-stage') {
                    toggleInputAddStageMenu(parseInt(el.dataset.lane));
                } else if (action === 'add-input-stage') {
                    onAddInputStage(parseInt(el.dataset.lane), el.dataset.type);
                } else if (action === 'toggle-output-mute') {
                    toggleOutputMute(parseInt(el.dataset.sink));
                } else if (action === 'toggle-output-phase') {
                    toggleOutputPhase(parseInt(el.dataset.sink));
                } else if (action === 'open-output-peq') {
                    openOutputPeq(parseInt(el.dataset.channel));
                } else if (action === 'open-output-crossover') {
                    openOutputCrossover(parseInt(el.dataset.channel));
                } else if (action === 'open-output-compressor') {
                    openOutputCompressor(parseInt(el.dataset.channel));
                } else if (action === 'open-output-limiter') {
                    openOutputLimiter(parseInt(el.dataset.channel));
                } else if (action === 'matrix-cell') {
                    onMatrixCellClick(parseInt(el.dataset.out), parseInt(el.dataset.in));
                } else if (action === 'matrix-preset-1to1') {
                    matrixPreset1to1();
                } else if (action === 'matrix-preset-stereo-all') {
                    matrixPresetStereoAll();
                } else if (action === 'matrix-preset-clear') {
                    matrixPresetClear();
                } else if (action === 'matrix-save') {
                    matrixSave();
                } else if (action === 'matrix-load') {
                    matrixLoad();
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

            // Delegated input/change handler for sliders and delay inputs
            audioTab.addEventListener('input', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;
                if (action === 'input-gain') {
                    onInputGainChange(parseInt(el.dataset.lane), el.value);
                } else if (action === 'input-pga') {
                    onInputPgaChange(parseInt(el.dataset.lane), el.value);
                } else if (action === 'output-gain') {
                    onOutputGainChange(parseInt(el.dataset.sink), el.value);
                } else if (action === 'output-hw-vol') {
                    onOutputHwVolChange(parseInt(el.dataset.sink), el.value);
                } else if (action === 'matrix-gain-slide') {
                    onMatrixGainSlide(parseInt(el.dataset.out), parseInt(el.dataset.in), el.value);
                }
            });

            audioTab.addEventListener('change', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;
                if (action === 'output-delay') {
                    onOutputDelayChange(parseInt(el.dataset.channel), el.value);
                }
            });
        }

        // Initialize delegation when audio tab is first activated
        document.addEventListener('DOMContentLoaded', function() {
            audioTabInitDelegation();
        });
