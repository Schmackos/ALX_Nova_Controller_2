        // Binary WS message handler (waveform + spectrum)
        var _binDiag = { wf: 0, sp: 0, last: 0 };
        function handleBinaryMessage(buf) {
            const dv = new DataView(buf);
            const type = dv.getUint8(0);
            const adc = dv.getUint8(1);
            if (type === 0x01) _binDiag.wf++;
            else if (type === 0x02) _binDiag.sp++;
            var now = Date.now();
            if (now - _binDiag.last > 2000) {
                console.log('[BIN diag] wf=' + _binDiag.wf + ' sp=' + _binDiag.sp + ' tab=' + currentActiveTab + ' bytes=' + buf.byteLength);
                _binDiag.wf = 0; _binDiag.sp = 0; _binDiag.last = now;
            }
            if (type === 0x01 && currentActiveTab === 'audio') {
                // Waveform: [type:1][adc:1][samples:256]
                if (adc < NUM_ADCS && buf.byteLength >= 258) {
                    const samples = new Uint8Array(buf, 2, 256);
                    waveformTarget[adc] = samples;
                    if (!waveformCurrent[adc]) waveformCurrent[adc] = new Uint8Array(samples);
                    startAudioAnimation();
                }
            } else if (type === 0x02) {
                // Spectrum: [type:1][adc:1][freq:f32LE][bands:16xf32LE]
                if (adc < NUM_ADCS && buf.byteLength >= 70) {
                    const freq = dv.getFloat32(2, true);
                    for (let i = 0; i < 16; i++) spectrumTarget[adc][i] = dv.getFloat32(6 + i * 4, true);
                    targetDominantFreq[adc] = freq;
                    if (currentActiveTab === 'audio') startAudioAnimation();
                    // Feed RTA overlay for DSP tab
                    if (currentActiveTab === 'dsp' && adc === 0) {
                        peqRtaData = new Float32Array(16);
                        for (let i = 0; i < 16; i++) peqRtaData[i] = spectrumTarget[0][i];
                        if (peqGraphLayers.rta) dspDrawFreqResponse();
                    }
                }
            }
        }

        // ===== WS Message Router =====
        function routeWsMessage(data) {
            if (data.type === 'authRequired') {
                // Server requesting authentication
                const sessionId = getSessionIdFromCookie();
                if (sessionId) {
                    ws.send(JSON.stringify({
                        type: 'auth',
                        sessionId: sessionId
                    }));
                } else {
                    console.error('No session ID for WebSocket auth');
                    // Do not redirect automatically to avoid loops
                    showToast('Connection failed: No Session ID', 'error');
                }
            }
            else if (data.type === 'authSuccess') {
                console.log('WebSocket authenticated');
                updateConnectionStatus(true);
                wsReconnectDelay = WS_MIN_RECONNECT_DELAY;
                fetchUpdateStatus();

                // Draw initial graphs
                drawCpuGraph();
                drawMemoryGraph();
                drawPsramGraph();

                // Re-subscribe to audio stream if audio tab or DSP RTA is active
                if (audioSubscribed && currentActiveTab === 'audio') {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                } else if (currentActiveTab === 'dsp' && peqGraphLayers.rta) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                }

                // Show reconnection notification if we were disconnected during an update
                if (wasDisconnectedDuringUpdate) {
                    showToast('Device is back online after update!', 'success');
                    wasDisconnectedDuringUpdate = false;
                } else if (hadPreviousConnection) {
                    // Show general reconnection notification
                    showToast('Device reconnected', 'success');
                }
                hadPreviousConnection = true;
            }
            else if (data.type === 'authFailed') {
                console.error('WebSocket auth failed:', data.error);
                ws.close();
                showToast('Session invalid. Redirecting to login...', 'error');
                setTimeout(() => {
                    window.location.href = '/login';
                }, 2000);
            }
            else if (data.type === 'wifiStatus') {
                updateWiFiStatus(data);
            } else if (data.type === 'updateStatus') {
                handleUpdateStatus(data);
            } else if (data.type === 'smartSensing') {
                updateSmartSensingUI(data);
            } else if (data.type === 'factoryResetProgress') {
                handlePhysicalResetProgress(data);
            } else if (data.type === 'factoryResetStatus') {
                // Handle factory reset status messages
                if (data.status === 'complete') {
                    showToast(data.message || 'Factory reset complete', 'success');
                } else if (data.status === 'rebooting') {
                    showToast(data.message || 'Rebooting device...', 'info');
                }
            } else if (data.type === 'rebootProgress') {
                handlePhysicalRebootProgress(data);
            } else if (data.type === 'debugLog') {
                appendDebugLog(data.timestamp, data.message, data.level);
            } else if (data.type === 'hardware_stats') {
                updateHardwareStats(data);
            } else if (data.type === 'justUpdated') {
                showUpdateSuccessNotification(data);
            } else if (data.type === 'displayState') {
                if (typeof data.backlightOn !== 'undefined') {
                    backlightOn = !!data.backlightOn;
                    document.getElementById('backlightToggle').checked = backlightOn;
                }
                if (typeof data.screenTimeout !== 'undefined') {
                    screenTimeoutSec = data.screenTimeout;
                    document.getElementById('screenTimeoutSelect').value = screenTimeoutSec.toString();
                }
                if (typeof data.backlightBrightness !== 'undefined') {
                    backlightBrightness = data.backlightBrightness;
                    var pct = Math.round(backlightBrightness * 100 / 255);
                    var options = [10, 25, 50, 75, 100];
                    var closest = options.reduce(function(a, b) { return Math.abs(b - pct) < Math.abs(a - pct) ? b : a; });
                    document.getElementById('brightnessSelect').value = closest;
                }
                if (typeof data.dimEnabled !== 'undefined') {
                    dimEnabled = !!data.dimEnabled;
                    document.getElementById('dimToggle').checked = dimEnabled;
                    updateDimVisibility();
                }
                if (typeof data.dimTimeout !== 'undefined') {
                    dimTimeoutSec = data.dimTimeout;
                    document.getElementById('dimTimeoutSelect').value = dimTimeoutSec.toString();
                }
                if (typeof data.dimBrightness !== 'undefined') {
                    dimBrightnessPwm = data.dimBrightness;
                    document.getElementById('dimBrightnessSelect').value = dimBrightnessPwm.toString();
                }
            } else if (data.type === 'buzzerState') {
                if (typeof data.enabled !== 'undefined') {
                    document.getElementById('buzzerToggle').checked = !!data.enabled;
                    document.getElementById('buzzerFields').style.display = data.enabled ? '' : 'none';
                }
                if (typeof data.volume !== 'undefined') {
                    document.getElementById('buzzerVolumeSelect').value = data.volume.toString();
                }
            } else if (data.type === 'mqttSettings') {
                document.getElementById('appState.mqttEnabled').checked = data.enabled || false;
                document.getElementById('mqttFields').style.display = (data.enabled || false) ? '' : 'none';
                document.getElementById('appState.mqttBroker').value = data.broker || '';
                document.getElementById('appState.mqttPort').value = data.port || 1883;
                document.getElementById('appState.mqttUsername').value = data.username || '';
                document.getElementById('appState.mqttBaseTopic').value = data.baseTopic || '';
                document.getElementById('appState.mqttHADiscovery').checked = data.haDiscovery || false;
                updateMqttConnectionStatus(data.connected, data.broker, data.port, data.baseTopic);
            } else if (data.type === 'audioLevels') {
                if (currentActiveTab === 'audio') {
                    if (data.numAdcsDetected !== undefined) {
                        numAdcsDetected = data.numAdcsDetected;
                        updateAdc2Visibility();
                    }
                    // Per-ADC VU/peak data
                    if (data.adc && Array.isArray(data.adc)) {
                        for (let a = 0; a < data.adc.length && a < NUM_ADCS; a++) {
                            const ad = data.adc[a];
                            vuTargetArr[a][0] = ad.vu1 !== undefined ? ad.vu1 : 0;
                            vuTargetArr[a][1] = ad.vu2 !== undefined ? ad.vu2 : 0;
                            peakTargetArr[a][0] = ad.peak1 !== undefined ? ad.peak1 : 0;
                            peakTargetArr[a][1] = ad.peak2 !== undefined ? ad.peak2 : 0;
                        }
                    }
                    // Per-ADC status badges and readouts
                    if (data.adcStatus && Array.isArray(data.adcStatus)) {
                        for (let a = 0; a < data.adcStatus.length && a < NUM_ADCS; a++) {
                            updateAdcStatusBadge(a, data.adcStatus[a]);
                        }
                    }
                    if (data.adc && Array.isArray(data.adc)) {
                        for (let a = 0; a < data.adc.length && a < NUM_ADCS; a++) {
                            const ad = data.adc[a];
                            updateAdcReadout(a, ad.dBFS, ad.vrms1, ad.vrms2);
                        }
                    }
                    vuDetected = data.signalDetected !== undefined ? data.signalDetected : false;
                    startVuAnimation();
                }
            } else if (data.type === 'inputNames') {
                if (data.names && Array.isArray(data.names)) {
                    for (let i = 0; i < data.names.length && i < NUM_ADCS * 2; i++) {
                        inputNames[i] = data.names[i];
                    }
                    applyInputNames();
                    loadInputNameFields();
                }
            } else if (data.type === 'audioGraphState') {
                var vuT = document.getElementById('vuMeterEnabledToggle');
                var wfT = document.getElementById('waveformEnabledToggle');
                var spT = document.getElementById('spectrumEnabledToggle');
                if (vuT) vuT.checked = data.vuMeterEnabled;
                if (wfT) wfT.checked = data.waveformEnabled;
                if (spT) spT.checked = data.spectrumEnabled;
                var fftSel = document.getElementById('fftWindowSelect');
                if (fftSel && data.fftWindowType !== undefined) fftSel.value = data.fftWindowType;
                toggleGraphDisabled('vuMeterContent', !data.vuMeterEnabled);
                toggleGraphDisabled('waveformContent', !data.waveformEnabled);
                toggleGraphDisabled('spectrumContent', !data.spectrumEnabled);
            } else if (data.type === 'debugState') {
                applyDebugState(data);
            } else if (data.type === 'signalGenerator') {
                applySigGenState(data);
            } else if (data.type === 'adcState') {
                if (Array.isArray(data.enabled)) {
                    for (var ai = 0; ai < data.enabled.length; ai++) {
                        var laneCb = document.getElementById('laneEnable' + ai);
                        if (laneCb) laneCb.checked = !!data.enabled[ai];
                        overviewApplyAdcEnabled(ai, !!data.enabled[ai]);
                    }
                }
            } else if (data.type === 'usbAudioState') {
                handleUsbAudioState(data);
            } else if (data.type === 'dacState') {
                handleDacState(data);
                if (data.eeprom) handleEepromDiag(data.eeprom);
            } else if (data.type === 'eepromProgramResult') {
                showToast(data.success ? 'EEPROM programmed' : 'EEPROM program failed', data.success ? 'success' : 'error');
            } else if (data.type === 'eepromEraseResult') {
                showToast(data.success ? 'EEPROM erased' : 'EEPROM erase failed', data.success ? 'success' : 'error');
            } else if (data.type === 'dspState') {
                dspHandleState(data);
            } else if (data.type === 'dspMetrics') {
                dspHandleMetrics(data);
            } else if (data.type === 'peqPresets') {
                peqHandlePresetsList(data.presets);
            } else if (data.type === 'thdResult') {
                thdUpdateResult(data);
            }
        }
