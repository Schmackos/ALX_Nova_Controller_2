// ===== Performance History =====

        var _eepromPresetsLoaded = false;
        function eepromLoadPresets() {
            if (_eepromPresetsLoaded) return;
            apiFetch('/api/dac/eeprom/presets')
            .then(r => r.safeJson())
            .then(d => {
                if (!d.success) return;
                var sel = document.getElementById('eepromPreset');
                if (!sel) return;
                d.presets.forEach(function(p) {
                    var opt = document.createElement('option');
                    opt.value = JSON.stringify(p);
                    opt.textContent = p.deviceName + ' (' + p.manufacturer + ')';
                    sel.appendChild(opt);
                });
                _eepromPresetsLoaded = true;
            }).catch(function(){});
        }

        function handleEepromDiag(eep) {
            if (!eep) return;
            var chipDetected = eep.scanned && eep.i2cMask > 0;
            var chipEmpty = chipDetected && !eep.found;
            var chipAddr = 0;
            if (eep.i2cMask > 0) {
                for (var b = 0; b < 8; b++) { if (eep.i2cMask & (1 << b)) { chipAddr = 0x50 + b; break; } }
            }
            var st = document.getElementById('eepromStatus');
            if (st) {
                if (eep.found) st.textContent = 'Programmed';
                else if (chipEmpty) st.textContent = 'Empty (blank)';
                else if (eep.scanned) st.textContent = 'No EEPROM detected';
                else st.textContent = 'Not scanned';
            }
            var addr = document.getElementById('eepromI2cAddr');
            if (addr) addr.textContent = (eep.found || chipDetected) ? '0x' + (eep.found ? eep.addr : chipAddr).toString(16).padStart(2,'0').toUpperCase() : '—';
            var cnt = document.getElementById('eepromI2cCount');
            if (cnt) cnt.textContent = eep.scanned ? eep.i2cDevices : '—';
            var badge = document.getElementById('eepromFoundBadge');
            if (badge) {
                badge.style.display = eep.scanned ? '' : 'none';
                if (eep.found) { badge.textContent = 'Programmed'; badge.style.background = '#4CAF50'; }
                else if (chipEmpty) { badge.textContent = 'Empty'; badge.style.background = '#FF9800'; }
                else { badge.textContent = 'Not Found'; badge.style.background = '#F44336'; }
                badge.style.color = '#fff';
            }
            var el;
            el = document.getElementById('dbgEepromFound');
            if (el) {
                if (eep.found) el.textContent = 'Yes @ 0x' + eep.addr.toString(16).padStart(2,'0').toUpperCase();
                else if (chipEmpty) el.textContent = 'Empty (blank) @ 0x' + chipAddr.toString(16).padStart(2,'0').toUpperCase();
                else if (eep.scanned) el.textContent = 'No';
                else el.textContent = '—';
            }
            el = document.getElementById('dbgEepromAddr');
            if (el) el.textContent = (eep.found || chipDetected) ? '0x' + (eep.found ? eep.addr : chipAddr).toString(16).padStart(2,'0').toUpperCase() : '—';
            el = document.getElementById('dbgI2cCount');
            if (el) el.textContent = eep.i2cDevices != null ? eep.i2cDevices : '—';
            el = document.getElementById('dbgEepromRdErr');
            if (el) el.textContent = eep.readErrors || 0;
            el = document.getElementById('dbgEepromWrErr');
            if (el) el.textContent = eep.writeErrors || 0;
            if (eep.found) {
                var fields = { dbgEepromDeviceId: '0x' + (eep.deviceId||0).toString(16).padStart(4,'0').toUpperCase(),
                               dbgEepromName: eep.deviceName || '—', dbgEepromMfr: eep.manufacturer || '—',
                               dbgEepromRev: eep.hwRevision != null ? eep.hwRevision : '—',
                               dbgEepromCh: eep.maxChannels || '—',
                               dbgEepromDacAddr: eep.dacI2cAddress ? '0x' + eep.dacI2cAddress.toString(16).padStart(2,'0') : 'None' };
                Object.entries(fields).forEach(function(kv) { el = document.getElementById(kv[0]); if (el) el.textContent = kv[1]; });
                var flagStrs = [];
                if (eep.flags & 1) flagStrs.push('IndepClk');
                if (eep.flags & 2) flagStrs.push('HW Vol');
                if (eep.flags & 4) flagStrs.push('Filters');
                el = document.getElementById('dbgEepromFlags');
                if (el) el.textContent = flagStrs.length ? flagStrs.join(', ') : 'None';
                el = document.getElementById('dbgEepromRates');
                if (el) el.textContent = (eep.sampleRates || []).join(', ') || '—';
            } else {
                ['dbgEepromDeviceId','dbgEepromName','dbgEepromMfr','dbgEepromRev','dbgEepromCh','dbgEepromDacAddr','dbgEepromFlags','dbgEepromRates'].forEach(function(id) {
                    el = document.getElementById(id); if (el) el.textContent = '—';
                });
            }
            eepromLoadPresets();
        }

        function updateHardwareStats(data) {
            var el, i;
            // Update ADC count from hardware_stats (fires on all tabs)
            if (data.audio && data.audio.numAdcsDetected !== undefined) {
                numAdcsDetected = data.audio.numAdcsDetected;
            }
            // CPU Stats
            if (data.cpu) {
                var cpuCalibrating = (data.cpu.usageCore0 < 0 || data.cpu.usageCore1 < 0);
                document.getElementById('cpuTotal').textContent = cpuCalibrating ? 'Calibrating...' : Math.round(data.cpu.usageTotal || 0) + '%';
                document.getElementById('cpuCore0').textContent = cpuCalibrating ? '...' : Math.round(data.cpu.usageCore0 || 0) + '%';
                document.getElementById('cpuCore1').textContent = cpuCalibrating ? '...' : Math.round(data.cpu.usageCore1 || 0) + '%';
                document.getElementById('cpuTemp').textContent = (data.cpu.temperature || 0).toFixed(1) + '°C';
                document.getElementById('cpuFreq').textContent = (data.cpu.freqMHz || 0) + ' MHz';
                document.getElementById('cpuModel').textContent = data.cpu.model || '--';
                document.getElementById('cpuRevision').textContent = data.cpu.revision || '--';
                document.getElementById('cpuCores').textContent = data.cpu.cores || '--';
            }

            // Memory Stats (Heap)
            if (data.memory) {
                const heapTotal = data.memory.heapTotal || 0;
                const heapFree = data.memory.heapFree || 0;
                const heapPercent = heapTotal > 0 ? Math.round((1 - heapFree / heapTotal) * 100) : 0;
                document.getElementById('heapPercent').textContent = heapPercent + '%';
                document.getElementById('heapFree').textContent = formatBytes(heapFree);
                document.getElementById('heapTotal').textContent = formatBytes(heapTotal);
                document.getElementById('heapMinFree').textContent = formatBytes(data.memory.heapMinFree || 0);
                document.getElementById('heapMaxBlock').textContent = formatBytes(data.memory.heapMaxBlock || 0);

                // Heap critical indicator
                const critRow = document.getElementById('heapCriticalRow');
                if (critRow) critRow.style.display = data.heapCritical ? '' : 'none';

                // DMA allocation failure indicator
                const dmaRow = document.getElementById('dmaAllocFailRow');
                if (dmaRow) {
                    dmaRow.style.display = data.dmaAllocFailed ? '' : 'none';
                    const dmaVal = document.getElementById('dmaAllocFailValue');
                    if (dmaVal && data.dmaAllocFailed) {
                        const mask = data.dmaAllocFailMask || 0;
                        const lanes = [];
                        for (let i = 0; i < 8; i++) { if (mask & (1 << i)) lanes.push('Lane ' + i); }
                        for (let i = 0; i < 8; i++) { if (mask & (1 << (i + 8))) lanes.push('Sink ' + i); }
                        dmaVal.textContent = lanes.length ? lanes.join(', ') : 'YES';
                    }
                }

                // PSRAM
                const psramTotal = data.memory.psramTotal || 0;
                const psramFree = data.memory.psramFree || 0;
                const psramPercent = psramTotal > 0 ? Math.round((1 - psramFree / psramTotal) * 100) : 0;
                document.getElementById('psramPercent').textContent = psramTotal > 0 ? psramPercent + '%' : 'N/A';
                document.getElementById('psramFree').textContent = psramTotal > 0 ? formatBytes(psramFree) : 'N/A';
                document.getElementById('psramTotal').textContent = psramTotal > 0 ? formatBytes(psramTotal) : 'N/A';
            }

            // PSRAM allocation tracking
            if (data.psramFallbackCount !== undefined) {
                var fbRow = document.getElementById('psramFallbackRow');
                if (fbRow) {
                    fbRow.style.display = data.psramFallbackCount > 0 ? '' : 'none';
                    var fbVal = document.getElementById('psramFallbackCount');
                    if (fbVal) fbVal.textContent = data.psramFallbackCount;
                }
                var fbBadge = document.getElementById('psramFallbackBadge');
                if (fbBadge) {
                    if (data.psramFallbackCount > 0) {
                        fbBadge.textContent = data.psramFallbackCount + ' fallback' + (data.psramFallbackCount > 1 ? 's' : '');
                        fbBadge.style.display = '';
                    } else {
                        fbBadge.style.display = 'none';
                    }
                }
                // Toast on new fallback
                if (typeof window._prevPsramFallbackCount === 'undefined') {
                    window._prevPsramFallbackCount = 0;
                }
                if (data.psramFallbackCount > window._prevPsramFallbackCount) {
                    showToast('PSRAM allocation fallback detected', 'warning');
                }
                window._prevPsramFallbackCount = data.psramFallbackCount;
            }

            // PSRAM pressure badge
            if (data.psramCritical !== undefined || data.psramWarning !== undefined) {
                var prBadge = document.getElementById('psramPressureBadge');
                if (prBadge) {
                    if (data.psramCritical) {
                        prBadge.textContent = 'CRITICAL';
                        prBadge.className = 'badge badge-red';
                        prBadge.style.display = '';
                    } else if (data.psramWarning) {
                        prBadge.textContent = 'WARNING';
                        prBadge.className = 'badge badge-amber';
                        prBadge.style.display = '';
                    } else {
                        prBadge.style.display = 'none';
                    }
                }
            }

            // PSRAM budget table
            if (data.heapBudget && Array.isArray(data.heapBudget)) {
                var budgetHeader = document.getElementById('psramBudgetHeader');
                if (budgetHeader && data.heapBudget.length > 0) budgetHeader.style.display = '';
                var budgetTbody = document.querySelector('#psramBudgetTable tbody');
                if (budgetTbody) {
                    budgetTbody.innerHTML = '';
                    for (var bi = 0; bi < data.heapBudget.length; bi++) {
                        var entry = data.heapBudget[bi];
                        if (entry.label && entry.bytes > 0) {
                            var bRow = document.createElement('tr');
                            var typeClass = entry.psram ? 'badge-green' : 'badge-amber';
                            var typeText = entry.psram ? 'PSRAM' : 'SRAM';
                            bRow.innerHTML = '<td>' + escapeHtml(entry.label) + '</td>' +
                                '<td>' + formatBytes(entry.bytes) + '</td>' +
                                '<td><span class="badge ' + typeClass + '">' + typeText + '</span></td>';
                            budgetTbody.appendChild(bRow);
                        }
                    }
                }
            }

            // Storage Stats
            if (data.storage) {
                document.getElementById('flashSize').textContent = formatBytes(data.storage.flashSize || 0);
                document.getElementById('sketchSize').textContent = formatBytes(data.storage.sketchSize || 0);
                document.getElementById('sketchFree').textContent = formatBytes(data.storage.sketchFree || 0);

                const sketchTotal = (data.storage.sketchSize || 0) + (data.storage.sketchFree || 0);
                const sketchPercent = sketchTotal > 0 ? Math.round((data.storage.sketchSize / sketchTotal) * 100) : 0;
                document.getElementById('sketchPercent').textContent = sketchPercent + '%';

                const LittleFSTotal = data.storage.LittleFSTotal || 0;
                const LittleFSUsed = data.storage.LittleFSUsed || 0;
                const LittleFSPercent = LittleFSTotal > 0 ? Math.round((LittleFSUsed / LittleFSTotal) * 100) : 0;
                document.getElementById('LittleFSPercent').textContent = LittleFSTotal > 0 ? LittleFSPercent + '%' : 'N/A';
                document.getElementById('LittleFSUsed').textContent = LittleFSTotal > 0 ? formatBytes(LittleFSUsed) : 'N/A';
                document.getElementById('LittleFSTotal').textContent = LittleFSTotal > 0 ? formatBytes(LittleFSTotal) : 'N/A';
            }

            // WiFi Stats
            if (data.wifi) {
                document.getElementById('wifiRssi').innerHTML = formatRssi(data.wifi.rssi);
                document.getElementById('wifiChannel').textContent = data.wifi.channel || '--';
                document.getElementById('apClients').textContent = data.wifi.apClients || 0;
            }

            // Audio ADC — per-ADC diagnostics from adcs array
            if (data.audio) {
                if (data.audio.adcs && Array.isArray(data.audio.adcs)) {
                    var adcs = data.audio.adcs;
                    // Show ADC 0 in the existing fields (legacy)
                    if (adcs.length > 0) {
                        document.getElementById('adcStatus').textContent = adcs[0].status || '--';
                        document.getElementById('adcNoiseFloor').textContent = adcs[0].noiseFloorDbfs !== undefined ? adcs[0].noiseFloorDbfs.toFixed(1) + ' dBFS' : '--';
                        document.getElementById('adcI2sErrors').textContent = adcs[0].i2sErrors !== undefined ? adcs[0].i2sErrors : '--';
                        document.getElementById('adcConsecutiveZeros').textContent = adcs[0].consecutiveZeros !== undefined ? adcs[0].consecutiveZeros : '--';
                        document.getElementById('adcTotalBuffers').textContent = adcs[0].totalBuffers !== undefined ? adcs[0].totalBuffers : '--';
                        var snr0 = document.getElementById('audioSnr0');
                        if (snr0 && adcs[0].snrDb !== undefined) snr0.textContent = adcs[0].snrDb.toFixed(1);
                        var sfdr0 = document.getElementById('audioSfdr0');
                        if (sfdr0 && adcs[0].sfdrDb !== undefined) sfdr0.textContent = adcs[0].sfdrDb.toFixed(1);
                    }
                    // Show ADC 1 fields if present
                    var el2 = document.getElementById('adcStatus1');
                    if (el2 && adcs.length > 1) {
                        el2.textContent = adcs[1].status || '--';
                        var nf2 = document.getElementById('adcNoiseFloor1');
                        if (nf2) nf2.textContent = adcs[1].noiseFloorDbfs !== undefined ? adcs[1].noiseFloorDbfs.toFixed(1) + ' dBFS' : '--';
                        var ie2 = document.getElementById('adcI2sErrors1');
                        if (ie2) ie2.textContent = adcs[1].i2sErrors !== undefined ? adcs[1].i2sErrors : '--';
                        var cz2 = document.getElementById('adcConsecutiveZeros1');
                        if (cz2) cz2.textContent = adcs[1].consecutiveZeros !== undefined ? adcs[1].consecutiveZeros : '--';
                        var tb2 = document.getElementById('adcTotalBuffers1');
                        if (tb2) tb2.textContent = adcs[1].totalBuffers !== undefined ? adcs[1].totalBuffers : '--';
                    }
                } else {
                    // Legacy flat format fallback
                    document.getElementById('adcStatus').textContent = data.audio.adcStatus || '--';
                    document.getElementById('adcNoiseFloor').textContent = (data.audio.noiseFloorDbfs !== undefined ? data.audio.noiseFloorDbfs.toFixed(1) + ' dBFS' : '--');
                    document.getElementById('adcI2sErrors').textContent = data.audio.i2sErrors !== undefined ? data.audio.i2sErrors : '--';
                    document.getElementById('adcConsecutiveZeros').textContent = data.audio.consecutiveZeros !== undefined ? data.audio.consecutiveZeros : '--';
                    document.getElementById('adcTotalBuffers').textContent = data.audio.totalBuffers !== undefined ? data.audio.totalBuffers : '--';
                }
                document.getElementById('adcSampleRate').textContent = data.audio.sampleRate ? (data.audio.sampleRate / 1000).toFixed(1) + ' kHz' : '--';
            }

            // Audio DAC diagnostics
            if (data.dac) {
                var d = data.dac;
                var statusEl = document.getElementById('dacStatus');
                if (statusEl) {
                    var statusText = d.ready ? 'Ready' : (d.enabled ? 'Not Ready' : 'Disabled');
                    statusEl.textContent = statusText;
                    statusEl.style.color = d.ready ? 'var(--success-color)' : (d.enabled ? 'var(--error-color)' : '');
                }
                el = document.getElementById('dacModel'); if (el) el.textContent = d.model || '--';
                el = document.getElementById('dacManufacturer'); if (el) el.textContent = d.manufacturer || '--';
                el = document.getElementById('dacDeviceId'); if (el) el.textContent = d.deviceId !== undefined ? '0x' + ('0000' + d.deviceId.toString(16)).slice(-4).toUpperCase() : '--';
                el = document.getElementById('dacDetection'); if (el) el.textContent = d.detected ? 'EEPROM (Auto)' : 'Manual';
                el = document.getElementById('dacDbgEnabled'); if (el) el.textContent = d.enabled ? 'Yes' : 'No';
                el = document.getElementById('dacDbgVolume'); if (el) el.textContent = d.volume !== undefined ? d.volume + '%' : '--';
                el = document.getElementById('dacDbgMute'); if (el) el.textContent = d.mute ? 'Yes' : 'No';
                el = document.getElementById('dacDbgChannels'); if (el) el.textContent = d.outputChannels || '--';
                el = document.getElementById('dacDbgFilter'); if (el) el.textContent = d.filterMode !== undefined ? d.filterMode : '--';
                el = document.getElementById('dacHwVolume'); if (el) el.textContent = d.hwVolume ? 'Yes' : 'No';
                el = document.getElementById('dacI2cControl'); if (el) el.textContent = d.i2cControl ? 'Yes' : 'No';
                el = document.getElementById('dacIndepClock'); if (el) el.textContent = d.independentClock ? 'Yes' : 'No';
                el = document.getElementById('dacHasFilters'); if (el) el.textContent = d.hasFilters ? 'Yes' : 'No';
                // TX diagnostics
                if (d.tx) {
                    el = document.getElementById('dacI2sTxEnabled'); if (el) {
                        el.textContent = d.tx.i2sTxEnabled ? 'Yes' : 'No';
                        el.style.color = d.tx.i2sTxEnabled ? 'var(--success-color)' : 'var(--error-color)';
                    }
                    el = document.getElementById('dacVolumeGain'); if (el) el.textContent = d.tx.volumeGain !== undefined ? parseFloat(d.tx.volumeGain).toFixed(4) : '--';
                    el = document.getElementById('dacTxWrites'); if (el) el.textContent = d.tx.writeCount || 0;
                    el = document.getElementById('dacTxData'); if (el) {
                        var written = d.tx.bytesWritten || 0;
                        var expected = d.tx.bytesExpected || 0;
                        var wKB = (written / 1024).toFixed(0);
                        var eKB = (expected / 1024).toFixed(0);
                        el.textContent = wKB + 'KB / ' + eKB + 'KB';
                        el.style.color = (expected > 0 && written < expected) ? 'var(--warning-color)' : '';
                    }
                    el = document.getElementById('dacTxPeak'); if (el) el.textContent = d.tx.peakSample || 0;
                    el = document.getElementById('dacTxZeroFrames'); if (el) el.textContent = d.tx.zeroFrames || 0;
                }
                el = document.getElementById('dacTxUnderruns'); if (el) {
                    el.textContent = d.txUnderruns || 0;
                    el.style.color = d.txUnderruns > 0 ? 'var(--warning-color)' : '';
                }
                // EEPROM diagnostics from hardware_stats
                if (d.eeprom) handleEepromDiag(d.eeprom);
            }

            if (data.audio) {
                // I2S Static Config
                if (data.audio.i2sConfig && Array.isArray(data.audio.i2sConfig)) {
                    for (i = 0; i < data.audio.i2sConfig.length && i < 2; i++) {
                        var c = data.audio.i2sConfig[i];
                        el = undefined;
                        el = document.getElementById('i2sMode' + i); if (el) el.textContent = c.mode || '--';
                        el = document.getElementById('i2sSampleRate' + i); if (el) el.textContent = c.sampleRate ? (c.sampleRate / 1000) + ' kHz' : '--';
                        el = document.getElementById('i2sBits' + i); if (el) el.textContent = c.bitsPerSample ? c.bitsPerSample + '-bit (24-bit payload)' : '--';
                        el = document.getElementById('i2sChannels' + i); if (el) el.textContent = c.channelFormat || '--';
                        el = document.getElementById('i2sDma' + i); if (el) el.textContent = c.dmaBufCount && c.dmaBufLen ? c.dmaBufCount + ' x ' + c.dmaBufLen : '--';
                        el = document.getElementById('i2sApll' + i); if (el) el.textContent = c.apll ? 'On' : 'Off';
                        el = document.getElementById('i2sMclk' + i); if (el) el.textContent = c.mclkHz ? (c.mclkHz / 1e6).toFixed(3) + ' MHz' : 'N/A';
                        el = document.getElementById('i2sFormat' + i); if (el) el.textContent = c.commFormat || '--';
                    }
                }

                // I2S Runtime Metrics
                if (data.audio.i2sRuntime) {
                    var rt = data.audio.i2sRuntime;
                    var stackEl = document.getElementById('i2sStackFree');
                    if (stackEl) {
                        var stackFree = rt.stackFree || 0;
                        var stackTotal = 10240;
                        var stackUsed = stackTotal - stackFree;
                        var stackPct = stackTotal > 0 ? Math.round(stackUsed / stackTotal * 100) : 0;
                        stackEl.textContent = stackFree + ' bytes (' + stackPct + '% used)';
                        stackEl.style.color = stackFree < 1024 ? 'var(--error-color)' : '';
                    }
                    var expectedBps = 187.5;
                    if (rt.buffersPerSec) {
                        for (i = 0; i < rt.buffersPerSec.length && i < 2; i++) {
                            var tEl = document.getElementById('i2sThroughput' + i);
                            if (tEl) {
                                var bps = parseFloat(rt.buffersPerSec[i]) || 0;
                                tEl.textContent = bps.toFixed(1) + ' buf/s';
                                tEl.style.color = (bps > 0 && bps < expectedBps * 0.9) ? 'var(--error-color)' : '';
                            }
                        }
                    }
                    if (rt.avgReadLatencyUs) {
                        for (i = 0; i < rt.avgReadLatencyUs.length && i < 2; i++) {
                            var lEl = document.getElementById('i2sLatency' + i);
                            if (lEl) {
                                var lat = parseFloat(rt.avgReadLatencyUs[i]) || 0;
                                lEl.textContent = (lat / 1000).toFixed(2) + ' ms';
                                lEl.style.color = lat > 10000 ? 'var(--error-color)' : '';
                            }
                        }
                    }
                }
            }

            // FreeRTOS Tasks — CPU load from hardware_stats
            if (data.cpu) {
                var tmCal = (data.cpu.usageCore0 < 0 || data.cpu.usageCore1 < 0);
                var el0 = document.getElementById('tmCpuCore0');
                if (el0) el0.textContent = tmCal ? '...' : Math.round(data.cpu.usageCore0 || 0) + '%';
                var el1 = document.getElementById('tmCpuCore1');
                if (el1) el1.textContent = tmCal ? '...' : Math.round(data.cpu.usageCore1 || 0) + '%';
                var elt = document.getElementById('tmCpuTotal');
                if (elt) elt.textContent = tmCal ? '...' : Math.round(data.cpu.usageTotal || 0) + '%';
            }
            if (data.tasks) {
                var tc = document.getElementById('taskCount');
                if (tc) tc.textContent = data.tasks.count || 0;
                var avgUs = data.tasks.loopAvgUs || 0;
                var la = document.getElementById('loopTimeAvg');
                if (la) la.textContent = avgUs + ' us';
                var lm = document.getElementById('loopTimeMax');
                if (lm) lm.textContent = (data.tasks.loopMaxUs || 0) + ' us';
                var lf = document.getElementById('tmLoopFreq');
                if (lf) lf.textContent = avgUs > 0 ? Math.round(1000000 / avgUs) + ' Hz' : '-- Hz';

                if (data.tasks.list) {
                    _taskData = data.tasks.list;
                    renderTaskTable();
                }
            } else {
                var tbody = document.getElementById('taskTableBody');
                if (tbody && !tbody.dataset.populated) {
                    tc = document.getElementById('taskCount');
                    if (tc) tc.textContent = 'Disabled';
                    la = document.getElementById('loopTimeAvg');
                    if (la) la.textContent = '-';
                    lm = document.getElementById('loopTimeMax');
                    if (lm) lm.textContent = '-';
                    lf = document.getElementById('tmLoopFreq');
                    if (lf) lf.textContent = '-';
                    el0 = document.getElementById('tmCpuCore0');
                    if (el0) el0.textContent = '-';
                    el1 = document.getElementById('tmCpuCore1');
                    if (el1) el1.textContent = '-';
                    elt = document.getElementById('tmCpuTotal');
                    if (elt) elt.textContent = '--%';
                    tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;opacity:0.5">Task monitor disabled</td></tr>';
                }
            }

            // Uptime
            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
            }

            // Reset Reason
            if (data.resetReason) {
                document.getElementById('resetReason').textContent = formatResetReason(data.resetReason);
            }

            // DSP CPU Load
            var hasDspData = (data.pipelineCpu !== undefined || data.cpuLoadPercent !== undefined);
            var dspSection = document.getElementById('dsp-cpu-section');
            if (hasDspData) {
                if (dspSection) dspSection.style.display = '';

                // Per-input DSP CPU (from dspMetrics cpuLoadPercent)
                var dspInputEl = document.getElementById('dsp-cpu-input');
                if (dspInputEl && data.cpuLoadPercent !== undefined) {
                    dspInputEl.textContent = data.cpuLoadPercent.toFixed(1) + '%';
                    dspInputEl.style.color = data.cpuLoadPercent >= 95 ? 'var(--error-color)' :
                                             data.cpuLoadPercent >= 80 ? 'var(--warning-color)' : '';
                }

                // Pipeline total CPU
                var dspPipelineEl = document.getElementById('dsp-cpu-pipeline');
                if (dspPipelineEl && data.pipelineCpu !== undefined) {
                    dspPipelineEl.textContent = data.pipelineCpu.toFixed(1) + '%';
                    dspPipelineEl.style.color = data.pipelineCpu >= 95 ? 'var(--error-color)' :
                                                data.pipelineCpu >= 80 ? 'var(--warning-color)' : '';
                }

                // Frame time breakdown
                var dspFrameEl = document.getElementById('dsp-frame-us');
                if (dspFrameEl && data.pipelineFrameUs !== undefined) {
                    dspFrameEl.textContent = data.pipelineFrameUs + ' µs';
                }

                var dspMatrixEl = document.getElementById('dsp-matrix-us');
                if (dspMatrixEl && data.matrixUs !== undefined) {
                    dspMatrixEl.textContent = data.matrixUs + ' µs';
                }

                var dspOutputEl = document.getElementById('dsp-output-us');
                if (dspOutputEl && data.outputDspUs !== undefined) {
                    dspOutputEl.textContent = data.outputDspUs + ' µs';
                }

                // FIR bypass warning row
                var dspWarnRow = document.getElementById('dsp-cpu-warn-row');
                var dspFirEl = document.getElementById('dsp-fir-bypass');
                if (dspWarnRow && data.firBypassCount !== undefined) {
                    dspWarnRow.style.display = data.firBypassCount > 0 ? '' : 'none';
                    if (dspFirEl) dspFirEl.textContent = data.firBypassCount;
                }
            }

            // Add to history
            addHistoryDataPoint(data);

            // Pin table — populate from firmware data (once)
            if (data.pins) {
                var ptb = document.getElementById('pinTableBody');
                if (ptb && !ptb.dataset.populated) {
                    ptb.dataset.populated = '1';
                    var catLabels = {audio:'Audio', display:'Display', input:'Input', core:'Core', network:'Network'};
                    var html = '';
                    for (i = 0; i < data.pins.length; i++) {
                        var p = data.pins[i];
                        var catName = catLabels[p.c] || p.c;
                        html += '<tr><td>' + p.g + '</td><td>' + p.f + '</td><td>' + p.d + '</td><td><span class="pin-cat pin-cat-' + p.c + '">' + catName + '</span></td></tr>';
                    }
                    ptb.innerHTML = html;
                }
            }

            // Update PSRAM health banner on Health Dashboard
            if (data.psramFallbackCount !== undefined || data.psramWarning !== undefined || data.psramCritical !== undefined) {
                updatePsramHealthBanner(data);
            }
        }

        var psramBudgetOpen = false;
        function togglePsramBudget() {
            psramBudgetOpen = !psramBudgetOpen;
            var content = document.getElementById('psramBudgetContent');
            var chevron = document.getElementById('psramBudgetChevron');
            var header = document.getElementById('psramBudgetHeader');
            if (content) content.classList.toggle('open', psramBudgetOpen);
            if (chevron) chevron.parentElement.parentElement.classList.toggle('open', psramBudgetOpen);
            if (header) header.classList.toggle('open', psramBudgetOpen);
        }

        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1048576).toFixed(1) + ' MB';
        }

        // ===== Task Table Sorting =====
        var _taskData = [];
        var _taskSortCol = 3; // default sort by priority
        var _taskSortAsc = false; // descending by default

        function sortTaskTable(col) {
            if (_taskSortCol === col) {
                _taskSortAsc = !_taskSortAsc;
            } else {
                _taskSortCol = col;
                _taskSortAsc = (col === 0); // name ascending by default, others descending
            }
            renderTaskTable();
        }

        function renderTaskTable() {
            var tbody = document.getElementById('taskTableBody');
            if (!tbody || !_taskData.length) return;
            tbody.dataset.populated = '1';

            var stateNames = ['Run', 'Rdy', 'Blk', 'Sus', 'Del'];
            // Build enriched rows for sorting
            var rows = [], i;
            for (i = 0; i < _taskData.length; i++) {
                var t = _taskData[i];
                var stackFree = t.stackFree || 0;
                var stackAlloc = t.stackAlloc || 0;
                var usedPct = (stackAlloc > 0) ? Math.round((1 - stackFree / stackAlloc) * 100) : -1;
                rows.push({ name: t.name, stackFree: stackFree, stackAlloc: stackAlloc, usedPct: usedPct, pri: t.pri, state: t.state, core: t.core });
            }

            // Sort
            var col = _taskSortCol;
            var asc = _taskSortAsc;
            rows.sort(function(a, b) {
                var va, vb;
                switch (col) {
                    case 0: va = a.name.toLowerCase(); vb = b.name.toLowerCase(); return asc ? (va < vb ? -1 : va > vb ? 1 : 0) : (vb < va ? -1 : vb > va ? 1 : 0);
                    case 1: va = a.stackAlloc > 0 ? a.stackFree : 0; vb = b.stackAlloc > 0 ? b.stackFree : 0; break;
                    case 2: va = a.usedPct; vb = b.usedPct; break;
                    case 3: va = a.pri; vb = b.pri; break;
                    case 4: va = a.state; vb = b.state; break;
                    case 5: va = a.core; vb = b.core; break;
                    default: va = 0; vb = 0;
                }
                return asc ? va - vb : vb - va;
            });

            // Update sort arrows
            var ths = document.querySelectorAll('#taskTable thead th .sort-arrow');
            for (i = 0; i < ths.length; i++) {
                ths[i].className = 'sort-arrow' + (i === col ? (asc ? ' asc' : ' desc') : '');
            }

            // Render rows
            var html = '';
            for (i = 0; i < rows.length; i++) {
                var r = rows[i];
                var stackStr = '', barHtml = '', pctHtml = '';
                if (r.stackAlloc > 0) {
                    var freePct = 100 - r.usedPct;
                    var barClass = freePct > 50 ? 'task-stack-ok' : freePct > 25 ? 'task-stack-warn' : 'task-stack-crit';
                    var pctClass = freePct > 50 ? 'task-pct-ok' : freePct > 25 ? 'task-pct-warn' : 'task-pct-crit';
                    stackStr = formatBytes(r.stackFree) + '/' + formatBytes(r.stackAlloc);
                    barHtml = ' <span class="task-stack-bar ' + barClass + '" style="width:' + Math.max(r.usedPct, 4) + 'px" title="' + r.usedPct + '% used"></span>';
                    pctHtml = '<span class="task-pct-text ' + pctClass + '">' + r.usedPct + '%</span>';
                } else {
                    stackStr = formatBytes(r.stackFree);
                    pctHtml = '<span class="task-pct-text" style="opacity:0.4">--</span>';
                }
                var stateName = r.state < stateNames.length ? stateNames[r.state] : '?';
                html += '<tr><td>' + r.name + '</td><td>' + stackStr + barHtml + '</td><td>' + pctHtml + '</td><td>' + r.pri + '</td><td>' + stateName + '</td><td>' + r.core + '</td></tr>';
            }
            tbody.innerHTML = html;
        }

        function formatUptime(ms) {
            const seconds = Math.floor(ms / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            const days = Math.floor(hours / 24);

            if (days > 0) return `${days}d ${hours % 24}h`;
            if (hours > 0) return `${hours}h ${minutes % 60}m`;
            if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
            return `${seconds}s`;
        }

        function formatResetReason(reason) {
            if (!reason) return '--';
            // The reset reason from the backend is already formatted as a readable string
            return reason;
        }

        function addHistoryDataPoint(data) {
            historyData.timestamps.push(Date.now());
            var c0 = data.cpu ? data.cpu.usageCore0 : 0;
            var c1 = data.cpu ? data.cpu.usageCore1 : 0;
            var ct = data.cpu ? data.cpu.usageTotal : 0;
            historyData.cpuTotal.push(ct >= 0 ? ct : 0);
            historyData.cpuCore0.push(c0 >= 0 ? c0 : 0);
            historyData.cpuCore1.push(c1 >= 0 ? c1 : 0);

            if (data.memory && data.memory.heapTotal > 0) {
                const memPercent = (1 - data.memory.heapFree / data.memory.heapTotal) * 100;
                historyData.memoryPercent.push(memPercent);
            } else {
                historyData.memoryPercent.push(0);
            }

            if (data.memory && data.memory.psramTotal > 0) {
                const psramPercent = (1 - data.memory.psramFree / data.memory.psramTotal) * 100;
                historyData.psramPercent.push(psramPercent);
            } else {
                historyData.psramPercent.push(0);
            }

            // Trim to max points
            while (historyData.timestamps.length > maxHistoryPoints) {
                historyData.timestamps.shift();
                historyData.cpuTotal.shift();
                historyData.cpuCore0.shift();
                historyData.cpuCore1.shift();
                historyData.memoryPercent.shift();
                historyData.psramPercent.shift();
            }

            // Always redraw graphs (they're always visible now)
            drawCpuGraph();
            drawMemoryGraph();
            drawPsramGraph();
        }

        function drawLineGraph(canvasId, lines, containerId) {
            const canvas = document.getElementById(canvasId);
            if (!canvas) return;

            if (containerId) {
                const container = document.getElementById(containerId);
                if (!container) return;
                const psramTotal = document.getElementById('psramTotal');
                if (!psramTotal || psramTotal.textContent === 'N/A') {
                    container.style.display = 'none';
                    return;
                }
                container.style.display = 'block';
            }

            const ctx = canvas.getContext('2d');
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * window.devicePixelRatio;
            canvas.height = rect.height * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

            const leftMargin = 35;
            const bottomMargin = 20;
            const w = rect.width - leftMargin;
            const h = rect.height - bottomMargin;

            ctx.fillStyle = '#1A1A1A';
            ctx.fillRect(0, 0, rect.width, rect.height);

            ctx.save();
            ctx.translate(leftMargin, 0);

            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            ctx.fillStyle = '#999';
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';

            for (let i = 0; i <= 4; i++) {
                const y = (h / 4) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
                ctx.fillText((100 - i * 25) + '%', -5, y);
            }

            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            [0, 0.5, 1].forEach((point, idx) => {
                ctx.fillText(['-60s', '-30s', 'now'][idx], w * point, h + 4);
            });

            if (lines[0].data.length < 2) {
                ctx.restore();
                return;
            }

            const step = w / (lines[0].data.length - 1);
            lines.forEach(line => {
                ctx.strokeStyle = line.color;
                ctx.lineWidth = line.width || 2;
                ctx.beginPath();
                line.data.forEach((val, i) => {
                    const x = i * step;
                    const y = h - (val / 100) * h;
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                });
                ctx.stroke();
            });

            ctx.restore();
        }

        function drawCpuGraph() {
            drawLineGraph('cpuGraph', [
                { data: historyData.cpuCore0, color: '#FFB74D', width: 1.5 },
                { data: historyData.cpuCore1, color: '#F57C00', width: 1.5 },
                { data: historyData.cpuTotal, color: '#FF9800', width: 2 }
            ]);
        }

        function drawMemoryGraph() {
            drawLineGraph('memoryGraph', [
                { data: historyData.memoryPercent, color: '#2196F3' }
            ]);
        }

        function drawPsramGraph() {
            drawLineGraph('psramGraph', [
                { data: historyData.psramPercent, color: '#9C27B0' }
            ], 'psramGraphContainer');
        }

        function eepromProgram() {
            var rates = document.getElementById('eepromRates').value.split(',').map(Number).filter(function(n){return n>0;});
            var flags = { independentClock: document.getElementById('eepromFlagClock').checked,
                          hwVolume: document.getElementById('eepromFlagVol').checked,
                          filters: document.getElementById('eepromFlagFilter').checked };
            var payload = {
                address: parseInt(document.getElementById('eepromTargetAddr').value),
                deviceId: parseInt(document.getElementById('eepromDeviceId').value),
                deviceName: document.getElementById('eepromDeviceName').value,
                manufacturer: document.getElementById('eepromManufacturer').value,
                hwRevision: parseInt(document.getElementById('eepromHwRev').value) || 1,
                maxChannels: parseInt(document.getElementById('eepromMaxCh').value) || 2,
                dacI2cAddress: parseInt(document.getElementById('eepromDacAddr').value),
                flags: flags, sampleRates: rates
            };
            apiFetch('/api/dac/eeprom', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(payload) })
            .then(function(r){ return r.safeJson(); })
            .then(function(d){ if (d.success) showToast('EEPROM programmed successfully','success'); else showToast(d.message||'Program failed','error'); })
            .catch(function(){ showToast('EEPROM program failed','error'); });
        }

        function eepromErase() {
            if (!confirm('Erase EEPROM? This will clear all stored DAC identification data.')) return;
            var addr = parseInt(document.getElementById('eepromTargetAddr').value);
            apiFetch('/api/dac/eeprom/erase', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({ address: addr }) })
            .then(function(r){ return r.safeJson(); })
            .then(function(d){ if (d.success) showToast('EEPROM erased','success'); else showToast(d.message||'Erase failed','error'); })
            .catch(function(){ showToast('EEPROM erase failed','error'); });
        }

        function eepromScan() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'eepromScan' }));
                showToast('Scanning I2C bus...', 'info');
            }
        }
        function eepromFillPreset() {
            var sel = document.getElementById('eepromPreset');
            if (!sel || !sel.value) return;
            var p = JSON.parse(sel.value);
            document.getElementById('eepromDeviceId').value = '0x' + p.deviceId.toString(16).padStart(4,'0');
            document.getElementById('eepromDeviceName').value = p.deviceName || '';
            document.getElementById('eepromManufacturer').value = p.manufacturer || '';
            document.getElementById('eepromMaxCh').value = p.maxChannels || 2;
            document.getElementById('eepromDacAddr').value = p.dacI2cAddress ? '0x' + p.dacI2cAddress.toString(16).padStart(2,'0') : '0x00';
            document.getElementById('eepromFlagClock').checked = !!(p.flags & 1);
            document.getElementById('eepromFlagVol').checked = !!(p.flags & 2);
            document.getElementById('eepromFlagFilter').checked = !!(p.flags & 4);
            document.getElementById('eepromRates').value = (p.sampleRates || []).join(',');
        }
        function eepromLoadHex() {
            apiFetch('/api/dac/eeprom')
            .then(r => r.safeJson())
            .then(d => {
                var el = document.getElementById('dbgEepromHex');
                if (!el) return;
                if (d.rawHex) {
                    var hex = d.rawHex;
                    var lines = [];
                    for (var i = 0; i < hex.length; i += 32) {
                        var addr = (i/2).toString(16).padStart(4,'0').toUpperCase();
                        var row = hex.substring(i, i+32).match(/.{2}/g).join(' ');
                        lines.push(addr + ': ' + row);
                    }
                    el.textContent = lines.join('\n');
                    el.style.display = '';
                } else {
                    el.textContent = 'No EEPROM data available';
                    el.style.display = '';
                }
            })
            .catch(function(){
                var el = document.getElementById('dbgEepromHex');
                if (el) { el.textContent = 'Failed to load'; el.style.display = ''; }
            });
        }
