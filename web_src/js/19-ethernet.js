// ===== Ethernet State Variables =====
let currentEthConnected = false;
let currentEthLinkUp = false;
let currentActiveInterface = 'none';
let ethConfirmTimer = null;
let ethConfirmSecondsLeft = 0;

// ===== Network Overview =====
function updateNetworkOverview(data) {
    const activeText = document.getElementById('activeInterfaceText');
    if (activeText) {
        switch (data.activeInterface) {
        case 'ethernet':
            activeText.textContent = 'Ethernet';
            activeText.style.color = 'var(--success)';
            break;
        case 'wifi':
            activeText.textContent = 'WiFi';
            activeText.style.color = '#2196F3';
            break;
        default:
            activeText.textContent = 'None';
            activeText.style.color = 'var(--text-secondary)';
            break;
        }
    }

    const hostnameDisplay = document.getElementById('networkHostnameDisplay');
    if (hostnameDisplay) {
        hostnameDisplay.textContent = escapeHtml(data.ethHostname || 'alx-nova');
    }

    const apBanner = document.getElementById('apAccessBanner');
    if (apBanner) {
        apBanner.style.display = data['appState.apEnabled'] ? '' : 'none';
        const apAccessIP = document.getElementById('apAccessIP');
        if (apAccessIP) {
            apAccessIP.textContent = escapeHtml(data.apIP || '192.168.4.1');
        }
    }
}

// ===== Ethernet Status =====
function updateEthernetStatus(data) {
    currentEthConnected = data.ethConnected || false;
    currentEthLinkUp = data.ethLinkUp || false;
    currentActiveInterface = data.activeInterface || 'none';

    const card = document.getElementById('ethStatusCard');
    const statusBox = document.getElementById('ethStatusBox');
    if (!card || !statusBox) return;

    card.classList.remove('eth-connected', 'eth-link-up', 'eth-disconnected');

    var html = '';

    if (data.ethConnected) {
        card.classList.add('eth-connected');

        var speedText = '';
        if (data.ethSpeed) {
            speedText = ' &mdash; ' + escapeHtml(String(data.ethSpeed)) + ' Mbps';
            if (data.ethFullDuplex) {
                speedText += ' Full Duplex';
            } else {
                speedText += ' Half Duplex';
            }
        }

        html += '<div class="info-row"><span class="info-label">Link Status</span><span class="info-value" style="color:var(--success)">Connected' + speedText + '</span></div>';

        if (data.ethIP) {
            html += '<div class="info-row"><span class="info-label">IP Address</span><span class="info-value">' + escapeHtml(data.ethIP) + '</span></div>';
        }

        html += '<div class="info-row"><span class="info-label">MAC Address</span><span class="info-value">' + escapeHtml(data.ethMAC || '—') + '</span></div>';

        if (data.ethGateway) {
            html += '<div class="info-row"><span class="info-label">Gateway</span><span class="info-value">' + escapeHtml(data.ethGateway) + '</span></div>';
        }

        if (data.ethSubnet) {
            html += '<div class="info-row"><span class="info-label">Subnet</span><span class="info-value">' + escapeHtml(data.ethSubnet) + '</span></div>';
        }

        if (data.ethDns1) {
            var dnsText = escapeHtml(data.ethDns1);
            if (data.ethDns2) {
                dnsText += ', ' + escapeHtml(data.ethDns2);
            }
            html += '<div class="info-row"><span class="info-label">DNS</span><span class="info-value">' + dnsText + '</span></div>';
        }

    } else if (data.ethLinkUp) {
        card.classList.add('eth-link-up');

        html += '<div class="info-row"><span class="info-label">Link Status</span><span class="info-value" style="color:var(--warning)">Link Up — Awaiting DHCP...</span></div>';

        if (data.ethSpeed) {
            var linkSpeedText = escapeHtml(String(data.ethSpeed)) + ' Mbps';
            if (data.ethFullDuplex) {
                linkSpeedText += ' Full Duplex';
            } else {
                linkSpeedText += ' Half Duplex';
            }
            html += '<div class="info-row"><span class="info-label">Speed</span><span class="info-value">' + linkSpeedText + '</span></div>';
        }

        html += '<div class="info-row"><span class="info-label">MAC Address</span><span class="info-value">' + escapeHtml(data.ethMAC || '—') + '</span></div>';

    } else {
        card.classList.add('eth-disconnected');

        html += '<div class="info-row"><span class="info-label">Link Status</span><span class="info-value" style="color:var(--text-secondary)">No cable detected</span></div>';
        html += '<div class="info-row"><span class="info-label">MAC Address</span><span class="info-value">' + escapeHtml(data.ethMAC || '—') + '</span></div>';
    }

    statusBox.innerHTML = html;
}

// ===== Ethernet Config Sync =====
function updateEthConfigFromStatus(data) {
    var hostnameInput = document.getElementById('ethHostnameInput');
    if (hostnameInput && document.activeElement !== hostnameInput) {
        hostnameInput.value = data.ethHostname || '';
    }

    var useStaticCheckbox = document.getElementById('ethUseStaticIP');
    if (useStaticCheckbox) {
        useStaticCheckbox.checked = !!data.ethUseStaticIP;
        var staticFields = document.getElementById('ethStaticIPFields');
        if (staticFields) {
            staticFields.style.display = data.ethUseStaticIP ? 'block' : 'none';
        }
    }

    if (data.ethPendingConfirm) {
        var modal = document.getElementById('ethConfirmModal');
        if (modal && !modal.classList.contains('active')) {
            showEthCountdownModal();
        }
    }
}

// ===== Toggle Static IP Fields =====
function toggleEthStaticIPFields() {
    var useStaticIP = document.getElementById('ethUseStaticIP').checked;
    var fields = document.getElementById('ethStaticIPFields');
    fields.style.display = useStaticIP ? 'block' : 'none';
}

// ===== Submit Ethernet Config =====
function submitEthConfig(event) {
    event.preventDefault();

    var useStaticIP = document.getElementById('ethUseStaticIP').checked;

    var body = { useStaticIP: useStaticIP };

    if (useStaticIP) {
        var staticIP = document.getElementById('ethStaticIP').value.trim();
        var subnet = document.getElementById('ethSubnetInput').value.trim();
        var gateway = document.getElementById('ethGatewayInput').value.trim();
        var dns1 = document.getElementById('ethDns1Input').value.trim();
        var dns2 = document.getElementById('ethDns2Input').value.trim();

        if (!isValidIP(staticIP)) {
            showToast('Invalid static IP address', 'error');
            return;
        }
        if (!isValidIP(subnet)) {
            showToast('Invalid subnet mask', 'error');
            return;
        }
        if (!isValidIP(gateway)) {
            showToast('Invalid gateway address', 'error');
            return;
        }
        if (dns1 && !isValidIP(dns1)) {
            showToast('Invalid primary DNS address', 'error');
            return;
        }
        if (dns2 && !isValidIP(dns2)) {
            showToast('Invalid secondary DNS address', 'error');
            return;
        }

        body.staticIP = staticIP;
        body.subnet = subnet;
        body.gateway = gateway;
        body.dns1 = dns1;
        body.dns2 = dns2;
    }

    apiFetch('/api/ethconfig', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    })
    .then(function(res) { return res.safeJson(); })
    .then(function(data) {
        if (data.pendingConfirm) {
            showEthCountdownModal();
        } else {
            showToast('Ethernet configuration saved', 'success');
        }
    })
    .catch(function() { showToast('Failed to apply config', 'error'); });
}

// ===== Submit Hostname =====
function submitHostname() {
    var value = document.getElementById('ethHostnameInput').value.trim();

    if (!value || value.length > 63) {
        showToast('Hostname must be 1–63 characters', 'error');
        return;
    }
    if (!/^[a-zA-Z0-9-]+$/.test(value)) {
        showToast('Hostname may only contain letters, numbers, and hyphens', 'error');
        return;
    }
    if (value.charAt(0) === '-' || value.charAt(value.length - 1) === '-') {
        showToast('Hostname cannot start or end with a hyphen', 'error');
        return;
    }

    apiFetch('/api/ethconfig', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hostname: value })
    })
    .then(function(res) { return res.safeJson(); })
    .then(function() { showToast('Hostname saved', 'success'); })
    .catch(function() { showToast('Failed to save hostname', 'error'); });
}

// ===== Ethernet Confirm Modal =====
function showEthCountdownModal() {
    var modal = document.getElementById('ethConfirmModal');
    if (!modal) return;

    modal.classList.add('active');
    ethConfirmSecondsLeft = 60;

    var countdown = document.getElementById('ethConfirmCountdown');
    if (countdown) {
        countdown.textContent = ethConfirmSecondsLeft;
    }

    var fallbackInfo = document.getElementById('ethConfirmFallbackInfo');
    if (fallbackInfo) {
        fallbackInfo.textContent = 'If you lose connection, access via WiFi or AP mode (192.168.4.1)';
    }

    if (ethConfirmTimer) {
        clearInterval(ethConfirmTimer);
    }

    ethConfirmTimer = setInterval(function() {
        ethConfirmSecondsLeft -= 1;
        if (countdown) {
            countdown.textContent = ethConfirmSecondsLeft;
        }
        if (ethConfirmSecondsLeft <= 0) {
            closeEthConfirmModal();
            showToast('Configuration reverted to DHCP', 'info');
        }
    }, 1000);
}

function confirmEthConfig() {
    apiFetch('/api/ethconfig/confirm', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(function(res) { return res.safeJson(); })
    .then(function() {
        closeEthConfirmModal();
        showToast('Configuration confirmed and saved', 'success');
    })
    .catch(function() { showToast('Failed to confirm configuration', 'error'); });
}

function cancelEthConfig() {
    apiFetch('/api/ethconfig', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ useStaticIP: false })
    })
    .then(function(res) { return res.safeJson(); })
    .then(function() {
        closeEthConfirmModal();
        showToast('Reverted to DHCP', 'info');
    })
    .catch(function() {
        closeEthConfirmModal();
        showToast('Reverted to DHCP', 'info');
    });
}

function closeEthConfirmModal() {
    var modal = document.getElementById('ethConfirmModal');
    if (modal) {
        modal.classList.remove('active');
    }
    if (ethConfirmTimer) {
        clearInterval(ethConfirmTimer);
        ethConfirmTimer = null;
    }
}
