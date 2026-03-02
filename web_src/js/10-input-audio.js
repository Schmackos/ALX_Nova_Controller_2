        // ===== Audio Tab Functions =====
        function updateAdc2Visibility() {
            var show = numAdcsDetected > 1;
            var ids = ['adcSection1', 'waveformPanel1', 'spectrumPanel1', 'inputNamesAdc1', 'siggenTargetAdcGroup'];
            for (var i = 0; i < ids.length; i++) {
                var el = document.getElementById(ids[i]);
                if (el) el.style.display = show ? '' : 'none';
            }
            var grids = document.querySelectorAll('#audio .dual-canvas-grid');
            for (var i = 0; i < grids.length; i++) {
                grids[i].style.gridTemplateColumns = show ? '1fr 1fr' : '1fr';
            }
        }

        function updateAdcStatusBadge(adcIdx, status) {
            var el = document.getElementById('adcStatusBadge' + adcIdx);
            if (!el) return;
            el.textContent = status || 'OK';
            el.className = 'adc-status-badge';
            var s = (status || 'OK').toUpperCase();
            if (s === 'OK') el.classList.add('ok');
            else if (s === 'NO_DATA') el.classList.add('no-data');
            else if (s === 'CLIPPING') el.classList.add('clipping');
            else if (s === 'NOISE_ONLY') el.classList.add('noise-only');
            else if (s === 'I2S_ERROR') el.classList.add('i2s-error');
            else if (s === 'HW_FAULT') el.classList.add('hw-fault');
            var clip = document.getElementById('clipIndicator' + adcIdx);
            if (clip) {
                if (s === 'CLIPPING' || s === 'HW_FAULT') clip.classList.add('active');
                else clip.classList.remove('active');
            }
        }

        function updateAdcReadout(adcIdx, dBFS, vrms1, vrms2) {
            var el = document.getElementById('adcReadout' + adcIdx);
            if (!el) return;
            var dbStr = (dBFS !== undefined && dBFS > -95) ? dBFS.toFixed(1) + ' dBFS' : '-inf dBFS';
            var vrms = 0;
            if (vrms1 !== undefined && vrms2 !== undefined) vrms = Math.max(vrms1, vrms2);
            else if (vrms1 !== undefined) vrms = vrms1;
            var vStr = vrms > 0.001 ? vrms.toFixed(3) + ' Vrms' : '-- Vrms';
            el.textContent = dbStr + ' | ' + vStr;
        }

        function applyInputNames() {
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('vuChName' + i);
                if (el) el.textContent = inputNames[i] || ('Ch ' + (i + 1));
                var segEl = document.getElementById('vuChNameSeg' + i);
                if (segEl) segEl.textContent = inputNames[i] || ('Ch ' + (i + 1));
            }
            if (typeof dspRenderChannelTabs === 'function') dspRenderChannelTabs();
            if (typeof dspRenderRouting === 'function') dspRenderRouting();
            if (typeof updatePeqCopyToDropdown === 'function') updatePeqCopyToDropdown();
            if (typeof updateChainCopyToDropdown === 'function') updateChainCopyToDropdown();
        }

        function updatePeqCopyToDropdown() {
            var sel = document.getElementById('peqCopyTo');
            if (!sel) return;
            // Preserve first option and rebuild channel options
            var html = '<option value="">Copy to...</option>';
            for (var i = 0; i < DSP_MAX_CH; i++) {
                var name = inputNames[i] || DSP_CH_NAMES[i];
                html += '<option value="' + i + '">' + name + '</option>';
            }
            html += '<option value="all">All Channels</option>';
            sel.innerHTML = html;
        }

        function loadInputNameFields() {
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('inputName' + i);
                if (el) el.value = inputNames[i] || '';
            }
        }

        function saveInputNames() {
            var names = [];
            for (var i = 0; i < NUM_ADCS * 2; i++) {
                var el = document.getElementById('inputName' + i);
                names.push(el ? el.value : '');
            }
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'setInputNames', names: names }));
                showToast('Input names saved', 'success');
            }
        }

        function updateAudioSettings() {
            const sampleRate = parseInt(document.getElementById('audioSampleRateSelect').value);

            apiFetch('/api/smartsensing', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ audioSampleRate: sampleRate })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) showToast('Sample rate updated', 'success');
                else showToast('Failed to update sample rate', 'error');
            })
            .catch(err => showToast('Failed to update sample rate', 'error'));
        }

        // ===== Per-ADC Input Enable/Disable =====
        function setAdcEnabled(adc, en) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'setAdcEnabled',adc:adc,enabled:en}));
        }

        // ===== USB Audio Input =====
        function setUsbAudioEnabled(en) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:'setUsbAudioEnabled',enabled:en}));
        }

        function handleUsbAudioState(d) {
            var enableCb = document.getElementById('usbAudioEnable');
            var fields = document.getElementById('usbAudioFields');
            if (enableCb) enableCb.checked = !!d.enabled;
            if (fields) fields.style.display = d.enabled ? '' : 'none';

            var badge = document.getElementById('usbAudioBadge');
            var statusEl = document.getElementById('usbAudioStatus');
            var formatEl = document.getElementById('usbAudioFormat');
            var volEl = document.getElementById('usbAudioVolume');
            var details = document.getElementById('usbAudioDetails');
            if (!d.enabled) {
                if (badge) { badge.textContent = 'Disabled'; badge.style.background = '#9E9E9E'; }
            } else if (d.streaming) {
                if (badge) { badge.textContent = 'Streaming'; badge.style.background = '#4CAF50'; }
                if (statusEl) statusEl.textContent = 'Streaming';
                if (details) details.style.display = '';
            } else if (d.connected) {
                if (badge) { badge.textContent = 'Connected'; badge.style.background = '#FF9800'; }
                if (statusEl) statusEl.textContent = 'Connected (idle)';
                if (details) details.style.display = '';
            } else {
                if (badge) { badge.textContent = 'Disconnected'; badge.style.background = '#9E9E9E'; }
                if (statusEl) statusEl.textContent = 'Disconnected';
                if (details) details.style.display = 'none';
            }
            if (formatEl) {
                if (d.negotiatedRate) {
                    formatEl.textContent = (d.negotiatedRate / 1000) + ' kHz / ' + (d.negotiatedDepth || d.bitDepth) + '-bit stereo';
                } else if (d.connected) {
                    formatEl.textContent = (d.sampleRate/1000) + ' kHz / ' + d.bitDepth + '-bit ' + (d.channels === 1 ? 'mono' : 'stereo');
                } else {
                    formatEl.textContent = '\u2014';
                }
            }
            if (volEl) {
                if (d.connected) {
                    if (d.mute) {
                        volEl.textContent = 'Muted';
                    } else {
                        var dbVal = (d.volume / 256).toFixed(1);
                        var pct = Math.round(d.volumeLinear * 100);
                        volEl.textContent = dbVal + ' dB (' + pct + '%)';
                    }
                } else {
                    volEl.textContent = '\u2014';
                }
            }
            var ovr = document.getElementById('usbAudioOverruns');
            if (ovr) ovr.textContent = d.overruns || 0;
            var udr = document.getElementById('usbAudioUnderruns');
            if (udr) udr.textContent = d.underruns || 0;
            // VU meters (visible only when streaming)
            var vuSection = document.getElementById('usbAudioVu');
            if (vuSection) {
                vuSection.style.display = d.streaming ? '' : 'none';
                if (d.streaming && d.vuL !== undefined) {
                    var pctL = Math.max(0, Math.min(100, 100 + (d.vuL || -90)));
                    var pctR = Math.max(0, Math.min(100, 100 + (d.vuR || -90)));
                    var barL = document.getElementById('usbVuBarL');
                    var barR = document.getElementById('usbVuBarR');
                    if (barL) barL.style.width = pctL + '%';
                    if (barR) barR.style.width = pctR + '%';
                    var readL = document.getElementById('usbVuReadL');
                    var readR = document.getElementById('usbVuReadR');
                    if (readL) readL.textContent = (d.vuL > -90) ? d.vuL.toFixed(1) + ' dBFS' : '-inf dBFS';
                    if (readR) readR.textContent = (d.vuR > -90) ? d.vuR.toFixed(1) + ' dBFS' : '-inf dBFS';
                }
            }
            if (typeof overviewApplyUsbState === 'function') overviewApplyUsbState(d);
        }
