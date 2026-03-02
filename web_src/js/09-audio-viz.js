        let LERP_SPEED = 0.25;
        let VU_LERP = 0.3;

        function updateLerpFactors(rateMs) {
            LERP_SPEED = Math.min(0.25 * (50 / rateMs), 0.7);
            VU_LERP = Math.min(0.3 * (50 / rateMs), 0.7);
        }

        function drawRoundedBar(ctx, x, y, w, h, radius) {
            if (h < 1) return;
            const r = Math.min(radius, w / 2, h);
            ctx.beginPath();
            ctx.moveTo(x, y + h);
            ctx.lineTo(x, y + r);
            ctx.arcTo(x, y, x + r, y, r);
            ctx.arcTo(x + w, y, x + w, y + r, r);
            ctx.lineTo(x + w, y + h);
            ctx.closePath();
            ctx.fill();
        }

        function startAudioAnimation() {
            if (!audioAnimFrameId) audioAnimFrameId = requestAnimationFrame(audioAnimLoop);
        }

        function audioAnimLoop() {
            audioAnimFrameId = null;
            let needsMore = false;

            for (let a = 0; a < NUM_ADCS; a++) {
                // Lerp waveform
                if (waveformCurrent[a] && waveformTarget[a]) {
                    for (let i = 0; i < waveformCurrent[a].length && i < waveformTarget[a].length; i++) {
                        const diff = waveformTarget[a][i] - waveformCurrent[a][i];
                        if (Math.abs(diff) > 0.5) { waveformCurrent[a][i] += diff * LERP_SPEED; needsMore = true; }
                        else waveformCurrent[a][i] = waveformTarget[a][i];
                    }
                }

                // Lerp spectrum
                for (let i = 0; i < 16; i++) {
                    const diff = spectrumTarget[a][i] - spectrumCurrent[a][i];
                    if (Math.abs(diff) > 0.001) { spectrumCurrent[a][i] += diff * LERP_SPEED; needsMore = true; }
                    else spectrumCurrent[a][i] = spectrumTarget[a][i];
                }

                // Lerp dominant freq
                const fDiff = targetDominantFreq[a] - currentDominantFreq[a];
                if (Math.abs(fDiff) > 1) { currentDominantFreq[a] += fDiff * LERP_SPEED; needsMore = true; }
                else currentDominantFreq[a] = targetDominantFreq[a];

                drawAudioWaveform(waveformCurrent[a], a);
                drawSpectrumBars(spectrumCurrent[a], currentDominantFreq[a], a);
            }

            if (needsMore) audioAnimFrameId = requestAnimationFrame(audioAnimLoop);
        }

        function drawAudioWaveform(data, adcIndex) {
            adcIndex = adcIndex || 0;
            const canvas = document.getElementById('audioWaveformCanvas' + adcIndex);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const dpr = window.devicePixelRatio;
            const resized = resizeCanvasIfNeeded(canvas);
            if (resized === -1) return; // canvas not laid out yet (0x0)
            const dims = canvasDims[canvas.id];
            const w = dims.w, h = dims.h;

            const isNight = document.body.classList.contains('night-mode');
            const plotX = 36, plotY = 4;
            const plotW = w - 40, plotH = h - 22;

            // Offscreen background cache — grid, labels, axes drawn once
            const bgKey = 'wf' + adcIndex;
            if (resized || !bgCache[bgKey]) {
                const offscreen = document.createElement('canvas');
                offscreen.width = dims.tw;
                offscreen.height = dims.th;
                const bgCtx = offscreen.getContext('2d');
                bgCtx.scale(dpr, dpr);
                const bgColor = isNight ? '#1E1E1E' : '#F5F5F5';
                const gridColor = isNight ? '#333333' : '#D0D0D0';
                const labelColor = isNight ? '#999999' : '#757575';
                bgCtx.fillStyle = bgColor;
                bgCtx.fillRect(0, 0, w, h);
                bgCtx.font = '10px -apple-system, sans-serif';
                bgCtx.textAlign = 'right';
                bgCtx.textBaseline = 'middle';
                const yLabels = ['+1.0', '+0.5', '0', '-0.5', '-1.0'];
                const yValues = [1.0, 0.5, 0, -0.5, -1.0];
                for (let i = 0; i < yLabels.length; i++) {
                    const yPos = plotY + plotH * (1 - (yValues[i] + 1) / 2);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(yLabels[i], plotX - 4, yPos);
                    bgCtx.strokeStyle = gridColor;
                    bgCtx.lineWidth = 0.5;
                    bgCtx.beginPath();
                    bgCtx.moveTo(plotX, yPos);
                    bgCtx.lineTo(plotX + plotW, yPos);
                    bgCtx.stroke();
                }
                const sampleRate = 48000;
                const sel = document.getElementById('audioSampleRateSelect');
                const sr = sel ? parseInt(sel.value) || sampleRate : sampleRate;
                const numSamples = 256;
                const totalTimeMs = (numSamples / sr) * 1000;
                bgCtx.textAlign = 'center';
                bgCtx.textBaseline = 'top';
                const numXLabels = 5;
                for (let i = 0; i <= numXLabels; i++) {
                    const xFrac = i / numXLabels;
                    const xPos = plotX + xFrac * plotW;
                    const timeVal = (xFrac * totalTimeMs).toFixed(1);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(timeVal + 'ms', xPos, plotY + plotH + 4);
                    if (i > 0 && i < numXLabels) {
                        bgCtx.strokeStyle = gridColor;
                        bgCtx.lineWidth = 0.5;
                        bgCtx.beginPath();
                        bgCtx.moveTo(xPos, plotY);
                        bgCtx.lineTo(xPos, plotY + plotH);
                        bgCtx.stroke();
                    }
                }
                bgCache[bgKey] = offscreen;
            }

            // Blit cached background
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(bgCache[bgKey], 0, 0);
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

            if (!data || data.length === 0) return;

            const len = data.length;
            let scale = 1;
            if (waveformAutoScaleEnabled) {
                let maxDev = 0;
                for (let i = 0; i < len; i++) {
                    const dev = Math.abs(data[i] - 128);
                    if (dev > maxDev) maxDev = dev;
                }
                scale = maxDev > 2 ? (0.45 * 255) / maxDev : 1;
            }

            // Draw waveform — no shadow blur (saves ~2-3ms GPU convolution per frame)
            ctx.strokeStyle = '#FF9800';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            const step = plotW / (len - 1);
            for (let i = 0; i < len; i++) {
                const x = plotX + i * step;
                const centered = (data[i] - 128) * scale + 128;
                const y = plotY + (1 - centered / 255) * plotH;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function drawSpectrumBars(bands, freq, adcIndex) {
            adcIndex = adcIndex || 0;
            const canvas = document.getElementById('audioSpectrumCanvas' + adcIndex);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const dpr = window.devicePixelRatio;
            const resized = resizeCanvasIfNeeded(canvas);
            if (resized === -1) return; // canvas not laid out yet (0x0)
            const dims = canvasDims[canvas.id];
            const w = dims.w, h = dims.h;

            const isNight = document.body.classList.contains('night-mode');
            const plotX = 32, plotY = 4;
            const plotW = w - 36, plotH = h - 22;

            // Compute bar geometry (needed for both bg cache and bar drawing)
            const numBands = (bands && bands.length) ? Math.min(bands.length, 16) : 16;
            const gap = 2;
            const barWidth = (plotW - gap * (numBands - 1)) / numBands;
            const bandEdges = [0, 40, 80, 160, 315, 630, 1250, 2500, 5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000];

            // Offscreen background cache — grid, labels, axes drawn once
            const bgKey = 'sp' + adcIndex;
            if (resized || !bgCache[bgKey]) {
                const offscreen = document.createElement('canvas');
                offscreen.width = dims.tw;
                offscreen.height = dims.th;
                const bgCtx = offscreen.getContext('2d');
                bgCtx.scale(dpr, dpr);
                const bgColor = isNight ? '#1E1E1E' : '#F5F5F5';
                const gridColor = isNight ? '#333333' : '#D0D0D0';
                const labelColor = isNight ? '#999999' : '#757575';
                bgCtx.fillStyle = bgColor;
                bgCtx.fillRect(0, 0, w, h);
                bgCtx.font = '10px -apple-system, sans-serif';
                bgCtx.textAlign = 'right';
                bgCtx.textBaseline = 'middle';
                const dbLevels = [0, -12, -24, -36];
                for (let i = 0; i < dbLevels.length; i++) {
                    const db = dbLevels[i];
                    const linearVal = Math.pow(10, db / 20);
                    const yPos = plotY + plotH * (1 - linearVal);
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(db + 'dB', plotX - 4, yPos);
                    bgCtx.strokeStyle = gridColor;
                    bgCtx.lineWidth = 0.5;
                    bgCtx.beginPath();
                    bgCtx.moveTo(plotX, yPos);
                    bgCtx.lineTo(plotX + plotW, yPos);
                    bgCtx.stroke();
                }
                bgCtx.textAlign = 'center';
                bgCtx.textBaseline = 'top';
                for (let i = 0; i < numBands; i += 2) {
                    const centerFreq = Math.sqrt(bandEdges[i] * bandEdges[i + 1]);
                    const xCenter = plotX + i * (barWidth + gap) + barWidth / 2;
                    bgCtx.fillStyle = labelColor;
                    bgCtx.fillText(formatFreq(centerFreq), xCenter, plotY + plotH + 4);
                }
                bgCache[bgKey] = offscreen;
            }

            // Blit cached background
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(bgCache[bgKey], 0, 0);
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

            // Update dominant frequency readout
            const freqEl = document.getElementById('dominantFreq' + adcIndex);
            if (freqEl) {
                freqEl.textContent = freq > 0 ? freq.toFixed(0) + ' Hz' : '-- Hz';
            }

            if (!bands || bands.length === 0) return;

            const now = performance.now();

            // Draw bars
            for (let i = 0; i < numBands; i++) {
                const val = Math.min(Math.max(bands[i], 0), 1);
                const barHeight = val * plotH;
                const x = plotX + i * (barWidth + gap);
                const y = plotY + plotH - barHeight;

                if (ledBarMode) {
                    // LED segmented bar mode
                    const segH = 4, segGap = 1.5;
                    const totalSegs = Math.floor(plotH / (segH + segGap));
                    const litSegs = Math.round(val * totalSegs);
                    for (let s = 0; s < totalSegs; s++) {
                        const segY = plotY + plotH - (s + 1) * (segH + segGap);
                        const frac = s / totalSegs;
                        let r, g, b;
                        if (frac < 0.6) { r = 76; g = 175; b = 80; }          // green
                        else if (frac < 0.8) { r = 255; g = 193; b = 7; }      // yellow
                        else { r = 244; g = 67; b = 54; }                        // red
                        if (s < litSegs) {
                            ctx.fillStyle = `rgb(${r},${g},${b})`;
                        } else {
                            ctx.fillStyle = `rgba(${r},${g},${b},0.05)`;
                        }
                        drawRoundedBar(ctx, x, segY, barWidth, segH, 1);
                    }
                } else {
                    // Standard smooth bars with rounded tops — use pre-computed color LUT
                    ctx.fillStyle = spectrumColorLUT[Math.round(val * 255)];
                    drawRoundedBar(ctx, x, y, barWidth, barHeight, 3);
                }

                // Peak hold indicator
                if (val > spectrumPeaks[adcIndex][i]) {
                    spectrumPeaks[adcIndex][i] = val;
                    spectrumPeakTimes[adcIndex][i] = now;
                }
                const elapsed = now - spectrumPeakTimes[adcIndex][i];
                if (elapsed > 1500) {
                    spectrumPeaks[adcIndex][i] -= 0.002 * (elapsed - 1500) / 16.67;
                    if (spectrumPeaks[adcIndex][i] < val) spectrumPeaks[adcIndex][i] = val;
                }
                if (spectrumPeaks[adcIndex][i] > 0.01) {
                    const peakY = plotY + plotH - spectrumPeaks[adcIndex][i] * plotH;
                    ctx.fillStyle = isNight ? 'rgba(255,255,255,0.85)' : 'rgba(0,0,0,0.6)';
                    ctx.fillRect(x, peakY, barWidth, 2);
                }
            }
        }

        function linearToDbPercent(val) {
            if (val <= 0) return 0;
            const db = 20 * Math.log10(val);
            const clamped = Math.max(db, -60);
            return ((clamped + 60) / 60) * 100;
        }

        function formatDbFS(val) {
            if (val <= 0) return '-inf dBFS';
            const db = 20 * Math.log10(val);
            return db.toFixed(1) + ' dBFS';
        }

        function startVuAnimation() {
            if (!vuAnimFrameId) vuAnimFrameId = requestAnimationFrame(vuAnimLoop);
        }

        function vuAnimLoop() {
            vuAnimFrameId = null;
            for (let a = 0; a < NUM_ADCS; a++) {
                for (let ch = 0; ch < 2; ch++) {
                    vuCurrent[a][ch] += (vuTargetArr[a][ch] - vuCurrent[a][ch]) * VU_LERP;
                    peakCurrent[a][ch] += (peakTargetArr[a][ch] - peakCurrent[a][ch]) * VU_LERP;
                }
                updateLevelMeters(a, vuCurrent[a][0], vuCurrent[a][1], peakCurrent[a][0], peakCurrent[a][1]);
            }
            // Update signal detection indicator — use cached refs if available
            const refs = vuDomRefs || {};
            const dot = refs['dot'] || document.getElementById('audioSignalDot');
            const txt = refs['txt'] || document.getElementById('audioSignalText');
            if (dot) dot.classList.toggle('active', vuDetected);
            if (txt) txt.textContent = vuDetected ? 'Detected' : 'Not detected';
            if (audioSubscribed) vuAnimFrameId = requestAnimationFrame(vuAnimLoop);
        }

        function drawPPM(canvasId, level, peak) {
            const canvas = document.getElementById(canvasId);
            if (!canvas) return;
            const ctx = canvas.getContext('2d');
            const w = canvas.offsetWidth;
            const h = canvas.offsetHeight;
            if (canvas.width !== w || canvas.height !== h) {
                canvas.width = w;
                canvas.height = h;
            }
            ctx.clearRect(0, 0, w, h);

            const segments = 48;
            const gap = 1;
            const segW = (w - (segments - 1) * gap) / segments;
            const pct = linearToDbPercent(level) / 100;
            const litCount = Math.round(pct * segments);
            const greenEnd = Math.round(segments * (40 / 60));
            const yellowEnd = Math.round(segments * (54 / 60));

            for (let i = 0; i < segments; i++) {
                const x = i * (segW + gap);
                if (i < litCount) {
                    if (i < greenEnd) ctx.fillStyle = '#4CAF50';
                    else if (i < yellowEnd) ctx.fillStyle = '#FFC107';
                    else ctx.fillStyle = '#F44336';
                } else {
                    ctx.fillStyle = 'rgba(255,255,255,0.06)';
                }
                ctx.fillRect(x, 0, segW, h);
            }

            // Peak marker
            const peakPct = linearToDbPercent(peak) / 100;
            const peakSeg = Math.min(Math.round(peakPct * segments), segments - 1);
            if (peak > 0) {
                const px = peakSeg * (segW + gap);
                ctx.fillStyle = '#FFFFFF';
                ctx.fillRect(px, 0, segW, h);
            }
        }

        function updateLevelMeters(adcIdx, vu1, vu2, peak1, peak2) {
            vu1 = Math.min(Math.max(vu1, 0), 1);
            vu2 = Math.min(Math.max(vu2, 0), 1);
            peak1 = Math.min(Math.max(peak1, 0), 1);
            peak2 = Math.min(Math.max(peak2, 0), 1);

            // Use cached DOM refs if available, else fallback to getElementById
            const refs = vuDomRefs || {};

            if (!vuSegmentedMode) {
                // Continuous dB-scaled bars
                const pct1 = linearToDbPercent(vu1);
                const pct2 = linearToDbPercent(vu2);
                const fillL = refs['fillL' + adcIdx] || document.getElementById('vuFill' + adcIdx + 'L');
                const fillR = refs['fillR' + adcIdx] || document.getElementById('vuFill' + adcIdx + 'R');
                if (fillL) fillL.style.width = pct1 + '%';
                if (fillR) fillR.style.width = pct2 + '%';

                const pkPct1 = linearToDbPercent(peak1);
                const pkPct2 = linearToDbPercent(peak2);
                const pkL = refs['pkL' + adcIdx] || document.getElementById('vuPeak' + adcIdx + 'L');
                const pkR = refs['pkR' + adcIdx] || document.getElementById('vuPeak' + adcIdx + 'R');
                if (pkL) pkL.style.left = pkPct1 + '%';
                if (pkR) pkR.style.left = pkPct2 + '%';

                const dbL = refs['dbL' + adcIdx] || document.getElementById('vuDb' + adcIdx + 'L');
                const dbR = refs['dbR' + adcIdx] || document.getElementById('vuDb' + adcIdx + 'R');
                if (dbL) dbL.textContent = formatDbFS(vu1);
                if (dbR) dbR.textContent = formatDbFS(vu2);
            } else {
                // Segmented PPM canvas
                drawPPM('ppmCanvas' + adcIdx + 'L', vu1, peak1);
                drawPPM('ppmCanvas' + adcIdx + 'R', vu2, peak2);

                const dbSegL = refs['dbSegL' + adcIdx] || document.getElementById('vuDbSeg' + adcIdx + 'L');
                const dbSegR = refs['dbSegR' + adcIdx] || document.getElementById('vuDbSeg' + adcIdx + 'R');
                if (dbSegL) dbSegL.textContent = formatDbFS(vu1);
                if (dbSegR) dbSegR.textContent = formatDbFS(vu2);
            }
            // Update combined dBFS readout from smoothed VU (matches per-channel source)
            var vuC = Math.sqrt((vu1 * vu1 + vu2 * vu2) * 0.5);
            var el = document.getElementById('adcReadout' + adcIdx);
            if (el) {
                var dbStr = vuC > 0 ? (20 * Math.log10(vuC)).toFixed(1) + ' dBFS' : '-inf dBFS';
                var old = el.textContent;
                var vrmsPart = old.indexOf('|') >= 0 ? old.substring(old.indexOf('|')) : '| -- Vrms';
                el.textContent = dbStr + ' ' + vrmsPart;
            }
            if (typeof overviewUpdateAdc === 'function') overviewUpdateAdc(adcIdx, vu1, vu2);
            if (typeof updateInputOverview === 'function') updateInputOverview();
        }

        function toggleVuMode(seg) {
            vuSegmentedMode = seg;
            localStorage.setItem('vuSegmented', seg);
            for (let a = 0; a < NUM_ADCS; a++) {
                var cont = document.getElementById('vuContinuous' + a);
                var segDiv = document.getElementById('vuSegmented' + a);
                if (cont) cont.style.display = seg ? 'none' : '';
                if (segDiv) segDiv.style.display = seg ? '' : 'none';
            }
        }

        function toggleGraphDisabled(id, disabled) {
            var el = document.getElementById(id);
            if (el) { if (disabled) el.classList.add('graph-disabled'); else el.classList.remove('graph-disabled'); }
        }

        function setGraphEnabled(graph, enabled) {
            var map = {vuMeter:'setVuMeterEnabled', waveform:'setWaveformEnabled', spectrum:'setSpectrumEnabled'};
            var contentMap = {vuMeter:'vuMeterContent', waveform:'waveformContent', spectrum:'spectrumContent'};
            toggleGraphDisabled(contentMap[graph], !enabled);
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(JSON.stringify({type:map[graph], enabled:enabled}));
        }
