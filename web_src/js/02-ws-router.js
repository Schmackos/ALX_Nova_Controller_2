        let _wsLimitWarned = false;

        function validateWsMessage(data, requiredFields) {
            if (!data || typeof data !== 'object') return false;
            for (var i = 0; i < requiredFields.length; i++) {
                if (data[requiredFields[i]] === undefined) {
                    console.warn('[WS] Missing field "' + requiredFields[i] + '" in ' + (data.type || 'unknown') + ' message');
                    return false;
                }
            }
            return true;
        }

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
                if (adc < numInputLanes && buf.byteLength >= 258) {
                    const samples = new Uint8Array(buf, 2, 256);
                    waveformTarget[adc] = samples;
                    if (!waveformCurrent[adc]) waveformCurrent[adc] = new Uint8Array(samples);
                    startAudioAnimation();
                }
            } else if (type === 0x02) {
                // Spectrum: [type:1][adc:1][freq:f32LE][bands:16xf32LE]
                if (adc < numInputLanes && buf.byteLength >= 70) {
                    const freq = dv.getFloat32(2, true);
                    for (let i = 0; i < 16; i++) spectrumTarget[adc][i] = dv.getFloat32(6 + i * 4, true);
                    targetDominantFreq[adc] = freq;
                    if (currentActiveTab === 'audio') startAudioAnimation();
                }
            }
        }

        // ===== WS Message Router =====
        function routeWsMessage(data) {
            if (data.type === 'authRequired') {
                // Server requesting authentication — fetch one-time token
                apiFetch('/api/ws-token').then(r => r.json()).then(d => {
                    if (d.success && d.token) {
                        ws.send(JSON.stringify({ type: 'auth', token: d.token }));
                    } else {
                        showToast('Connection failed: token error', 'error');
                    }
                }).catch(() => {
                    showToast('Connection failed: auth error', 'error');
                });
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

                // Re-subscribe to audio stream if audio tab is active
                if (audioSubscribed && currentActiveTab === 'audio') {
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
                if (!validateWsMessage(data, ['connected'])) return;
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
                appendDebugLog(data.timestamp, data.message, data.level, data.module);
            } else if (data.type === 'hardware_stats') {
                if (!validateWsMessage(data, ['cpu'])) return;
                updateHardwareStats(data);
                if (data.wsClientCount >= data.wsClientMax - 1 && !_wsLimitWarned) {
                    showToast('WebSocket client limit nearly reached (' + data.wsClientCount + '/' + data.wsClientMax + ')', 'warning');
                    _wsLimitWarned = true;
                }
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
                if (data.useTls !== undefined) {
                    document.getElementById('appState.mqttUseTls').checked = data.useTls;
                    document.getElementById('mqttTlsFields').style.display = data.useTls ? '' : 'none';
                }
                if (data.verifyCert !== undefined) {
                    document.getElementById('appState.mqttVerifyCert').checked = data.verifyCert;
                }
                updateMqttConnectionStatus(data.connected, data.broker, data.port, data.baseTopic);
            } else if (data.type === 'audioLevels') {
                if (!validateWsMessage(data, ['adc'])) return;
                if (currentActiveTab === 'audio') {
                    if (data.numAdcsDetected !== undefined) {
                        numAdcsDetected = data.numAdcsDetected;
                    }
                    // Per-ADC VU/peak data
                    if (data.adc && Array.isArray(data.adc)) {
                        for (let a = 0; a < data.adc.length && a < numInputLanes; a++) {
                            const ad = data.adc[a];
                            vuTargetArr[a][0] = ad.vu1 !== undefined ? ad.vu1 : 0;
                            vuTargetArr[a][1] = ad.vu2 !== undefined ? ad.vu2 : 0;
                            peakTargetArr[a][0] = ad.peak1 !== undefined ? ad.peak1 : 0;
                            peakTargetArr[a][1] = ad.peak2 !== undefined ? ad.peak2 : 0;
                        }
                    }
                    vuDetected = data.signalDetected !== undefined ? data.signalDetected : false;
                    startVuAnimation();
                }
                // Feed new Audio tab VU meters
                audioTabUpdateLevels(data);
            } else if (data.type === 'inputNames') {
                if (data.names && Array.isArray(data.names)) {
                    for (let i = 0; i < data.names.length && i < numInputLanes * 2; i++) {
                        inputNames[i] = data.names[i];
                    }
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
            } else if (data.type === 'dacState') {
                if (data.eeprom) handleEepromDiag(data.eeprom);
            } else if (data.type === 'eepromProgramResult') {
                showToast(data.success ? 'EEPROM programmed' : 'EEPROM program failed', data.success ? 'success' : 'error');
            } else if (data.type === 'eepromEraseResult') {
                showToast(data.success ? 'EEPROM erased' : 'EEPROM erase failed', data.success ? 'success' : 'error');
            } else if (data.type === 'i2sPortState') {
                handleI2sPortState(data);
            } else if (data.type === 'halDeviceState') {
                handleHalDeviceState(data);
                if (data.unknownDevices) handleHalUnknownDevices(data.unknownDevices);
            } else if (data.type === 'audioChannelMap') {
                handleAudioChannelMap(data);
            } else if (data.type === 'diagEvent') {
                handleDiagEvent(data);
            }
        }
