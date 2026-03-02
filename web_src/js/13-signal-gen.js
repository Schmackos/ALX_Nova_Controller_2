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
            if (typeof overviewApplySigGenState === 'function') overviewApplySigGenState(d);
        }
