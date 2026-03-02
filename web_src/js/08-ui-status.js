        // ===== Status Bar Updates =====
        function updateStatusBar(wifiConnected, mqttConnected, ampState, wsConnected) {
            // WiFi status
            const wifiIndicator = document.getElementById('statusWifi');
            const wifiText = document.getElementById('statusWifiText');
            if (wifiConnected) {
                wifiIndicator.className = 'status-indicator online';
                wifiText.textContent = 'WiFi';
            } else {
                wifiIndicator.className = 'status-indicator offline';
                wifiText.textContent = 'WiFi';
            }

            // MQTT status
            const mqttIndicator = document.getElementById('statusMqtt');
            const mqttText = document.getElementById('statusMqttText');
            if (mqttConnected) {
                mqttIndicator.className = 'status-indicator online';
                mqttText.textContent = 'MQTT';
            } else if (mqttConnected === false) {
                mqttIndicator.className = 'status-indicator offline';
                mqttText.textContent = 'MQTT';
            } else {
                mqttIndicator.className = 'status-indicator';
                mqttText.textContent = 'MQTT';
            }

            // Amplifier status
            const ampIndicator = document.getElementById('statusAmp');
            const ampText = document.getElementById('statusAmpText');
            if (ampState) {
                ampIndicator.className = 'status-indicator online';
                ampText.textContent = 'Amp ON';
            } else {
                ampIndicator.className = 'status-indicator';
                ampText.textContent = 'Amp OFF';
            }

            // WebSocket status
            const wsIndicator = document.getElementById('statusWs');
            if (wsConnected) {
                wsIndicator.className = 'status-indicator online';
            } else {
                wsIndicator.className = 'status-indicator offline';
            }
        }

        // ===== LED Control =====
        function updateLED() {
            const led = document.getElementById('led');
            const status = document.getElementById('ledStatus');
            if (ledState) {
                led.classList.remove('off');
                led.classList.add('on');
                status.textContent = 'LED is ON';
            } else {
                led.classList.remove('on');
                led.classList.add('off');
                status.textContent = 'LED is OFF';
            }
        }

        function updateBlinkButton() {
            const btn = document.getElementById('toggleBtn');
            const state = document.getElementById('blinkingState');
            if (blinkingEnabled) {
                btn.textContent = 'Stop Blinking';
                btn.classList.remove('btn-primary');
                btn.classList.add('btn-danger');
                state.textContent = 'ON';
            } else {
                btn.textContent = 'Start Blinking';
                btn.classList.remove('btn-danger');
                btn.classList.add('btn-primary');
                state.textContent = 'OFF';
            }
        }

        function toggleBlinking() {
            blinkingEnabled = !blinkingEnabled;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'toggle', enabled: blinkingEnabled }));
            }
            updateBlinkButton();
        }

        function toggleLedMode() {
            ledBarMode = document.getElementById('ledModeToggle').checked;
            localStorage.setItem('ledBarMode', ledBarMode.toString());
        }

        // ===== WiFi Status =====
        function updateWiFiStatus(data) {
            const statusBox = document.getElementById('wifiStatusBox');
            const apToggle = document.getElementById('apToggle');
            const autoUpdateToggle = document.getElementById('autoUpdateToggle');

            // Store AP SSID for pre-filling the config modal
            if (data.apSSID) {
                currentAPSSID = data.apSSID;
            } else if (data.serialNumber) {
                // Fallback to serial number if appState.apSSID not provided
                currentAPSSID = data.serialNumber;
            }

            // Track current WiFi connection state and SSID
            currentWifiConnected = data.connected || false;
            currentWifiSSID = data.connected ? (data.ssid || '') : '';

            let html = '';

            // Client (STA) Status
            if (data.connected) {
                const ipType = data.usingStaticIP ? 'Static IP' : 'DHCP';
                html += `
                    <div class="info-row"><span class="info-label">Client Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Network</span><span class="info-value">${data.ssid || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Client IP</span><span class="info-value">${data.staIP || data.ip || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">IP Configuration</span><span class="info-value">${ipType}</span></div>
                    <div class="info-row"><span class="info-label">Signal</span><span class="info-value">${formatRssi(data.rssi)}</span></div>
                    <div class="info-row"><span class="info-label">Saved Networks</span><span class="info-value">${data.networkCount || 0}</span></div>
                `;
            } else {
                html += `
                    <div class="info-row"><span class="info-label">Client Status</span><span class="info-value text-error">Not Connected</span></div>
                `;
            }

            // AP Details separator if both are relevant
            let apContentAdded = false;
            if (data.mode === 'ap' || data.apEnabled) {
                if (html !== '') html += '<div class="divider"></div>';

                html += `
                    <div class="info-row"><span class="info-label">AP Mode</span><span class="info-value text-warning">Active</span></div>
                    <div class="info-row"><span class="info-label">AP SSID</span><span class="info-value">${data.apSSID || 'ALX-Device'}</span></div>
                    <div class="info-row"><span class="info-label">AP IP</span><span class="info-value">${data.apIP || data.ip || '192.168.4.1'}</span></div>
                `;

                if (data.apClients !== undefined) {
                    html += `<div class="info-row"><span class="info-label">Clients Connected</span><span class="info-value">${data.apClients}</span></div>`;
                }
                apContentAdded = true;
            }

            // Common details - only add divider if AP content was shown or if we have any content
            if (html !== '' && apContentAdded) {
                html += `<div class="divider"></div>`;
            }
            html += `<div class="info-row"><span class="info-label">MAC Address</span><span class="info-value">${data.mac || 'Unknown'}</span></div>`;

            apToggle.checked = data.apEnabled || (data.mode === 'ap');
            document.getElementById('apFields').style.display = apToggle.checked ? '' : 'none';
            statusBox.innerHTML = html;

            if (typeof data.autoUpdateEnabled !== 'undefined') {
                autoUpdateEnabled = !!data.autoUpdateEnabled;
                autoUpdateToggle.checked = autoUpdateEnabled;
            }

            if (typeof data.autoAPEnabled !== 'undefined') {
                document.getElementById('autoAPToggle').checked = !!data.autoAPEnabled;
            }

            if (typeof data.timezoneOffset !== 'undefined') {
                currentTimezoneOffset = data.timezoneOffset;
                document.getElementById('timezoneSelect').value = data.timezoneOffset.toString();
                updateTimezoneDisplay(data.timezoneOffset, data.dstOffset || 0);
            }

            if (typeof data.dstOffset !== 'undefined') {
                currentDstOffset = data.dstOffset;
                document.getElementById('dstToggle').checked = (data.dstOffset === 3600);
            }

            if (typeof data.darkMode !== 'undefined') {
                darkMode = !!data.darkMode;
                document.getElementById('darkModeToggle').checked = darkMode;
                applyTheme(darkMode);
            }

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

            if (typeof data.bootAnimEnabled !== 'undefined') {
                if (!data.bootAnimEnabled) {
                    document.getElementById('bootAnimSelect').value = '-1';
                } else if (typeof data.bootAnimStyle !== 'undefined') {
                    document.getElementById('bootAnimSelect').value = data.bootAnimStyle.toString();
                }
            }

            if (typeof data.buzzerEnabled !== 'undefined') {
                document.getElementById('buzzerToggle').checked = !!data.buzzerEnabled;
                document.getElementById('buzzerFields').style.display = data.buzzerEnabled ? '' : 'none';
            }
            if (typeof data.buzzerVolume !== 'undefined') {
                document.getElementById('buzzerVolumeSelect').value = data.buzzerVolume.toString();
            }

            if (typeof data.enableCertValidation !== 'undefined') {
                enableCertValidation = !!data.enableCertValidation;
                document.getElementById('certValidationToggle').checked = enableCertValidation;
            }

            if (typeof data.hardwareStatsInterval !== 'undefined') {
                document.getElementById('statsIntervalSelect').value = data.hardwareStatsInterval.toString();
            }

            if (typeof data.audioUpdateRate !== 'undefined') {
                document.getElementById('audioUpdateRateSelect').value = data.audioUpdateRate.toString();
                updateLerpFactors(data.audioUpdateRate);
            }

            if (data.firmwareVersion) {
                currentFirmwareVersion = data.firmwareVersion;
                document.getElementById('currentVersion').textContent = data.firmwareVersion;
            }

            // Always show latest version row if we have any version info
            if (data.latestVersion) {
                currentLatestVersion = data.latestVersion;
                const latestVersionEl = document.getElementById('latestVersion');
                const latestVersionRow = document.getElementById('latestVersionRow');
                const latestVersionNotes = document.getElementById('latestVersionNotes');

                latestVersionRow.style.display = 'flex';

                // If up-to-date, show green "Up-To-Date" text and hide release notes link
                if (!data.updateAvailable && data.latestVersion !== 'Checking...' && data.latestVersion !== 'Unknown') {
                    latestVersionEl.textContent = 'Up-To-Date, no newer version available';
                    latestVersionEl.style.opacity = '1';
                    latestVersionEl.style.fontStyle = 'normal';
                    latestVersionEl.style.color = 'var(--success)';
                    latestVersionNotes.style.display = 'none';
                } else {
                    latestVersionEl.textContent = data.latestVersion;
                    latestVersionNotes.style.display = '';

                    // Style based on status
                    if (data.latestVersion === 'Checking...') {
                        latestVersionEl.style.opacity = '0.6';
                        latestVersionEl.style.fontStyle = 'italic';
                        latestVersionEl.style.color = '';
                    } else if (data.latestVersion === 'Unknown') {
                        latestVersionEl.style.opacity = '0.6';
                        latestVersionEl.style.fontStyle = 'italic';
                        latestVersionEl.style.color = 'var(--text-secondary)';
                    } else {
                        latestVersionEl.style.opacity = '1';
                        latestVersionEl.style.fontStyle = 'normal';
                        latestVersionEl.style.color = '';
                    }
                }

                if (data.updateAvailable) {
                    document.getElementById('updateBtn').classList.remove('hidden');
                } else {
                    document.getElementById('updateBtn').classList.add('hidden');
                }
            }

            // Update device serial number in Debug tab
            if (data.serialNumber) {
                document.getElementById('deviceSerial').textContent = data.serialNumber;
                // Also update sidebar version
                const sidebarVer = document.getElementById('sidebarVersion');
                if (sidebarVer && data.firmwareVersion) {
                    sidebarVer.textContent = 'v' + data.firmwareVersion;
                }
            }

            // Update MAC address in Debug tab
            if (data.mac) {
                document.getElementById('deviceMac').textContent = data.mac;
            }

            // Pre-fill WiFi SSID with currently connected network
            if (data.ssid && data.connected) {
                document.getElementById('appState.wifiSSID').value = data.ssid;
            }

            // Update global WiFi status for status bar
            currentWifiConnected = data.connected || data.mode === 'ap';
            updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);

            // Refresh saved networks list
            loadSavedNetworks();
        }

        // ===== Timezone / Time Functions =====
        function updateTimezone() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            const dstOffset = document.getElementById('dstToggle').checked ? 3600 : 0;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezoneOffset: offset, dstOffset: dstOffset })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('Timezone updated', 'success');
                    currentTimezoneOffset = offset;
                    currentDstOffset = dstOffset;
                    updateTimezoneDisplay(offset, dstOffset);
                    // Wait a moment for NTP sync then refresh time
                    setTimeout(updateCurrentTime, 2000);
                }
            })
            .catch(err => showToast('Failed to update timezone', 'error'));
        }

        function updateDST() {
            const offset = parseInt(document.getElementById('timezoneSelect').value);
            const dstOffset = document.getElementById('dstToggle').checked ? 3600 : 0;
            apiFetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timezoneOffset: offset, dstOffset: dstOffset })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    showToast('DST setting updated', 'success');
                    currentTimezoneOffset = offset;
                    currentDstOffset = dstOffset;
                    updateTimezoneDisplay(offset, dstOffset);
                    // Wait a moment for NTP sync then refresh time
                    setTimeout(updateCurrentTime, 2000);
                }
            })
            .catch(err => showToast('Failed to update DST setting', 'error'));
        }

        function updateTimezoneDisplay(offset, dstOffset = 0) {
            const totalOffset = offset + dstOffset;
            const hours = totalOffset / 3600;
            const sign = hours >= 0 ? '+' : '';
            const baseHours = offset / 3600;
            const baseSign = baseHours >= 0 ? '+' : '';

            let displayText = `UTC${sign}${hours} hours (GMT${baseSign}${baseHours}`;
            if (dstOffset !== 0) {
                displayText += ' + DST)';
            } else {
                displayText += ')';
            }

            document.getElementById('timezoneInfo').textContent = displayText;
            updateCurrentTime();
        }

        function updateCurrentTime() {
            apiFetch('/api/settings')
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    // Create date object with current UTC time
                    const now = new Date();
                    // Apply timezone and DST offsets
                    const offset = (data.timezoneOffset || 0) + (data.dstOffset || 0);
                    const localTime = new Date(now.getTime() + offset * 1000);

                    // Format time
                    const year = localTime.getUTCFullYear();
                    const month = String(localTime.getUTCMonth() + 1).padStart(2, '0');
                    const day = String(localTime.getUTCDate()).padStart(2, '0');
                    const hours = String(localTime.getUTCHours()).padStart(2, '0');
                    const minutes = String(localTime.getUTCMinutes()).padStart(2, '0');
                    const seconds = String(localTime.getUTCSeconds()).padStart(2, '0');

                    document.getElementById('currentTimeDisplay').textContent =
                        `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
                }
            })
            .catch(err => {
                document.getElementById('currentTimeDisplay').textContent = 'Error loading time';
            });
        }

        // Update time every second when on settings tab
        function startTimeUpdates() {
            if (!timeUpdateInterval) {
                updateCurrentTime();
                timeUpdateInterval = setInterval(updateCurrentTime, 1000);
            }
        }

        function stopTimeUpdates() {
            if (timeUpdateInterval) {
                clearInterval(timeUpdateInterval);
                timeUpdateInterval = null;
            }
        }
