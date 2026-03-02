// ===== Settings / Theme Functions =====

function toggleTheme() {
    darkMode = document.getElementById('darkModeToggle').checked;
    applyTheme(darkMode);
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ darkMode: darkMode })
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
    .then(res => res.json())
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
        body: JSON.stringify({ enableCertValidation: enableCertValidation })
    })
    .then(res => res.json())
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
    .then(res => res.json())
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
    .then(res => res.json())
    .then(data => {
        if (data.success) showToast('Audio rate set to ' + (labels[rate]||rate+'ms'), 'success');
    })
    .catch(err => showToast('Failed to update audio rate', 'error'));
}

function exportSettings() {
    apiFetch('/api/settings/export')
    .then(res => res.json())
    .then(data => {
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'alx-settings.json';
        a.click();
        URL.revokeObjectURL(url);
        showToast('Settings exported', 'success');
    })
    .catch(err => showToast('Failed to export settings', 'error'));
}

function handleFileSelect(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            const settings = JSON.parse(e.target.result);
            importSettings(settings);
        } catch (err) {
            showToast('Invalid settings file', 'error');
        }
    };
    reader.readAsText(file);
}

function manualOverride(state) {
    apiFetch('/api/smartsensing', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ manualOverride: state })
    })
    .then(res => res.json())
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
    .then(res => res.json())
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
    .then(res => res.json())
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
    .then(res => res.json())
    .then(data => {
        if (data.success) showToast('Timer updated', 'success');
    })
    .catch(err => showToast('Failed to update timer', 'error'));
}

function startFactoryReset() {
    if (confirm('Are you sure? This will erase all settings!')) {
        apiFetch('/api/factoryreset', { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            if (data.success) showToast('Factory reset in progress...', 'success');
        })
        .catch(err => showToast('Failed to reset', 'error'));
    }
}

function startReboot() {
    if (confirm('Are you sure you want to reboot the device?')) {
        apiFetch('/api/reboot', { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            if (data.success) showToast('Rebooting...', 'success');
        })
        .catch(err => showToast('Failed to reboot', 'error'));
    }
}
