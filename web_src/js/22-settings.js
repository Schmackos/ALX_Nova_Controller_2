// ===== Settings / Theme Functions =====

        function handlePhysicalResetProgress(data) {
            showToast('Factory reset: ' + data.progress + '%', 'info');
        }

        function handlePhysicalRebootProgress(data) {
            showToast('Rebooting: ' + data.progress + '%', 'info');
        }

        function updateSmartAutoSettingsVisibility(mode) {
            const settingsCard = document.getElementById('smartAutoSettingsCard');
            if (settingsCard) {
                settingsCard.style.display = (mode === 'smart_auto') ? 'block' : 'none';
            }
        }

        function updateSmartSensingUI(data) {
            if (data.mode !== undefined) {
                let modeValue = data.mode;
                if (typeof data.mode === 'number') {
                    const modeMap = { 0: 'always_on', 1: 'always_off', 2: 'smart_auto' };
                    modeValue = modeMap[data.mode] || 'smart_auto';
                }
                document.querySelectorAll('input[name="sensingMode"]').forEach(function(radio) {
                    radio.checked = (radio.value === modeValue);
                });
                updateSmartAutoSettingsVisibility(modeValue);
                const modeLabels = { 'always_on': 'Always On', 'always_off': 'Always Off', 'smart_auto': 'Smart Auto' };
                const modeEl = document.getElementById('infoSensingMode');
                if (modeEl) modeEl.textContent = modeLabels[modeValue] || modeValue;
            }
            if (data.timerDuration !== undefined && !inputFocusState.timerDuration) {
                const el = document.getElementById('appState.timerDuration');
                if (el) el.value = data.timerDuration;
            }
            if (data.timerDuration !== undefined) {
                const el = document.getElementById('infoTimerDuration');
                if (el) el.textContent = data.timerDuration + ' min';
            }
            if (data.audioThreshold !== undefined && !inputFocusState.audioThreshold) {
                const el = document.getElementById('audioThreshold');
                if (el) el.value = Math.round(data.audioThreshold);
            }
            if (data.audioThreshold !== undefined) {
                const el = document.getElementById('infoAudioThreshold');
                if (el) el.textContent = Math.round(data.audioThreshold) + ' dBFS';
            }
            if (data.amplifierState !== undefined) {
                const display = document.getElementById('amplifierDisplay');
                const status = document.getElementById('amplifierStatus');
                if (display) display.classList.toggle('on', data.amplifierState);
                if (status) status.textContent = data.amplifierState ? 'ON' : 'OFF';
                currentAmpState = data.amplifierState;
                updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);
            }
            if (data.signalDetected !== undefined) {
                const el = document.getElementById('signalDetected');
                if (el) el.textContent = data.signalDetected ? 'Yes' : 'No';
            }
            if (data.audioLevel !== undefined) {
                const el = document.getElementById('audioLevel');
                if (el) el.textContent = data.audioLevel.toFixed(1) + ' dBFS';
            }
            // audioVrms now sourced from hardwareStats.audio.adcs[0].vrms (see 24-hardware-stats.js)
            const timerDisplay = document.getElementById('timerDisplay');
            const timerValue = document.getElementById('timerValue');
            if (timerDisplay && timerValue) {
                if (data.timerActive && data.timerRemaining !== undefined) {
                    timerDisplay.classList.remove('hidden');
                    const mins = Math.floor(data.timerRemaining / 60);
                    const secs = data.timerRemaining % 60;
                    timerValue.textContent = mins.toString().padStart(2, '0') + ':' + secs.toString().padStart(2, '0');
                } else {
                    timerDisplay.classList.add('hidden');
                }
            }
            if (data.audioSampleRate !== undefined) {
                const sel = document.getElementById('audioSampleRateSelect');
                if (sel) sel.value = data.audioSampleRate.toString();
            }
        }

function toggleTheme() {
    darkMode = document.getElementById('darkModeToggle').checked;
    applyTheme(darkMode);
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 'appState.darkMode': darkMode })
    });
}

function applyTheme(isDarkMode) {
    if (isDarkMode) {
        document.body.classList.add('night-mode');
        document.querySelector('meta[name="theme-color"]').setAttribute('content', '#121212');
    } else {
        document.body.classList.remove('night-mode');
        document.querySelector('meta[name="theme-color"]').setAttribute('content', '#F5F5F5');
    }
    localStorage.setItem('darkMode', isDarkMode ? 'true' : 'false');
    invalidateBgCache();
}

function toggleBacklight() {
    backlightOn = document.getElementById('backlightToggle').checked;
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setBacklight', enabled: backlightOn }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ backlightOn })
    });
}

function setBrightness() {
    var pct = parseInt(document.getElementById('brightnessSelect').value);
    var pwm = Math.round(pct * 255 / 100);
    backlightBrightness = pwm;
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setBrightness', value: pwm }));
    }
}

function setScreenTimeout() {
    screenTimeoutSec = parseInt(document.getElementById('screenTimeoutSelect').value);
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setScreenTimeout', value: screenTimeoutSec }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ screenTimeout: screenTimeoutSec })
    });
}

function updateDimVisibility() {
    var show = dimEnabled ? '' : 'none';
    document.getElementById('dimTimeoutRow').style.display = show;
    document.getElementById('dimBrightnessRow').style.display = show;
}

function toggleDim() {
    dimEnabled = document.getElementById('dimToggle').checked;
    updateDimVisibility();
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setDimEnabled', enabled: dimEnabled }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dimEnabled: dimEnabled })
    });
}

function setDimTimeout() {
    dimTimeoutSec = parseInt(document.getElementById('dimTimeoutSelect').value);
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setDimTimeout', value: dimTimeoutSec }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dimTimeout: dimTimeoutSec })
    });
}

function setDimBrightness() {
    dimBrightnessPwm = parseInt(document.getElementById('dimBrightnessSelect').value);
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setDimBrightness', value: dimBrightnessPwm }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dimBrightness: dimBrightnessPwm })
    });
}

function setBootAnimation() {
    var val = parseInt(document.getElementById('bootAnimSelect').value);
    var payload = {};
    if (val < 0) {
        payload.bootAnimEnabled = false;
    } else {
        payload.bootAnimEnabled = true;
        payload.bootAnimStyle = val;
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });
}

function toggleBuzzer() {
    var enabled = document.getElementById('buzzerToggle').checked;
    document.getElementById('buzzerFields').style.display = enabled ? '' : 'none';
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setBuzzerEnabled', enabled: enabled }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ buzzerEnabled: enabled })
    });
}

function setBuzzerVolume() {
    var vol = parseInt(document.getElementById('buzzerVolumeSelect').value);
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'setBuzzerVolume', value: vol }));
    }
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ buzzerVolume: vol })
    });
}

function toggleAutoUpdate() {
    autoUpdateEnabled = document.getElementById('autoUpdateToggle').checked;
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ autoUpdateEnabled: autoUpdateEnabled })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast(autoUpdateEnabled ? 'Auto-update enabled' : 'Auto-update disabled', 'success');
    })
    .catch(err => showToast('Failed to update setting', 'error'));
}

function toggleCertValidation() {
    enableCertValidation = document.getElementById('certValidationToggle').checked;
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 'appState.enableCertValidation': enableCertValidation })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast(enableCertValidation ? 'SSL validation enabled' : 'SSL validation disabled', 'success');
    })
    .catch(err => showToast('Failed to update setting', 'error'));
}

function setStatsInterval() {
    const interval = parseInt(document.getElementById('statsIntervalSelect').value);
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hardwareStatsInterval: interval })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast('Stats interval set to ' + interval + 's', 'success');
    })
    .catch(err => showToast('Failed to update interval', 'error'));
}

function setFftWindow(val) {
    if (ws && ws.readyState === WebSocket.OPEN)
        ws.send(JSON.stringify({type:'setFftWindowType', value:parseInt(val)}));
}
function setDebugToggle(type, enabled) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({type: type, enabled: enabled}));
    }
}

function setDebugSerialLevel(level) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({type: 'setDebugSerialLevel', level: parseInt(level)}));
    }
}

function applyDebugState(d) {
    var modeT = document.getElementById('debugModeToggle');
    var hwT = document.getElementById('debugHwStatsToggle');
    var i2sT = document.getElementById('debugI2sMetricsToggle');
    var tmT = document.getElementById('debugTaskMonitorToggle');
    var lvl = document.getElementById('debugSerialLevel');
    if (modeT) modeT.checked = d.debugMode;
    if (hwT) hwT.checked = d.debugHwStats;
    if (i2sT) i2sT.checked = d.debugI2sMetrics;
    if (tmT) tmT.checked = d.debugTaskMonitor;
    if (lvl) lvl.value = d.debugSerialLevel;

    // Hide/show I2S and Task sections when disabled
    var i2sSec = document.getElementById('i2sMetricsSection');
    if (i2sSec) i2sSec.style.display = (d.debugMode && d.debugI2sMetrics) ? '' : 'none';
    var tmSec = document.getElementById('taskMonitorSection');
    if (tmSec) tmSec.style.display = (d.debugMode && d.debugTaskMonitor) ? '' : 'none';

    // Hide/show hardware stats sections when HW Stats disabled
    var hwSec = document.getElementById('hwStatsSection');
    if (hwSec) hwSec.style.display = (d.debugMode && d.debugHwStats) ? '' : 'none';

    // Hide/show debug tab based on debugMode only
    updateDebugTabVisibility(d.debugMode);

    // Populate pin table if included (sent once on connect)
    if (d.pins) {
        var ptb = document.getElementById('pinTableBody');
        if (ptb && !ptb.dataset.populated) {
            ptb.dataset.populated = '1';
            var catLabels = {audio:'Audio', display:'Display', input:'Input', core:'Core', network:'Network'};
            var html = '';
            for (var i = 0; i < d.pins.length; i++) {
                var p = d.pins[i];
                var catName = catLabels[p.c] || p.c;
                html += '<tr><td>' + p.g + '</td><td>' + p.f + '</td><td>' + p.d + '</td><td><span class="pin-cat pin-cat-' + p.c + '">' + catName + '</span></td></tr>';
            }
            ptb.innerHTML = html;
        }
    }
}

function updateDebugTabVisibility(visible) {
    // Sidebar item
    var sideItem = document.querySelector('.sidebar-item[data-tab="debug"]');
    if (sideItem) sideItem.style.display = visible ? '' : 'none';
    // Mobile tab button
    var tabBtn = document.querySelector('.tab[data-tab="debug"]');
    if (tabBtn) tabBtn.style.display = visible ? '' : 'none';
    // If currently on debug tab and hiding, switch to settings
    if (!visible) {
        var debugPanel = document.getElementById('debug');
        if (debugPanel && debugPanel.classList.contains('active')) {
            switchTab('settings');
        }
    }
}

function setAudioUpdateRate() {
    const rate = parseInt(document.getElementById('audioUpdateRateSelect').value);
    updateLerpFactors(rate);
    const labels = {100:'100 ms',50:'50 ms',33:'33 ms',20:'20 ms'};
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ audioUpdateRate: rate })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast('Audio rate set to ' + (labels[rate]||rate+'ms'), 'success');
    })
    .catch(err => showToast('Failed to update audio rate', 'error'));
}

var IMPORT_SECTIONS = [
    { key: 'wifi',            label: 'WiFi Configuration' },
    { key: 'general',         label: 'General Settings' },
    { key: 'smartSensing',    label: 'Smart Sensing' },
    { key: 'mqtt',            label: 'MQTT Configuration' },
    { key: 'signalGenerator', label: 'Signal Generator' },
    { key: 'inputChannelNames', label: 'Input Channel Names' },
    { key: 'hal',             label: 'HAL Device Configs',     tag: 'v2.0' },
    { key: 'customDevices',   label: 'Custom Device Schemas',  tag: 'v2.0' },
    { key: 'dsp',             label: 'DSP Settings',           tag: 'v2.0' },
    { key: 'pipeline',        label: 'Pipeline Audio Routing', tag: 'v2.0' }
];

var _pendingImportData = null;

function exportSettings() {
    apiFetch('/api/settings/export')
    .then(function(res) { return res.safeJson(); })
    .then(function(data) {
        var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = 'alx-settings.json';
        a.click();
        URL.revokeObjectURL(url);
        showToast('Settings exported', 'success');
    })
    .catch(function() { showToast('Failed to export settings', 'error'); });
}

function handleFileSelect(event) {
    var file = event.target.files[0];
    if (!file) return;

    var reader = new FileReader();
    reader.onload = function(e) {
        try {
            var settings = JSON.parse(e.target.result);
            showImportPreview(settings, file.name);
        } catch (err) {
            showToast('Invalid settings file', 'error');
        }
    };
    reader.readAsText(file);
    // Reset input so same file can be re-selected
    event.target.value = '';
}

function showImportPreview(settings, fileName) {
    _pendingImportData = settings;

    var card = document.getElementById('importPreviewCard');
    var metaEl = document.getElementById('importPreviewMeta');
    var sectionsEl = document.getElementById('importPreviewSections');

    // Build metadata display
    var metaHtml = '';
    metaHtml += '<span class="meta-label">File</span><span class="meta-value">' + escapeHtml(fileName) + '</span>';
    if (settings.exportVersion) {
        metaHtml += '<span class="meta-label">Format</span><span class="meta-value">v' + escapeHtml(String(settings.exportVersion)) + '</span>';
    }
    if (settings.exportTimestamp) {
        var ts = settings.exportTimestamp;
        var dateStr = (typeof ts === 'number') ? new Date(ts * 1000).toLocaleString() : escapeHtml(String(ts));
        metaHtml += '<span class="meta-label">Exported</span><span class="meta-value">' + dateStr + '</span>';
    }
    if (settings.firmwareVersion) {
        metaHtml += '<span class="meta-label">Firmware</span><span class="meta-value">' + escapeHtml(String(settings.firmwareVersion)) + '</span>';
    }
    if (settings.deviceSerial) {
        metaHtml += '<span class="meta-label">Device</span><span class="meta-value">' + escapeHtml(String(settings.deviceSerial)) + '</span>';
    }
    metaEl.innerHTML = metaHtml;

    // Build section checklist
    var checkSvg = '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M12 2C6.5 2 2 6.5 2 12S6.5 22 12 22 22 17.5 22 12 17.5 2 12 2M10 17L5 12L6.41 10.59L10 14.17L17.59 6.58L19 8L10 17Z"/></svg>';
    var missSvg = '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4Z"/></svg>';

    var sectionsHtml = '';
    for (var i = 0; i < IMPORT_SECTIONS.length; i++) {
        var sec = IMPORT_SECTIONS[i];
        var present = settings.hasOwnProperty(sec.key);
        var cls = present ? 'present' : 'missing';
        var icon = present ? checkSvg : missSvg;
        var tagHtml = sec.tag ? ' <span class="section-tag">' + escapeHtml(sec.tag) + '</span>' : '';
        sectionsHtml += '<div class="import-section-item ' + cls + '">' + icon + '<span>' + escapeHtml(sec.label) + tagHtml + '</span></div>';
    }
    sectionsEl.innerHTML = sectionsHtml;

    card.style.display = '';
    card.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function cancelImportPreview() {
    _pendingImportData = null;
    var card = document.getElementById('importPreviewCard');
    if (card) card.style.display = 'none';
}

function applyImportPreview() {
    if (!_pendingImportData) return;
    var settings = _pendingImportData;
    _pendingImportData = null;

    var card = document.getElementById('importPreviewCard');
    if (card) card.style.display = 'none';

    importSettings(settings);
}

function importSettings(settings) {
    apiFetch('/api/settings/import', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(function(res) { return res.safeJson(); })
    .then(function(data) {
        if (data.success) {
            showToast('Settings imported. Rebooting...', 'success');
        } else {
            showToast(data.message || 'Import failed', 'error');
        }
    })
    .catch(function() { showToast('Failed to import settings', 'error'); });
}

function manualOverride(state) {
    apiFetch('/api/smartsensing', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ manualOverride: state })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast(state ? 'Turned ON' : 'Turned OFF', 'success');
    })
    .catch(err => showToast('Failed to control amplifier', 'error'));
}

function toggleSmartAutoSettings() {
    smartAutoSettingsCollapsed = !smartAutoSettingsCollapsed;
    const content = document.getElementById('smartAutoContent');
    const chevron = document.getElementById('smartAutoChevron');
    const header = chevron.parentElement;

    if (smartAutoSettingsCollapsed) {
        content.classList.remove('open');
        header.classList.remove('open');
    } else {
        content.classList.add('open');
        header.classList.add('open');
    }
}

function updateAudioThreshold() {
    const value = parseFloat(document.getElementById('audioThreshold').value);
    if (isNaN(value) || value < -96 || value > 0) return;

    apiFetch('/api/smartsensing', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ audioThreshold: value })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast('Threshold updated', 'success');
    })
    .catch(err => showToast('Failed to update threshold', 'error'));
}

function updateSensingMode() {
    const selected = document.querySelector('input[name="sensingMode"]:checked');
    if (!selected) return;

    // Show/hide Smart Auto Settings card based on mode
    updateSmartAutoSettingsVisibility(selected.value);

    apiFetch('/api/smartsensing', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: selected.value })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast('Mode updated', 'success');
    })
    .catch(err => showToast('Failed to update mode', 'error'));
}

function updateTimerDuration() {
    const value = parseInt(document.getElementById('appState.timerDuration').value);
    if (isNaN(value) || value < 1 || value > 60) return;

    apiFetch('/api/smartsensing', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ timerDuration: value })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast('Timer updated', 'success');
    })
    .catch(err => showToast('Failed to update timer', 'error'));
}

function confirmDestructiveAction(title, message, onConfirm) {
    var overlay = document.createElement('div');
    overlay.className = 'confirm-overlay';
    var dialog = document.createElement('div');
    dialog.className = 'confirm-dialog';

    var iconSvg = '<svg viewBox="0 0 24 24" width="24" height="24" fill="var(--warning, #FFC107)" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg>';

    dialog.innerHTML =
        '<div class="confirm-header">' + iconSvg + ' ' + title + '</div>' +
        '<div class="confirm-body">' + message + '</div>' +
        '<div class="confirm-actions">' +
            '<button class="btn confirm-cancel-btn" id="confirmCancelBtn">Cancel</button>' +
            '<button class="btn confirm-confirm-btn" id="confirmConfirmBtn" disabled>Confirm (3)</button>' +
        '</div>';

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    var confirmBtn = document.getElementById('confirmConfirmBtn');
    var cancelBtn = document.getElementById('confirmCancelBtn');
    var countdown = 3;
    var timer = setInterval(function() {
        countdown--;
        if (countdown > 0) {
            confirmBtn.textContent = 'Confirm (' + countdown + ')';
        } else {
            clearInterval(timer);
            confirmBtn.textContent = 'Confirm';
            confirmBtn.disabled = false;
        }
    }, 1000);

    cancelBtn.onclick = function() {
        clearInterval(timer);
        document.body.removeChild(overlay);
    };

    confirmBtn.onclick = function() {
        clearInterval(timer);
        document.body.removeChild(overlay);
        onConfirm();
    };

    overlay.onclick = function(e) {
        if (e.target === overlay) {
            clearInterval(timer);
            document.body.removeChild(overlay);
        }
    };
}

function startFactoryReset() {
    confirmDestructiveAction(
        'Factory Reset',
        'This will erase all settings and restore defaults. This cannot be undone.',
        function() {
            apiFetch('/api/factoryreset', { method: 'POST' })
            .then(res => res.safeJson())
            .then(data => {
                if (data.success) showToast('Factory reset in progress...', 'success');
            })
            .catch(err => showToast('Failed to reset', 'error'));
        }
    );
}

function startReboot() {
    confirmDestructiveAction(
        'Reboot Device',
        'The device will restart. You will lose connection temporarily.',
        function() {
            apiFetch('/api/reboot', { method: 'POST' })
            .then(res => res.safeJson())
            .then(data => {
                if (data.success) showToast('Rebooting...', 'success');
            })
            .catch(err => showToast('Failed to reboot', 'error'));
        }
    );
}
