// ===== MQTT Settings =====

function loadMqttSettings() {
    apiFetch('/api/mqtt')
    .then(res => res.safeJson())
    .then(data => {
        document.getElementById('appState.mqttEnabled').checked = data.enabled || false;
        document.getElementById('mqttFields').style.display = (data.enabled || false) ? '' : 'none';
        document.getElementById('appState.mqttBroker').value = data.broker || '';
        document.getElementById('appState.mqttPort').value = data.port || 1883;
        document.getElementById('appState.mqttUsername').value = data.username || '';
        document.getElementById('appState.mqttPassword').value = '';
        document.getElementById('appState.mqttPassword').placeholder = data.hasPassword
            ? 'Enter password (leave empty to keep current)'
            : 'Password';
        document.getElementById('appState.mqttBaseTopic').value = data.baseTopic || '';
        document.getElementById('appState.mqttBaseTopic').placeholder = data.defaultBaseTopic || 'ALX/device-serial';
        document.getElementById('mqttDefaultTopic').textContent = data.defaultBaseTopic || 'ALX/{serial}';
        document.getElementById('appState.mqttHADiscovery').checked = data.haDiscovery || false;
        document.getElementById('appState.mqttUseTls').checked = data.useTls || false;
        document.getElementById('mqttTlsFields').style.display = (data.useTls || false) ? '' : 'none';
        document.getElementById('appState.mqttVerifyCert').checked = data.verifyCert || false;
        updateMqttConnectionStatus(data.connected, data.broker, data.port, data.effectiveBaseTopic);
    })
    .catch(err => console.error('Failed to load MQTT settings:', err));
}

function updateMqttConnectionStatus(connected, broker, port, baseTopic) {
    const statusBox = document.getElementById('mqttStatusBox');
    const enabled = document.getElementById('appState.mqttEnabled').checked;

    let html = '';
    if (connected) {
        html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-success">Connected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Unknown'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                    ${broker ? `<div class="info-row"><span class="info-label">TLS</span><span class="info-value">${document.getElementById('appState.mqttUseTls').checked ? 'Enabled' : 'Off'}</span></div>` : ''}
                `;
        currentMqttConnected = true;
    } else if (enabled) {
        html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-error">Disconnected</span></div>
                    <div class="info-row"><span class="info-label">Broker</span><span class="info-value">${broker || 'Not configured'}</span></div>
                    <div class="info-row"><span class="info-label">Port</span><span class="info-value">${port || 1883}</span></div>
                `;
        currentMqttConnected = false;
    } else {
        html = `
                    <div class="info-row"><span class="info-label">Status</span><span class="info-value text-secondary">Disabled</span></div>
                    <div class="info-row"><span class="info-label">MQTT</span><span class="info-value">Not enabled</span></div>
                `;
        currentMqttConnected = null;
    }
    statusBox.innerHTML = html;
    // Update status bar
    updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, ws && ws.readyState === WebSocket.OPEN);
}

function toggleMqttEnabled() {
    const enabled = document.getElementById('appState.mqttEnabled').checked;
    document.getElementById('mqttFields').style.display = enabled ? '' : 'none';
    apiFetch('/api/mqtt', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: enabled })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            showToast(enabled ? 'MQTT enabled' : 'MQTT disabled', 'success');
            setTimeout(loadMqttSettings, 1000);
        } else {
            showToast(data.message || 'Failed to toggle MQTT', 'error');
            // Revert toggle on failure
            document.getElementById('appState.mqttEnabled').checked = !enabled;
        }
    })
    .catch(err => {
        showToast('Failed to toggle MQTT', 'error');
        document.getElementById('appState.mqttEnabled').checked = !enabled;
    });
}

function toggleMqttTls() {
    const useTls = document.getElementById('appState.mqttUseTls').checked;
    document.getElementById('mqttTlsFields').style.display = useTls ? '' : 'none';
    // Auto-suggest TLS port
    const portInput = document.getElementById('appState.mqttPort');
    if (useTls && portInput.value === '1883') {
        portInput.value = '8883';
    } else if (!useTls && portInput.value === '8883') {
        portInput.value = '1883';
    }
}

function saveMqttSettings() {
    const settings = {
        broker: document.getElementById('appState.mqttBroker').value,
        port: parseInt(document.getElementById('appState.mqttPort').value) || 1883,
        username: document.getElementById('appState.mqttUsername').value,
        password: document.getElementById('appState.mqttPassword').value,
        baseTopic: document.getElementById('appState.mqttBaseTopic').value,
        haDiscovery: document.getElementById('appState.mqttHADiscovery').checked,
        useTls: document.getElementById('appState.mqttUseTls').checked,
        verifyCert: document.getElementById('appState.mqttVerifyCert').checked
    };

    apiFetch('/api/mqtt', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            showToast('MQTT settings saved', 'success');
            setTimeout(loadMqttSettings, 2000);
        } else {
            showToast(data.message || 'Failed to save', 'error');
        }
    })
    .catch(err => showToast('Failed to save MQTT settings', 'error'));
}

// ===== WiFi Management Functions =====
function toggleAutoAP() {
    const enabled = document.getElementById('autoAPToggle').checked;
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ autoAPEnabled: enabled })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast(enabled ? 'Auto AP enabled' : 'Auto AP disabled', 'success');
    })
    .catch(err => showToast('Failed to update setting', 'error'));
}
