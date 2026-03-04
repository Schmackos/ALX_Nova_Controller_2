// ===== Firmware Update =====

function checkForUpdate() {
    showToast('Checking for updates...', 'info');
    apiFetch('/api/checkupdate')
    .then(res => res.json())
    .then(data => {
        // Update current version display
        if (data.currentVersion) {
            currentFirmwareVersion = data.currentVersion;
            document.getElementById('currentVersion').textContent = data.currentVersion;
        }

        // Update latest version display
        if (data.latestVersion) {
            currentLatestVersion = data.latestVersion;
            const latestVersionEl = document.getElementById('latestVersion');
            const latestVersionNotes = document.getElementById('latestVersionNotes');
            document.getElementById('latestVersionRow').style.display = 'flex';

            // If up-to-date, show green "Up-To-Date" text and hide release notes link
            if (!data.updateAvailable && data.latestVersion !== 'Unknown') {
                latestVersionEl.textContent = 'Up-To-Date, no newer version available';
                latestVersionEl.style.opacity = '1';
                latestVersionEl.style.fontStyle = 'normal';
                latestVersionEl.style.color = 'var(--success)';
                latestVersionNotes.style.display = 'none';
            } else {
                latestVersionEl.textContent = data.latestVersion;
                latestVersionNotes.style.display = '';

                // Style based on status
                if (data.latestVersion === 'Unknown') {
                    latestVersionEl.style.opacity = '0.6';
                    latestVersionEl.style.fontStyle = 'italic';
                    latestVersionEl.style.color = 'var(--text-secondary)';
                } else {
                    latestVersionEl.style.opacity = '1';
                    latestVersionEl.style.fontStyle = 'normal';
                    latestVersionEl.style.color = '';
                }
            }
        }

        // Show/hide update button based on update availability
        if (data.updateAvailable) {
            document.getElementById('updateBtn').classList.remove('hidden');
            showToast(`Update available: ${data.latestVersion}`, 'success');
        } else {
            document.getElementById('updateBtn').classList.add('hidden');
            showToast('Firmware is up to date', 'success');
        }

        // Reset progress UI when checking
        document.getElementById('progressContainer').classList.add('hidden');
        document.getElementById('progressStatus').classList.add('hidden');
    })
    .catch(err => showToast('Failed to check for updates', 'error'));
}

function fetchUpdateStatus() {
    apiFetch('/api/updatestatus')
    .then(res => res.json())
    .then(data => handleUpdateStatus(data))
    .catch(err => console.error('Failed to fetch update status:', err));
}

function startOTAUpdate() {
    apiFetch('/api/startupdate', { method: 'POST' })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            document.getElementById('progressContainer').classList.remove('hidden');
            document.getElementById('progressStatus').classList.remove('hidden');
            showToast('Update started', 'success');
        } else {
            showToast(data.message || 'Failed to start update', 'error');
        }
    })
    .catch(err => showToast('Failed to start update', 'error'));
}

function handleUpdateStatus(data) {
    if (data.otaChannel !== undefined) {
        otaChannel = data.otaChannel;
        const sel = document.getElementById('otaChannelSelect');
        if (sel) sel.value = String(otaChannel);
    }

    // Skip progress bar updates if manual upload is in progress
    // Manual upload manages its own progress bar
    if (manualUploadInProgress) {
        return;
    }

    const container = document.getElementById('progressContainer');
    const bar = document.getElementById('progressBar');
    const status = document.getElementById('progressStatus');
    const updateBtn = document.getElementById('updateBtn');

    // Update version info if available
    if (data.currentVersion) {
        currentFirmwareVersion = data.currentVersion;
        document.getElementById('currentVersion').textContent = data.currentVersion;
    }
    if (data.latestVersion) {
        currentLatestVersion = data.latestVersion;
        const latestVersionEl = document.getElementById('latestVersion');
        latestVersionEl.textContent = data.latestVersion;
        document.getElementById('latestVersionRow').style.display = 'flex';

        // Style based on status
        if (data.latestVersion === 'Unknown') {
            latestVersionEl.style.opacity = '0.6';
            latestVersionEl.style.fontStyle = 'italic';
            latestVersionEl.style.color = 'var(--text-secondary)';
        } else {
            latestVersionEl.style.opacity = '1';
            latestVersionEl.style.fontStyle = 'normal';
            latestVersionEl.style.color = '';
        }
    }

    // Handle different status states
    if (data.status === 'preparing') {
        container.classList.remove('hidden');
        status.classList.remove('hidden');
        bar.style.width = '0%';
        status.textContent = data.message || 'Preparing for update...';
        updateBtn.classList.add('hidden');
    } else if (data.status === 'downloading' || data.status === 'uploading') {
        container.classList.remove('hidden');
        status.classList.remove('hidden');
        bar.style.width = data.progress + '%';
        // Show percentage and downloaded size for OTA
        let statusText = `${data.progress}%`;
        if (data.bytesDownloaded !== undefined && data.totalBytes !== undefined && data.totalBytes > 0) {
            const downloadedKB = (data.bytesDownloaded / 1024).toFixed(0);
            const totalKB = (data.totalBytes / 1024).toFixed(0);
            statusText = `Downloading: ${data.progress}% (${downloadedKB} / ${totalKB} KB)`;
        }
        status.textContent = data.message || statusText;
        updateBtn.classList.add('hidden');
    } else if (data.status === 'complete') {
        bar.style.width = '100%';
        status.textContent = 'Update complete! Rebooting...';
        showToast('Update complete! Rebooting...', 'success');
        updateBtn.classList.add('hidden');
        wasDisconnectedDuringUpdate = true;  // Flag for reconnection notification
    } else if (data.status === 'error') {
        container.classList.add('hidden');
        status.classList.add('hidden');
        showToast(data.message || 'Update failed', 'error');
        // Re-show update button if update is still available
        if (data.updateAvailable) {
            updateBtn.classList.remove('hidden');
        }
    } else {
        // Idle state or unknown - reset UI
        container.classList.add('hidden');
        status.classList.add('hidden');

        // Show/hide update button based on update availability
        if (data.updateAvailable) {
            updateBtn.classList.remove('hidden');
        } else {
            updateBtn.classList.add('hidden');
        }
    }
}

function showUpdateSuccessNotification(data) {
    // Clear the flag so we don't show duplicate notification
    wasDisconnectedDuringUpdate = false;
    showToast(`Firmware updated: ${data.previousVersion} → ${data.currentVersion}`, 'success');
}

// ===== Manual Firmware Upload =====
function handleFirmwareSelect(event) {
    const file = event.target.files[0];
    if (file) uploadFirmware(file);
}

function uploadFirmware(file) {
    if (!file.name.endsWith('.bin')) {
        showToast('Please select a .bin file', 'error');
        return;
    }

    manualUploadInProgress = true;
    const container = document.getElementById('progressContainer');
    const bar = document.getElementById('progressBar');
    const status = document.getElementById('progressStatus');

    container.classList.remove('hidden');
    status.classList.remove('hidden');
    status.textContent = 'Uploading...';

    const formData = new FormData();
    formData.append('firmware', file);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/firmware/upload', true);

    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            bar.style.width = percent + '%';
            status.textContent = `Uploading: ${percent}%`;
        }
    };

    xhr.onload = function() {
        manualUploadInProgress = false;
        if (xhr.status === 200) {
            const response = JSON.parse(xhr.responseText);
            if (response.success) {
                status.textContent = 'Upload complete! Rebooting...';
                showToast('Firmware uploaded successfully', 'success');
                wasDisconnectedDuringUpdate = true;  // Flag for reconnection notification
            } else {
                container.classList.add('hidden');
                status.classList.add('hidden');
                showToast(response.message || 'Upload failed', 'error');
            }
        } else {
            container.classList.add('hidden');
            status.classList.add('hidden');
            showToast('Upload failed', 'error');
        }
    };

    xhr.onerror = function() {
        manualUploadInProgress = false;
        container.classList.add('hidden');
        status.classList.add('hidden');
        showToast('Upload failed', 'error');
    };

    xhr.send(formData);
}

// ===== OTA Channel =====

function setOtaChannel() {
    const val = parseInt(document.getElementById('otaChannelSelect').value);
    otaChannel = val;
    apiFetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ otaChannel: val })
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            showToast(val === 0 ? 'Channel: Stable' : 'Channel: Beta', 'success');
            cachedReleaseList = [];
            const browser = document.getElementById('releasesBrowser');
            if (browser && !browser.classList.contains('hidden')) {
                loadReleaseList();
            }
        }
    })
    .catch(() => showToast('Failed to set channel', 'error'));
}

// ===== Release Browser =====

function toggleReleasesBrowser() {
    const browser = document.getElementById('releasesBrowser');
    if (!browser) return;
    const isVisible = !browser.classList.contains('hidden');
    if (isVisible) {
        browser.classList.add('hidden');
    } else {
        browser.classList.remove('hidden');
        loadReleaseList();
    }
}

function loadReleaseList() {
    if (releaseListLoading) return;
    releaseListLoading = true;
    const loading = document.getElementById('releaseListLoading');
    const items = document.getElementById('releaseListItems');
    if (loading) loading.style.display = '';
    if (items) items.innerHTML = '';

    apiFetch('/api/releases')
    .then(res => res.json())
    .then(data => {
        releaseListLoading = false;
        if (loading) loading.style.display = 'none';
        if (!data.success || !data.releases || data.releases.length === 0) {
            if (items) items.innerHTML = '<div class="text-secondary" style="font-size:13px;padding:8px;">No releases found.</div>';
            return;
        }
        cachedReleaseList = data.releases;
        renderReleaseList(data.releases);
    })
    .catch(() => {
        releaseListLoading = false;
        if (loading) loading.style.display = 'none';
        if (items) items.innerHTML = '<div class="text-secondary" style="font-size:13px;padding:8px;">Failed to load releases.</div>';
    });
}

function renderReleaseList(releases) {
    const container = document.getElementById('releaseListItems');
    if (!container) return;
    const curVer = currentFirmwareVersion || '';
    container.innerHTML = '';
    releases.forEach(function(rel) {
        const isCurrent = rel.version === curVer || rel.version === 'v' + curVer || 'v' + rel.version === curVer;
        const isDown = isOlderRelease(rel.version, curVer);
        const badge = rel.isPrerelease ? ' <span style="color:var(--warning);font-size:11px;font-weight:600;">[beta]</span>' : '';
        const dateStr = rel.publishedAt || '';
        const div = document.createElement('div');
        div.style.cssText = 'display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid var(--border);';
        let btnHtml = '';
        if (isCurrent) {
            btnHtml = '<span style="color:var(--success);font-size:12px;white-space:nowrap;">Current</span>';
        } else if (isDown) {
            btnHtml = '<button class="btn btn-sm" style="background:var(--warning);color:#000;white-space:nowrap;" onclick="installRelease(\'' + rel.version.replace(/'/g, "\\'") + '\',true)">Downgrade</button>';
        } else {
            btnHtml = '<button class="btn btn-sm btn-primary" style="white-space:nowrap;" onclick="installRelease(\'' + rel.version.replace(/'/g, "\\'") + '\',false)">Install</button>';
        }
        div.innerHTML = '<div style="min-width:0;"><span style="font-weight:600;">' + rel.version + '</span>' + badge +
            '<span class="text-secondary" style="font-size:12px;margin-left:8px;">' + dateStr + '</span></div>' +
            '<div style="margin-left:8px;">' + btnHtml + '</div>';
        container.appendChild(div);
    });
}

function isOlderRelease(v1, v2) {
    // Returns true if v1 is older than v2
    const strip = function(v) { return v.replace(/^v/, ''); };
    const a = strip(v1), b = strip(v2);
    const baseParts = function(s) { return s.replace(/-beta\.\d+/, '').split('.').map(Number); };
    const betaN = function(s) { const m = s.match(/-beta\.(\d+)/); return m ? parseInt(m[1]) : 0; };
    const pa = baseParts(a), pb = baseParts(b);
    for (let i = 0; i < 3; i++) {
        if ((pa[i]||0) < (pb[i]||0)) return true;
        if ((pa[i]||0) > (pb[i]||0)) return false;
    }
    const ba = betaN(a), bb = betaN(b);
    if (ba === 0 && bb === 0) return false;
    if (ba === 0 && bb > 0) return false;
    if (ba > 0 && bb === 0) return true;
    return ba < bb;
}

function installRelease(version, isDowngrade) {
    const msg = isDowngrade
        ? 'Downgrade to ' + version + '? Settings may be incompatible with older firmware.'
        : 'Install version ' + version + '?';
    if (!confirm(msg)) return;

    apiFetch('/api/installrelease', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ version: version })
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            document.getElementById('releasesBrowser').classList.add('hidden');
            document.getElementById('progressContainer').classList.remove('hidden');
            document.getElementById('progressStatus').classList.remove('hidden');
            showToast('Installing ' + version + '...', 'success');
        } else {
            showToast(data.message || 'Install failed', 'error');
        }
    })
    .catch(() => showToast('Failed to start install', 'error'));
}

// Drag and drop for firmware
function initFirmwareDragDrop() {
    const dropZone = document.getElementById('firmwareDropZone');

    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, e => {
            e.preventDefault();
            e.stopPropagation();
        });
    });

    ['dragenter', 'dragover'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'));
    });

    ['dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'));
    });

    dropZone.addEventListener('drop', e => {
        const file = e.dataTransfer.files[0];
        if (file) uploadFirmware(file);
    });
}
