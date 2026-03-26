        function updateSigGen() {
            document.getElementById('siggenFields').style.display = document.getElementById('siggenEnable').checked ? '' : 'none';
            var wf = parseInt(document.getElementById('siggenWaveform').value);
            document.getElementById('siggenSweepGroup').style.display = wf === 3 ? '' : 'none';
            var mode = parseInt(document.getElementById('siggenOutputMode').value);
            document.getElementById('siggenPwmNote').style.display = mode === 1 ? '' : 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'setSignalGen',
                    enabled: document.getElementById('siggenEnable').checked,
                    waveform: wf,
                    frequency: parseFloat(document.getElementById('siggenFreq').value),
                    amplitude: parseFloat(document.getElementById('siggenAmp').value),
                    channel: parseInt(document.getElementById('siggenChannel').value),
                    outputMode: mode,
                    sweepSpeed: parseFloat(document.getElementById('siggenSweepSpeed').value),
                    targetAdc: parseInt(document.getElementById('siggenTargetAdc').value)
                }));
            }
        }
        function siggenPreset(wf, freq, amp) {
            document.getElementById('siggenWaveform').value = wf;
            document.getElementById('siggenFreq').value = freq;
            document.getElementById('siggenFreqVal').textContent = freq;
            document.getElementById('siggenAmp').value = amp;
            document.getElementById('siggenAmpVal').textContent = amp;
            if (wf === 3) document.getElementById('siggenSweepSpeed').value = 1000;
            document.getElementById('siggenEnable').checked = true;
            updateSigGen();
        }
        function applySigGenState(d) {
            document.getElementById('siggenEnable').checked = d.enabled;
            document.getElementById('siggenFields').style.display = d.enabled ? '' : 'none';
            document.getElementById('siggenWaveform').value = d.waveform;
            document.getElementById('siggenFreq').value = d.frequency;
            document.getElementById('siggenFreqVal').textContent = Math.round(d.frequency);
            document.getElementById('siggenAmp').value = d.amplitude;
            document.getElementById('siggenAmpVal').textContent = Math.round(d.amplitude);
            document.getElementById('siggenChannel').value = d.channel;
            document.getElementById('siggenOutputMode').value = d.outputMode;
            document.getElementById('siggenSweepSpeed').value = d.sweepSpeed;
            if (d.targetAdc !== undefined) document.getElementById('siggenTargetAdc').value = d.targetAdc;
            document.getElementById('siggenSweepGroup').style.display = d.waveform === 3 ? '' : 'none';
            document.getElementById('siggenPwmNote').style.display = d.outputMode === 1 ? '' : 'none';
        }

        // ===== THD Measurement =====
        var _thdMeasuring = false;

        function startThdMeasurement() {
            var freq = parseFloat(document.getElementById('siggenFreq').value) || 1000;
            var avg = parseInt(document.getElementById('thdAverages').value) || 8;
            _thdMeasuring = true;
            _updateThdProgress(0, avg);
            document.getElementById('thdMeasureBtn').disabled = true;
            document.getElementById('thdStopBtn').disabled = false;
            document.getElementById('thdResults').style.display = 'none';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'startThdMeasurement', freq: freq, averages: avg }));
            }
        }

        function stopThdMeasurement() {
            _thdMeasuring = false;
            document.getElementById('thdMeasureBtn').disabled = false;
            document.getElementById('thdStopBtn').disabled = true;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'stopThdMeasurement' }));
            }
        }

        function _updateThdProgress(processed, target) {
            var bar = document.getElementById('thdProgressBar');
            var label = document.getElementById('thdProgressLabel');
            if (!bar || !label) return;
            var pct = target > 0 ? Math.round((processed / target) * 100) : 0;
            bar.style.width = pct + '%';
            label.textContent = processed + ' / ' + target + ' frames';
        }

        function handleThdResult(data) {
            if (!_thdMeasuring && !data.valid) return;
            _updateThdProgress(data.framesProcessed || 0, data.framesTarget || 0);
            if (data.valid) {
                _thdMeasuring = false;
                var measureBtn = document.getElementById('thdMeasureBtn');
                var stopBtn = document.getElementById('thdStopBtn');
                if (measureBtn) measureBtn.disabled = false;
                if (stopBtn) stopBtn.disabled = true;
                _renderThdResults(data);
            }
        }

        function _renderThdResults(data) {
            var container = document.getElementById('thdResults');
            if (!container) return;
            var harmonicNames = ['2nd', '3rd', '4th', '5th', '6th', '7th', '8th', '9th'];
            var harmonics = data.harmonicLevels || [];
            var rows = '';
            for (var i = 0; i < harmonicNames.length && i < harmonics.length; i++) {
                rows += '<tr><td>' + harmonicNames[i] + '</td><td>' + harmonics[i].toFixed(1) + ' dB</td></tr>';
            }
            container.innerHTML =
                '<div class="thd-summary">' +
                '  <span class="thd-metric"><span class="thd-metric-label">THD+N</span><span class="thd-metric-value">' + (data.thdPlusNPercent || 0).toFixed(4) + '%</span></span>' +
                '  <span class="thd-metric"><span class="thd-metric-label">THD+N</span><span class="thd-metric-value">' + (data.thdPlusNDb || 0).toFixed(1) + ' dB</span></span>' +
                '  <span class="thd-metric"><span class="thd-metric-label">Fundamental</span><span class="thd-metric-value">' + (data.fundamentalDbfs || 0).toFixed(1) + ' dBFS</span></span>' +
                '</div>' +
                '<table class="thd-harmonics-table"><thead><tr><th>Harmonic</th><th>Level (rel)</th></tr></thead>' +
                '<tbody>' + rows + '</tbody></table>';
            container.style.display = 'block';
        }
