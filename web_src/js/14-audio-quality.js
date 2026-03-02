        // ===== Emergency Limiter =====
        function updateEmergencyLimiter(field) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            if (field === 'enabled') {
                ws.send(JSON.stringify({
                    type: 'setEmergencyLimiterEnabled',
                    enabled: document.getElementById('emergencyLimiterEnable').checked
                }));
            } else if (field === 'threshold') {
                ws.send(JSON.stringify({
                    type: 'setEmergencyLimiterThreshold',
                    threshold: parseFloat(document.getElementById('emergencyLimiterThreshold').value)
                }));
            }
        }
        function applyEmergencyLimiterState(d) {
            document.getElementById('emergencyLimiterEnable').checked = d.enabled;
            document.getElementById('emergencyLimiterThreshold').value = d.threshold;
            document.getElementById('emergencyLimiterThresholdVal').textContent = parseFloat(d.threshold).toFixed(1);

            // Update status badge
            var statusBadge = document.getElementById('emergencyLimiterStatusBadge');
            var statusText = document.getElementById('emergencyLimiterStatus');
            if (d.active) {
                statusBadge.textContent = 'ACTIVE';
                statusBadge.style.background = '#F44336';
                statusText.textContent = 'Limiting';
                statusText.style.color = '#F44336';
            } else {
                statusBadge.textContent = 'Idle';
                statusBadge.style.background = '#4CAF50';
                statusText.textContent = 'Idle';
                statusText.style.color = '';
            }

            // Update metrics
            document.getElementById('emergencyLimiterGR').textContent = parseFloat(d.gainReductionDb).toFixed(1) + ' dB';
            document.getElementById('emergencyLimiterTriggers').textContent = d.triggerCount || 0;
        }

        function updateAudioQuality(field) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            if (field === 'enabled') {
                ws.send(JSON.stringify({
                    type: 'setAudioQualityEnabled',
                    enabled: document.getElementById('audioQualityEnabled').checked
                }));
            } else if (field === 'threshold') {
                ws.send(JSON.stringify({
                    type: 'setAudioQualityThreshold',
                    threshold: parseFloat(document.getElementById('audioQualityThreshold').value)
                }));
            }
        }

        function applyAudioQualityState(d) {
            document.getElementById('audioQualityEnabled').checked = d.enabled;
            document.getElementById('audioQualityThreshold').value = d.threshold;
        }

        function applyAudioQualityDiag(d) {
            document.getElementById('aqGlitchesTotal').textContent = d.glitchesTotal || 0;
            document.getElementById('aqGlitchesMinute').textContent = d.glitchesLastMinute || 0;
            document.getElementById('aqLatencyAvg').textContent = d.timingAvgMs ? parseFloat(d.timingAvgMs).toFixed(2) + ' ms' : '--';
            document.getElementById('aqLastGlitchType').textContent = d.lastGlitchTypeStr || '--';

            // Correlation badges
            var dspBadge = document.getElementById('aqCorrelationDsp');
            var wifiBadge = document.getElementById('aqCorrelationWifi');

            if (d.correlationDspSwap) {
                dspBadge.textContent = 'YES';
                dspBadge.style.background = '#F44336';
            } else {
                dspBadge.textContent = 'No';
                dspBadge.style.background = '#4CAF50';
            }

            if (d.correlationWifi) {
                wifiBadge.textContent = 'YES';
                wifiBadge.style.background = '#F44336';
            } else {
                wifiBadge.textContent = 'No';
                wifiBadge.style.background = '#4CAF50';
            }
        }

        function resetAudioQualityStats() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            ws.send(JSON.stringify({ type: 'resetAudioQualityStats' }));
        }
