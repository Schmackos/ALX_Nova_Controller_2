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
