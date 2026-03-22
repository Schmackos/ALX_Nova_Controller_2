// ===== WiFi Network Variables =====
let wifiScanInProgress = false;
// Store saved networks data globally for config management
let savedNetworksData = [];
// Store original network config to detect changes
let originalNetworkConfig = {
    useStaticIP: false,
    staticIP: '',
    subnet: '',
    gateway: '',
    dns1: '',
    dns2: ''
};
let networkRemovalPollTimer = null;
// Track connection attempts for network change detection
let connectionPollAttempts = 0;
let lastKnownNewIP = '';

// ===== WiFi Functions =====
function submitWiFiConfig(event) {
    event.preventDefault();
    const ssid = document.getElementById('appState.wifiSSID').value;
    const password = document.getElementById('appState.wifiPassword').value;
    const useStaticIP = document.getElementById('useStaticIP').checked;

    // Build request body
    const requestBody = { ssid, password, useStaticIP };

    // Add static IP configuration if enabled
    if (useStaticIP) {
        requestBody.staticIP = document.getElementById('staticIP').value;
        requestBody.subnet = document.getElementById('subnet').value;
        requestBody.gateway = document.getElementById('gateway').value;
        requestBody.dns1 = document.getElementById('dns1').value;
        requestBody.dns2 = document.getElementById('dns2').value;

        // Validate IP addresses
        if (!isValidIP(requestBody.staticIP)) {
            showToast('Invalid IPv4 address', 'error');
            return;
        }
        if (!isValidIP(requestBody.subnet)) {
            showToast('Invalid network mask', 'error');
            return;
        }
        if (!isValidIP(requestBody.gateway)) {
            showToast('Invalid gateway address', 'error');
            return;
        }
        if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
            showToast('Invalid primary DNS address', 'error');
            return;
        }
        if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
            showToast('Invalid secondary DNS address', 'error');
            return;
        }
    }

    showWiFiModal(ssid);

    apiFetch('/api/wificonfig', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody)
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            // Start polling for connection status
            if (wifiConnectionPollTimer) clearInterval(wifiConnectionPollTimer);
            wifiConnectionPollTimer = setInterval(pollWiFiConnection, 2000);
        } else {
            updateWiFiConnectionStatus('error', data.message || 'Failed to initiate connection');
        }
    })
    .catch(err => updateWiFiConnectionStatus('error', 'Network error: ' + err.message));
}

function saveNetworkSettings(event) {
    event.preventDefault();
    const ssid = document.getElementById('appState.wifiSSID').value;
    const password = document.getElementById('appState.wifiPassword').value;
    const useStaticIP = document.getElementById('useStaticIP').checked;

    if (!ssid) {
        showToast('Please enter network SSID', 'error');
        return;
    }

    if (!password) {
        showToast('Please enter network password', 'error');
        return;
    }

    // Build request body
    const requestBody = { ssid, password, useStaticIP };

    // Add static IP configuration if enabled
    if (useStaticIP) {
        requestBody.staticIP = document.getElementById('staticIP').value;
        requestBody.subnet = document.getElementById('subnet').value;
        requestBody.gateway = document.getElementById('gateway').value;
        requestBody.dns1 = document.getElementById('dns1').value;
        requestBody.dns2 = document.getElementById('dns2').value;

        // Validate IP addresses
        if (!isValidIP(requestBody.staticIP)) {
            showToast('Invalid IPv4 address', 'error');
            return;
        }
        if (!isValidIP(requestBody.subnet)) {
            showToast('Invalid network mask', 'error');
            return;
        }
        if (!isValidIP(requestBody.gateway)) {
            showToast('Invalid gateway address', 'error');
            return;
        }
        if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
            showToast('Invalid primary DNS address', 'error');
            return;
        }
        if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
            showToast('Invalid secondary DNS address', 'error');
            return;
        }
    }

    // Call save-only endpoint
    apiFetch('/api/wifisave', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody)
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            showToast('Network saved successfully', 'success');

            // Clear the form
            document.getElementById('appState.wifiSSID').value = '';
            document.getElementById('appState.wifiPassword').value = '';
            document.getElementById('useStaticIP').checked = false;
            toggleStaticIPFields(); // Hide static IP fields

            // Clear static IP fields
            document.getElementById('staticIP').value = '';
            document.getElementById('subnet').value = '255.255.255.0';
            document.getElementById('gateway').value = '';
            document.getElementById('dns1').value = '';
            document.getElementById('dns2').value = '';

            // Reload saved networks list
            loadSavedNetworks();
        } else {
            showToast(data.message || 'Failed to save network', 'error');
        }
    })
    .catch(err => showToast('Network error: ' + err.message, 'error'));
}

function showWiFiModal(ssid) {
    // Remove existing modal if any
    const existing = document.getElementById('wifiConnectionModal');
    if (existing) existing.remove();

    const modal = document.createElement('div');
    modal.id = 'wifiConnectionModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title">Connecting to WiFi</div>
                    <div class="info-box">
                        <div style="text-align: center; padding: 20px;">
                            <div id="wifiLoader" class="animate-pulse" style="font-size: 40px; margin-bottom: 16px;">📶</div>
                            <div id="wifiStatusText">Connecting to <strong>${ssid}</strong>...</div>
                            <div id="wifiIPInfo" class="hidden" style="margin-top: 16px; font-family: monospace; font-size: 18px; color: var(--success);"></div>
                        </div>
                    </div>
                    <div id="wifiModalActions" class="modal-actions" style="margin-top: 16px;">
                        <button type="button" class="btn btn-secondary" onclick="closeWiFiModal()">Cancel</button>
                    </div>
                </div>
            `;
    document.body.appendChild(modal);
}

function updateWiFiConnectionStatus(type, message, ip) {
    const statusText = document.getElementById('wifiStatusText');
    const loader = document.getElementById('wifiLoader');
    const ipInfo = document.getElementById('wifiIPInfo');
    const actions = document.getElementById('wifiModalActions');

    if (!statusText) return; // Modal might be closed

    statusText.innerHTML = message;

    if (type === 'success') {
        loader.innerHTML = '<svg viewBox="0 0 24 24" width="32" height="32" fill="var(--success)" aria-hidden="true"><path d="M12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22A10,10 0 0,1 2,12A10,10 0 0,1 12,2M11,16.5L18,9.5L16.59,8.09L11,13.67L7.91,10.59L6.5,12L11,16.5Z"/></svg>';
        loader.classList.remove('animate-pulse');

        if (ip) {
            ipInfo.textContent = 'IP: ' + ip;
            ipInfo.classList.remove('hidden');

            actions.innerHTML = `
                        <button class="btn btn-success" onclick="window.location.href='http://${ip}'">Go to Dashboard</button>
                    `;
        } else {
             actions.innerHTML = `<button class="btn btn-secondary" onclick="closeWiFiModal()">Close</button>`;
        }
    } else if (type === 'error') {
        loader.innerHTML = '<svg viewBox="0 0 24 24" width="32" height="32" fill="var(--error)" aria-hidden="true"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2M15.59,7L12,10.59L8.41,7L7,8.41L10.59,12L7,15.59L8.41,17L12,13.41L15.59,17L17,15.59L13.41,12L17,8.41L15.59,7Z"/></svg>';
        loader.classList.remove('animate-pulse');
        actions.innerHTML = `<button class="btn btn-secondary" onclick="closeWiFiModal()">Close</button>`;
    }
}


function closeWiFiModal() {
    const modal = document.getElementById('wifiConnectionModal');
    if (modal) modal.remove();
    if (wifiConnectionPollTimer) {
        clearInterval(wifiConnectionPollTimer);
        wifiConnectionPollTimer = null;
    }
}

function pollWiFiConnection() {
    connectionPollAttempts++;

    apiFetch('/api/wifistatus')
        .then(res => res.safeJson())
        .then(data => {
            // Reset attempts on successful response
            connectionPollAttempts = 0;

            if (data.wifiConnecting) {
                // Still connecting, keep polling
                return;
            }

            // Stop polling
            if (wifiConnectionPollTimer) {
                clearInterval(wifiConnectionPollTimer);
                wifiConnectionPollTimer = null;
            }

            if (data.wifiConnectSuccess) {
                lastKnownNewIP = data.wifiNewIP || data.staIP || '';
                updateWiFiConnectionStatus('success', 'Connected successfully!', lastKnownNewIP);
            } else {
                const errorMsg = data.wifiConnectError || 'Failed to connect. Check credentials.';
                updateWiFiConnectionStatus('error', errorMsg);
            }
        })
        .catch(err => {
            console.log('Poll attempt ' + connectionPollAttempts + ' failed:', err.message);

            // If we've had multiple failed fetch attempts, the device likely changed networks
            if (connectionPollAttempts >= 3) {
                if (wifiConnectionPollTimer) {
                    clearInterval(wifiConnectionPollTimer);
                    wifiConnectionPollTimer = null;
                }

                // Check if we're on AP mode IP - device may have connected to WiFi
                const currentHost = window.location.hostname;
                if (currentHost === '192.168.4.1' || currentHost.startsWith('192.168.4.')) {
                    // We were on AP mode, device likely connected to new network
                    updateWiFiConnectionStatus('success',
                        'Connection successful! The device has connected to WiFi and is no longer reachable at this address. Please connect to your WiFi network and access the device at its new IP address.',
                        '');
                } else {
                    // Generic network error
                    updateWiFiConnectionStatus('error', 'Network error: Lost connection to device. The device may have changed networks.');
                }
                connectionPollAttempts = 0;
            }
            // Otherwise keep trying (device might be temporarily unreachable during network switch)
        });
}

function showAPModeModal(apIP) {
    const modal = document.createElement('div');
    modal.id = 'apModeModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title">AP Mode Activated</div>
                    <div class="info-box">
                        <div style="text-align: center; padding: 20px;">
                            <div style="font-size: 40px; margin-bottom: 16px;">📶</div>
                            <div style="margin-bottom: 8px;">No saved networks available.</div>
                            <div style="margin-bottom: 16px;">Access Point mode has been started.</div>
                            <div style="margin-top: 16px; font-family: monospace; font-size: 18px; color: var(--accent);">${apIP}</div>
                        </div>
                    </div>
                    <div class="modal-actions" style="margin-top: 16px;">
                        <button class="primary" onclick="window.location.href='http://${apIP}'">Go to Dashboard</button>
                    </div>
                </div>
            `;
    document.body.appendChild(modal);
}

function closeAPModeModal() {
    const modal = document.getElementById('apModeModal');
    if (modal) modal.remove();
}

// Toggle static IP fields visibility
function toggleStaticIPFields() {
    const useStaticIP = document.getElementById('useStaticIP').checked;
    const fields = document.getElementById('staticIPFields');
    fields.style.display = useStaticIP ? 'block' : 'none';
}

// Auto-populate gateway and DNS based on static IP
function updateStaticIPDefaults() {
    const staticIP = document.getElementById('staticIP').value;
    const gatewayField = document.getElementById('gateway');
    const dns1Field = document.getElementById('dns1');

    if (!staticIP || !isValidIP(staticIP)) return;

    // Extract network portion and suggest gateway (.1)
    const parts = staticIP.split('.');
    if (parts.length === 4) {
        const suggestedGateway = `${parts[0]}.${parts[1]}.${parts[2]}.1`;

        // Only auto-fill if fields are empty
        if (!gatewayField.value) {
            gatewayField.value = suggestedGateway;
        }
        if (!dns1Field.value) {
            dns1Field.value = suggestedGateway;
        }
    }
}

// Validate IP address format
function isValidIP(ip) {
    if (!ip) return false;
    const parts = ip.split('.');
    if (parts.length !== 4) return false;
    return parts.every(part => {
        const num = parseInt(part, 10);
        return num >= 0 && num <= 255 && part === num.toString();
    });
}

function scanWiFiNetworks() {
    const scanBtn = document.getElementById('scanBtn');
    const scanStatus = document.getElementById('scanStatus');
    const select = document.getElementById('wifiNetworkSelect');

    if (wifiScanInProgress) return;

    wifiScanInProgress = true;
    scanBtn.disabled = true;
    scanBtn.textContent = '⏳';
    scanStatus.textContent = 'Scanning for networks...';

    // Start scan and poll for results
    pollWiFiScan();
}

function pollWiFiScan() {
    apiFetch('/api/wifiscan')
        .then(res => res.safeJson())
        .then(data => {
            const scanBtn = document.getElementById('scanBtn');
            const scanStatus = document.getElementById('scanStatus');
            const select = document.getElementById('wifiNetworkSelect');

            if (data.scanning) {
                // Still scanning, poll again
                setTimeout(pollWiFiScan, 1000);
                return;
            }

            // Scan complete
            wifiScanInProgress = false;
            scanBtn.disabled = false;
            scanBtn.textContent = '🔍';

            // Clear and populate dropdown
            select.innerHTML = '<option value="">-- Select a network --</option>';

            if (data.networks && data.networks.length > 0) {
                // Sort by signal strength (strongest first)
                data.networks.sort((a, b) => b.rssi - a.rssi);

                data.networks.forEach(network => {
                    const option = document.createElement('option');
                    option.value = network.ssid;
                    // Show signal strength indicator
                    const signalIcon = network.rssi > -50 ? '📶' : network.rssi > -70 ? '📶' : '📶';
                    const lockIcon = network.encryption === 'secured' ? '🔒' : '🔓';
                    option.textContent = `${network.ssid} ${lockIcon} (${network.rssi} dBm)`;
                    select.appendChild(option);
                });
                scanStatus.textContent = `Found ${data.networks.length} network(s)`;
            } else {
                scanStatus.textContent = 'No networks found';
            }
        })
        .catch(err => {
            wifiScanInProgress = false;
            const scanBtn = document.getElementById('scanBtn');
            const scanStatus = document.getElementById('scanStatus');
            scanBtn.disabled = false;
            scanBtn.textContent = '🔍';
            scanStatus.textContent = 'Scan failed';
            showToast('Failed to scan networks', 'error');
        });
}

function onNetworkSelect() {
    const select = document.getElementById('wifiNetworkSelect');
    const ssidInput = document.getElementById('appState.wifiSSID');
    const passwordInput = document.getElementById('appState.wifiPassword');
    const useStaticIPCheckbox = document.getElementById('useStaticIP');
    const staticIPFields = document.getElementById('staticIPFields');

    if (select.value) {
        ssidInput.value = select.value;

        // Check if this is a saved network
        const savedNetwork = savedNetworksData.find(net => net.ssid === select.value);
        if (savedNetwork) {
            // Populate password field with placeholder
            passwordInput.value = '';
            passwordInput.placeholder = '••••••••';

            // Populate Static IP fields if configured
            if (savedNetwork.useStaticIP) {
                useStaticIPCheckbox.checked = true;
                staticIPFields.style.display = 'block';
                document.getElementById('staticIP').value = savedNetwork.staticIP || '';
                document.getElementById('subnet').value = savedNetwork.subnet || '255.255.255.0';
                document.getElementById('gateway').value = savedNetwork.gateway || '';
                document.getElementById('dns1').value = savedNetwork.dns1 || '';
                document.getElementById('dns2').value = savedNetwork.dns2 || '';
            } else {
                useStaticIPCheckbox.checked = false;
                staticIPFields.style.display = 'none';
                // Clear Static IP fields
                document.getElementById('staticIP').value = '';
                document.getElementById('subnet').value = '255.255.255.0';
                document.getElementById('gateway').value = '';
                document.getElementById('dns1').value = '';
                document.getElementById('dns2').value = '';
            }
        } else {
            // Not a saved network - clear password and Static IP fields
            passwordInput.value = '';
            passwordInput.placeholder = 'Enter password';
            useStaticIPCheckbox.checked = false;
            staticIPFields.style.display = 'none';
            document.getElementById('staticIP').value = '';
            document.getElementById('subnet').value = '255.255.255.0';
            document.getElementById('gateway').value = '';
            document.getElementById('dns1').value = '';
            document.getElementById('dns2').value = '';
        }
    }
}

// Load and display saved networks
function loadSavedNetworks() {
    const configSelect = document.getElementById('configNetworkSelect');

    apiFetch('/api/wifilist')
    .then(res => res.safeJson())
    .then(data => {
        if (data.success && data.networks) { // Check success flag specifically
            // Store networks data globally
            savedNetworksData = data.networks;

            if (data.networks.length > 0) {
                // Populate config network select dropdown
                configSelect.innerHTML = '<option value="">-- Select a saved network --</option>';
                data.networks.forEach(net => {
                    const option = document.createElement('option');
                    option.value = net.index;
                    option.textContent = net.ssid + (net.useStaticIP ? ' (Static IP)' : '');
                    configSelect.appendChild(option);
                });
            } else {
                configSelect.innerHTML = '<option value="">-- No saved networks --</option>';
            }
        } else {
            // Show error from API if available
            const errorMsg = data.error || 'Unknown error';
            console.error('API Error loading networks:', errorMsg);
        }
    })
    .catch(err => {
        console.error('Failed to load saved networks:', err);
    });
}

// Remove network by index
// Load network configuration for selected network
function loadNetworkConfig() {
    const select = document.getElementById('configNetworkSelect');
            const fields = document.getElementById('networkConfigFields');
            const staticIPFields = document.getElementById('configStaticIPFields');
            const useStaticIP = document.getElementById('configUseStaticIP');
            const passwordField = document.getElementById('configPassword');

            const selectedIndex = parseInt(select.value);
            if (isNaN(selectedIndex)) {
                fields.style.display = 'none';
                return;
            }

            // Find the network data
            const network = savedNetworksData.find(net => net.index === selectedIndex);
            if (!network) {
                fields.style.display = 'none';
                return;
            }

            // Show fields and populate data
            fields.style.display = 'block';

            // Clear password field (we don't store passwords on frontend for security)
            passwordField.value = '';
            passwordField.placeholder = 'Enter password (leave empty to keep current)';

            useStaticIP.checked = network.useStaticIP || false;

            // Store original values for change detection
            originalNetworkConfig = {
                useStaticIP: network.useStaticIP || false,
                staticIP: network.staticIP || '',
                subnet: network.subnet || '255.255.255.0',
                gateway: network.gateway || '',
                dns1: network.dns1 || '',
                dns2: network.dns2 || ''
            };

            if (network.useStaticIP) {
                staticIPFields.style.display = 'block';
                document.getElementById('configStaticIP').value = network.staticIP || '';
                document.getElementById('configSubnet').value = network.subnet || '255.255.255.0';
                document.getElementById('configGateway').value = network.gateway || '';
                document.getElementById('configDns1').value = network.dns1 || '';
                document.getElementById('configDns2').value = network.dns2 || '';
            } else {
                staticIPFields.style.display = 'none';
                // Clear fields
                document.getElementById('configStaticIP').value = '';
                document.getElementById('configSubnet').value = '255.255.255.0';
                document.getElementById('configGateway').value = '';
                document.getElementById('configDns1').value = '';
                document.getElementById('configDns2').value = '';
            }

            // Update button label
            updateConnectButtonLabel();

            // Add change listeners to update button label
            const fieldsToWatch = ['configPassword', 'configUseStaticIP', 'configStaticIP', 'configSubnet', 'configGateway', 'configDns1', 'configDns2'];
            fieldsToWatch.forEach(fieldId => {
                const field = document.getElementById(fieldId);
                if (field) {
                    field.removeEventListener('input', updateConnectButtonLabel);
                    field.removeEventListener('change', updateConnectButtonLabel);
                    field.addEventListener('input', updateConnectButtonLabel);
                    field.addEventListener('change', updateConnectButtonLabel);
                }
            });
}

function updateConnectButtonLabel() {
    const btn = document.getElementById('connectUpdateBtn');
    if (!btn) return;

    const passwordField = document.getElementById('configPassword');
    const useStaticIP = document.getElementById('configUseStaticIP').checked;

    // Check if password was entered
    const hasPasswordChange = passwordField.value.trim() !== '';

    // Check if static IP settings changed
    const hasStaticIPChange =
        useStaticIP !== originalNetworkConfig.useStaticIP ||
        (useStaticIP && (
            document.getElementById('configStaticIP').value !== originalNetworkConfig.staticIP ||
            document.getElementById('configSubnet').value !== originalNetworkConfig.subnet ||
            document.getElementById('configGateway').value !== originalNetworkConfig.gateway ||
            document.getElementById('configDns1').value !== originalNetworkConfig.dns1 ||
            document.getElementById('configDns2').value !== originalNetworkConfig.dns2
        ));

    // Update button text based on whether changes were made
    if (hasPasswordChange || hasStaticIPChange) {
        btn.textContent = 'Connect & Update';
    } else {
        btn.textContent = 'Connect';
    }
}

// Toggle config static IP fields visibility
function toggleConfigStaticIPFields() {
    const useStaticIP = document.getElementById('configUseStaticIP').checked;
    const fields = document.getElementById('configStaticIPFields');
    fields.style.display = useStaticIP ? 'block' : 'none';
    updateConnectButtonLabel();
}

// Update network configuration
function updateNetworkConfig(connect) {
    const select = document.getElementById('configNetworkSelect');
    const selectedIndex = parseInt(select.value);

    if (isNaN(selectedIndex)) {
        showToast('Please select a network', 'error');
        return;
    }

    const network = savedNetworksData.find(net => net.index === selectedIndex);
    if (!network) {
        showToast('Network not found', 'error');
        return;
    }

    const passwordField = document.getElementById('configPassword');
    const password = passwordField.value.trim();

    // If password is empty, we'll send empty string and backend will keep existing password
    const useStaticIP = document.getElementById('configUseStaticIP').checked;
    const requestBody = {
        ssid: network.ssid,
        password: password, // Empty string keeps existing password
        useStaticIP: useStaticIP
    };

    if (useStaticIP) {
        requestBody.staticIP = document.getElementById('configStaticIP').value;
        requestBody.subnet = document.getElementById('configSubnet').value;
        requestBody.gateway = document.getElementById('configGateway').value;
        requestBody.dns1 = document.getElementById('configDns1').value;
        requestBody.dns2 = document.getElementById('configDns2').value;

        // Validate IP addresses
        if (!isValidIP(requestBody.staticIP)) {
            showToast('Invalid IPv4 address', 'error');
            return;
        }
        if (!isValidIP(requestBody.subnet)) {
            showToast('Invalid network mask', 'error');
            return;
        }
        if (!isValidIP(requestBody.gateway)) {
            showToast('Invalid gateway address', 'error');
            return;
        }
        if (requestBody.dns1 && !isValidIP(requestBody.dns1)) {
            showToast('Invalid primary DNS address', 'error');
            return;
        }
        if (requestBody.dns2 && !isValidIP(requestBody.dns2)) {
            showToast('Invalid secondary DNS address', 'error');
            return;
        }
    }

    // Choose endpoint based on connect parameter
    const endpoint = connect ? '/api/wificonfig' : '/api/wifisave';

    // Show connection modal if connecting
    if (connect) {
        showWiFiModal(network.ssid);
    }

    apiFetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody)
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            if (connect) {
                // Start polling for connection status
                if (wifiConnectionPollTimer) clearInterval(wifiConnectionPollTimer);
                wifiConnectionPollTimer = setInterval(pollWiFiConnection, 2000);
            } else {
                showToast('Network settings updated', 'success');
                loadSavedNetworks(); // Reload the list
                // Clear password field after successful save
                document.getElementById('configPassword').value = '';
            }
        } else {
            if (connect) {
                updateWiFiConnectionStatus('error', data.message || 'Failed to connect');
            } else {
                showToast(data.message || 'Failed to update settings', 'error');
            }
        }
    })
    .catch(err => {
        if (connect) {
            updateWiFiConnectionStatus('error', 'Network error: ' + err.message);
        } else {
            showToast('Network error: ' + err.message, 'error');
        }
    });
}

function removeSelectedNetworkConfig() {
    const select = document.getElementById('configNetworkSelect');
    const selectedIndex = parseInt(select.value);

    if (isNaN(selectedIndex)) {
        showToast('Please select a network to remove', 'error');
        return;
    }

    const network = savedNetworksData.find(net => net.index === selectedIndex);
    if (!network) {
        showToast('Network not found', 'error');
        return;
    }

    // Check if this is the currently connected network
    const isCurrentNetwork = currentWifiConnected && currentWifiSSID === network.ssid;

    if (isCurrentNetwork) {
        // Show warning modal for currently connected network
        showRemoveCurrentNetworkModal(network, selectedIndex);
    } else {
        // Show simple confirmation for other networks
        if (!confirm(`Are you sure you want to remove "${network.ssid}"?`)) {
            return;
        }
        performNetworkRemoval(selectedIndex, false);
    }
}

function showRemoveCurrentNetworkModal(network, selectedIndex) {
    const modal = document.createElement('div');
    modal.id = 'removeNetworkModal';
    modal.className = 'modal-overlay active';
    modal.innerHTML = `
                <div class="modal">
                    <div class="modal-title"><svg viewBox="0 0 24 24" width="20" height="20" fill="var(--warning)" aria-hidden="true" style="vertical-align:middle;margin-right:6px;"><path d="M13,14H11V10H13M13,18H11V16H13M1,21H23L12,2L1,21Z"/></svg>Remove Current Network</div>
                    <div class="info-box" style="background: var(--error-bg); border-color: var(--error);">
                        <div style="padding: 20px;">
                            <div style="font-size: 16px; margin-bottom: 16px; font-weight: bold; color: var(--error);">
                                Warning: You are currently connected to this network
                            </div>
                            <div style="margin-bottom: 12px;">
                                Network: <strong>${network.ssid}</strong>
                            </div>
                            <div style="margin-bottom: 16px; line-height: 1.5;">
                                If you remove this network, the device will:
                                <ul style="margin: 8px 0; padding-left: 20px;">
                                    <li>Disconnect from this network</li>
                                    <li>Try to connect to other saved networks</li>
                                    <li>Start AP Mode if no networks connect successfully</li>
                                </ul>
                            </div>
                            <div style="font-weight: bold;">
                                Do you want to continue?
                            </div>
                        </div>
                    </div>
                    <div class="modal-actions">
                        <button class="secondary" onclick="closeRemoveNetworkModal()">Cancel</button>
                        <button class="primary" style="background: var(--error);" onclick="confirmNetworkRemoval(${selectedIndex})">Remove Network</button>
                    </div>
                </div>
            `;
    document.body.appendChild(modal);
}

function closeRemoveNetworkModal() {
    const modal = document.getElementById('removeNetworkModal');
    if (modal) modal.remove();
}

function confirmNetworkRemoval(selectedIndex) {
    closeRemoveNetworkModal();
    performNetworkRemoval(selectedIndex, true);
}

function performNetworkRemoval(selectedIndex, wasCurrentNetwork) {
    apiFetch('/api/wifiremove', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ index: selectedIndex })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            showToast('Network removed successfully', 'success');

            // Reload the network list
            apiFetch('/api/wifilist')
            .then(res => res.safeJson())
            .then(listData => {
                if (listData.success && listData.networks) {
                    // Store networks data globally
                    savedNetworksData = listData.networks;

                    const configSelect = document.getElementById('configNetworkSelect');
                    if (listData.networks.length > 0) {
                        // Populate config network select dropdown
                        configSelect.innerHTML = '<option value="">-- Select a saved network --</option>';
                        listData.networks.forEach(net => {
                            const option = document.createElement('option');
                            option.value = net.index;
                            option.textContent = net.ssid + (net.useStaticIP ? ' (Static IP)' : '');
                            configSelect.appendChild(option);
                        });
                    } else {
                        configSelect.innerHTML = '<option value="">-- No saved networks --</option>';
                    }

                    // Reset the select dropdown and hide fields
                    configSelect.value = '';
                    document.getElementById('networkConfigFields').style.display = 'none';
                }
            })
            .catch(err => {
                showToast('Failed to reload network list', 'error');
            });

            // If this was the current network, monitor for reconnection or AP mode
            if (wasCurrentNetwork) {
                showToast('Attempting to connect to other saved networks...', 'info');
                monitorNetworkRemoval();
            }
        } else {
            showToast(data.message || 'Failed to remove network', 'error');
        }
    })
    .catch(err => {
        showToast('Network error: ' + err.message, 'error');
    });
}

function monitorNetworkRemoval() {
    let pollCount = 0;
    const maxPolls = 30; // Poll for up to 30 seconds

    if (networkRemovalPollTimer) {
        clearInterval(networkRemovalPollTimer);
    }

    networkRemovalPollTimer = setInterval(() => {
        pollCount++;

        apiFetch('/api/wifistatus')
            .then(res => res.safeJson())
            .then(data => {
                // Check if AP mode is now active and we're not connected to WiFi
                if (data.mode === 'ap' && !data.connected && data.apIP) {
                    clearInterval(networkRemovalPollTimer);
                    networkRemovalPollTimer = null;
                    showAPModeModal(data.apIP);
                }
                // Check if successfully reconnected to another network
                else if (data.connected) {
                    clearInterval(networkRemovalPollTimer);
                    networkRemovalPollTimer = null;
                    showWiFiModal(data.ssid);
                    updateWiFiConnectionStatus('success', 'Network removed. Reconnected successfully!', data.ip);
                }
                // Timeout after max polls
                else if (pollCount >= maxPolls) {
                    clearInterval(networkRemovalPollTimer);
                    networkRemovalPollTimer = null;
                    showWiFiModal('');
                    updateWiFiConnectionStatus('error', 'Failed to connect to any saved network');
                }
            })
            .catch(err => {
                console.error('Error polling WiFi status:', err);
            });
    }, 1000); // Poll every second
}

function toggleAP() {
    const enabled = document.getElementById('apToggle').checked;
    document.getElementById('apFields').style.display = enabled ? '' : 'none';
    apiFetch('/api/toggleap', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) showToast(enabled ? 'AP enabled' : 'AP disabled', 'success');
    })
    .catch(err => showToast('Failed to toggle AP', 'error'));
}

function showAPConfig() {
    // Pre-fill with current AP SSID from stored data
    if (currentAPSSID) {
        document.getElementById('appState.apSSID').value = currentAPSSID;
    }
    document.getElementById('apConfigModal').classList.add('active');
}

function openAPConfig() {
    document.getElementById('apConfigModal').style.display = 'flex';
}

function closeAPConfig() {
    document.getElementById('apConfigModal').classList.remove('active');
    document.getElementById('apConfigModal').style.display = 'none';
}

function submitAPConfig(event) {
    event.preventDefault();
    const ssid = document.getElementById('appState.apSSID').value;
    const password = document.getElementById('appState.apPassword').value;

    if (password.length > 0 && password.length < 8) {
        showToast('Password must be at least 8 characters', 'error');
        return;
    }

    apiFetch('/api/apconfig', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password })
    })
    .then(res => res.safeJson())
    .then(data => {
        if (data.success) {
            showToast('AP settings saved', 'success');
            closeAPConfig();
        } else {
            showToast(data.message || 'Failed to save', 'error');
        }
    })
    .catch(err => showToast('Failed to save AP settings', 'error'));
}
