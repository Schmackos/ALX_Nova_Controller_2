        // ===== HAL Device Management =====
        // Provides device discovery, configuration, monitoring, and CRUD UI

        var halDevices = [];
        var halScanning = false;
        var halExpandedSlot = -1;
        var halEditingSlot = -1;

        function handleHalDeviceState(data) {
            halScanning = data.scanning || false;
            halDevices = data.devices || [];
            renderHalDevices();
        }

        function renderHalDevices() {
            var container = document.getElementById('hal-device-list');
            if (!container) return;

            var scanBtn = document.getElementById('hal-rescan-btn');
            if (scanBtn) {
                scanBtn.disabled = halScanning;
                scanBtn.textContent = halScanning ? 'Scanning...' : 'Rescan Devices';
            }

            if (halDevices.length === 0) {
                container.innerHTML = '<div class="empty-state">No HAL devices registered</div>';
                return;
            }

            var html = '';
            for (var i = 0; i < halDevices.length; i++) {
                html += buildHalDeviceCard(halDevices[i]);
            }
            container.innerHTML = html;
        }

        function halGetStateInfo(state) {
            switch (state) {
                case 1: return { cls: 'blue', label: 'Detected' };
                case 2: return { cls: 'amber', label: 'Configuring' };
                case 3: return { cls: 'green', label: 'Available' };
                case 4: return { cls: 'red', label: 'Unavailable' };
                case 5: return { cls: 'red', label: 'Error' };
                case 6: return { cls: 'amber', label: 'Manual' };
                case 7: return { cls: 'grey', label: 'Removed' };
                default: return { cls: 'grey', label: 'Unknown' };
            }
        }

        function halGetTypeInfo(type) {
            var types = {
                1: { label: 'DAC', icon: 'M12,3L1,9L12,15L21,10.09V17H23V9M5,13.18V17.18L12,21L19,17.18V13.18L12,17L5,13.18Z' },
                2: { label: 'ADC', icon: 'M12,2A3,3 0 0,1 15,5V11A3,3 0 0,1 12,14A3,3 0 0,1 9,11V5A3,3 0 0,1 12,2M19,11C19,14.53 16.39,17.44 13,17.93V21H11V17.93C7.61,17.44 5,14.53 5,11H7A5,5 0 0,0 12,16A5,5 0 0,0 17,11H19Z' },
                3: { label: 'Codec', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' },
                4: { label: 'Amp', icon: 'M14,3.23V5.29C16.89,6.15 19,8.83 19,12C19,15.17 16.89,17.84 14,18.7V20.77C18,19.86 21,16.28 21,12C21,7.72 18,4.14 14,3.23M16.5,12C16.5,10.23 15.5,8.71 14,7.97V16C15.5,15.29 16.5,13.76 16.5,12M3,9V15H7L12,20V4L7,9H3Z' },
                5: { label: 'DSP', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' },
                6: { label: 'Sensor', icon: 'M15,13V5A3,3 0 0,0 9,5V13A5,5 0 1,0 15,13M12,4A1,1 0 0,1 13,5V8H11V5A1,1 0 0,1 12,4Z' },
                7: { label: 'Display', icon: 'M21,16H3V4H21M21,2H3C1.89,2 1,2.89 1,4V16A2,2 0 0,0 3,18H10V20H8V22H16V20H14V18H21A2,2 0 0,0 23,16V4C23,2.89 22.1,2 21,2Z' },
                8: { label: 'Input', icon: 'M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4M12,6A6,6 0 0,0 6,12A6,6 0 0,0 12,18A6,6 0 0,0 18,12A6,6 0 0,0 12,6M12,8A4,4 0 0,1 16,12A4,4 0 0,1 12,16A4,4 0 0,1 8,12A4,4 0 0,1 12,8Z' },
                9: { label: 'GPIO', icon: 'M16,7V3H14V7H10V3H8V7H8C7,7 6,8 6,9V14.5L9.5,18V21H14.5V18L18,14.5V9C18,8 17,7 16,7Z' }
            };
            return types[type] || { label: 'Unknown', icon: 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11' };
        }

        function halGetBusLabel(busType) {
            return ['None', 'I2C', 'I2S', 'SPI', 'GPIO', 'Internal'][busType] || 'Unknown';
        }

        function halGetDiscLabel(disc) {
            return ['Builtin', 'EEPROM', 'GPIO ID', 'Manual', 'Online'][disc] || 'Unknown';
        }

        function buildHalDeviceCard(d) {
            var si = halGetStateInfo(d.state);
            var ti = halGetTypeInfo(d.type);
            var expanded = (halExpandedSlot === d.slot);
            var editing = (halEditingSlot === d.slot);
            var displayName = (d.userLabel && d.userLabel.length > 0) ? d.userLabel : (d.name || d.compatible);

            var stateClass = 'state-' + si.label.toLowerCase();
            var h = '<div class="card hal-device-card ' + stateClass + (expanded ? ' expanded' : '') + '">';

            // Header row - clickable to expand
            h += '<div class="hal-device-header" onclick="halToggleExpand(' + d.slot + ')">';
            h += '<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor" aria-hidden="true"><path d="' + ti.icon + '"/></svg>';
            h += '<span class="hal-device-name">' + escapeHtml(displayName) + '</span>';
            if (d.type === 6 && d.temperature !== undefined) {
                h += '<span class="hal-temp-reading">' + d.temperature.toFixed(1) + ' &deg;C</span>';
            }
            h += '<span class="status-dot status-' + si.cls + '" title="' + si.label + '"></span>';
            h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" class="hal-expand-icon' + (expanded ? ' rotated' : '') + '" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
            h += '</div>';

            // Badge row
            h += '<div class="hal-device-info">';
            h += '<span class="badge badge-' + si.cls + '">' + si.label + '</span>';
            h += '<span class="badge">' + ti.label + '</span>';
            h += '<span class="badge">' + halGetDiscLabel(d.discovery) + '</span>';
            if (d.ready) h += '<span class="badge badge-green">Ready</span>';
            h += '</div>';

            // Expanded detail section
            if (expanded) {
                h += '<div class="hal-device-details">';

                // Device info
                h += '<div class="hal-detail-row"><span>Compatible:</span><span>' + escapeHtml(d.compatible || '') + '</span></div>';
                if (d.manufacturer) h += '<div class="hal-detail-row"><span>Manufacturer:</span><span>' + escapeHtml(d.manufacturer) + '</span></div>';
                h += '<div class="hal-detail-row"><span>Bus:</span><span>' + halGetBusLabel(d.busType || 0) + (d.busIndex > 0 ? ' #' + d.busIndex : '') + '</span></div>';
                if (d.i2cAddr > 0) h += '<div class="hal-detail-row"><span>I2C Address:</span><span>0x' + d.i2cAddr.toString(16).toUpperCase().padStart(2, '0') + '</span></div>';
                if (d.busFreq > 0) h += '<div class="hal-detail-row"><span>Bus Freq:</span><span>' + (d.busFreq >= 1000000 ? (d.busFreq/1000000).toFixed(1) + ' MHz' : (d.busFreq/1000) + ' kHz') + '</span></div>';
                if (d.pinA >= 0) h += '<div class="hal-detail-row"><span>Pin A (SDA/Data):</span><span>GPIO ' + d.pinA + '</span></div>';
                if (d.pinB >= 0) h += '<div class="hal-detail-row"><span>Pin B (SCL/CLK):</span><span>GPIO ' + d.pinB + '</span></div>';
                if (d.channels > 0) h += '<div class="hal-detail-row"><span>Channels:</span><span>' + d.channels + '</span></div>';
                h += '<div class="hal-detail-row"><span>Slot:</span><span>' + d.slot + '</span></div>';

                // Capabilities
                if (d.capabilities > 0) {
                    var caps = [];
                    if (d.capabilities & 1) caps.push('HW Volume');
                    if (d.capabilities & 2) caps.push('Filters');
                    if (d.capabilities & 4) caps.push('Mute');
                    if (d.capabilities & 8) caps.push('ADC');
                    if (d.capabilities & 16) caps.push('DAC');
                    h += '<div class="hal-detail-row"><span>Capabilities:</span><span>' + caps.join(', ') + '</span></div>';
                }

                // Sample rates
                if (d.sampleRates > 0) {
                    var rates = [];
                    if (d.sampleRates & 1) rates.push('8k');
                    if (d.sampleRates & 2) rates.push('16k');
                    if (d.sampleRates & 4) rates.push('44.1k');
                    if (d.sampleRates & 8) rates.push('48k');
                    if (d.sampleRates & 16) rates.push('96k');
                    if (d.sampleRates & 32) rates.push('192k');
                    h += '<div class="hal-detail-row"><span>Sample Rates:</span><span>' + rates.join(', ') + '</span></div>';
                }

                // Edit form
                if (editing) {
                    h += halBuildEditForm(d);
                }

                // Action buttons
                h += '<div class="hal-device-actions">';
                if (!editing) {
                    h += '<button class="btn btn-sm" onclick="halStartEdit(' + d.slot + ')" title="Configure"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg> Edit</button>';
                }
                h += '<button class="btn btn-sm" onclick="halReinitDevice(' + d.slot + ')" title="Re-initialize"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M17.65,6.35C16.2,4.9 14.21,4 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20C15.73,20 18.84,17.45 19.73,14H17.65C16.83,16.33 14.61,18 12,18A6,6 0 0,1 6,12A6,6 0 0,1 12,6C13.66,6 15.14,6.69 16.22,7.78L13,11H20V4L17.65,6.35Z"/></svg> Reinit</button>';
                h += '<button class="btn btn-sm" onclick="exportDeviceYaml(' + d.slot + ')" title="Export YAML"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M5,20H19V18H5M19,9H15V3H9V9H5L12,16L19,9Z"/></svg> Export</button>';
                if (d.discovery !== 0) {  // Can't remove builtins
                    h += '<button class="btn btn-sm hal-btn-remove" onclick="halRemoveDevice(' + d.slot + ')" title="Remove"><svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg> Remove</button>';
                }
                h += '</div>';

                h += '</div>'; // hal-device-details
            }

            h += '</div>'; // card
            return h;
        }

        function halBuildEditForm(d) {
            var busType = d.busType || 0;
            var h = '<div class="hal-edit-form">';
            h += '<div class="hal-form-title">Device Configuration</div>';

            // User label
            h += '<div class="hal-form-row">';
            h += '<label>Label:</label>';
            h += '<input type="text" id="halCfgLabel" value="' + escapeHtml(d.userLabel || '') + '" placeholder="' + escapeHtml(d.name || '') + '" maxlength="32">';
            h += '</div>';

            // Enable toggle
            h += '<div class="hal-form-row">';
            h += '<label>Enabled:</label>';
            h += '<label class="toggle-label"><input type="checkbox" id="halCfgEnabled" ' + (d.cfgEnabled !== false ? 'checked' : '') + '> <span>Active</span></label>';
            h += '</div>';

            // I2C settings (for I2C bus devices)
            if (busType === 1) {
                h += '<div class="hal-form-section">I2C Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Address:</label>';
                h += '<input type="text" id="halCfgI2cAddr" value="0x' + (d.i2cAddr || 0).toString(16).toUpperCase().padStart(2, '0') + '" style="width:60px;">';
                h += '</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Bus:</label>';
                h += '<select id="halCfgI2cBus">';
                h += '<option value="0"' + ((d.busIndex || 0) === 0 ? ' selected' : '') + '>External (GPIO 48/54)</option>';
                h += '<option value="1"' + ((d.busIndex || 0) === 1 ? ' selected' : '') + '>Onboard (GPIO 7/8)</option>';
                h += '<option value="2"' + ((d.busIndex || 0) === 2 ? ' selected' : '') + '>Expansion (GPIO 28/29)</option>';
                h += '</select>';
                h += '</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Speed:</label>';
                h += '<select id="halCfgI2cSpeed">';
                h += '<option value="100000"' + ((d.busFreq || 0) <= 100000 ? ' selected' : '') + '>100 kHz</option>';
                h += '<option value="400000"' + ((d.busFreq || 0) >= 400000 ? ' selected' : '') + '>400 kHz</option>';
                h += '</select>';
                h += '</div>';
            }

            // I2S settings (for audio devices: DAC, ADC, Codec)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Audio Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>I2S Port:</label>';
                h += '<select id="halCfgI2sPort">';
                for (var p = 0; p < 3; p++) {
                    h += '<option value="' + p + '"' + (p === (d.cfgI2sPort !== undefined ? d.cfgI2sPort : 0) ? ' selected' : '') + '>I2S ' + p + '</option>';
                }
                h += '</select>';
                h += '</div>';

                // Volume (for devices with HW volume capability)
                if (d.capabilities & 1) {
                    h += '<div class="hal-form-row">';
                    h += '<label>Volume:</label>';
                    h += '<input type="range" id="halCfgVolume" min="0" max="100" value="' + (d.cfgVolume !== undefined ? d.cfgVolume : 100) + '" oninput="document.getElementById(\'halCfgVolLabel\').textContent=this.value+\'%\'" style="flex:1;"><span id="halCfgVolLabel">' + (d.cfgVolume !== undefined ? d.cfgVolume : 100) + '%</span>';
                    h += '</div>';
                }

                // Mute (for devices with mute capability)
                if (d.capabilities & 4) {
                    h += '<div class="hal-form-row">';
                    h += '<label>Mute:</label>';
                    h += '<label class="toggle-label"><input type="checkbox" id="halCfgMute" ' + (d.cfgMute ? 'checked' : '') + '> <span>Muted</span></label>';
                    h += '</div>';
                }
            }

            // GPIO settings (for GPIO devices like amp)
            if (busType === 4) {
                h += '<div class="hal-form-section">GPIO Settings</div>';
                h += '<div class="hal-form-row">';
                h += '<label>Pin:</label>';
                h += '<input type="number" id="halCfgPinA" value="' + (d.pinA >= 0 ? d.pinA : '') + '" min="0" max="54" style="width:60px;">';
                h += '</div>';
            }

            // Pin overrides (for I2C/I2S devices)
            if (busType === 1 || busType === 2) {
                h += '<div class="hal-form-section">Pin Overrides (-1 = default)</div>';
                h += '<div class="hal-form-row">';
                h += '<label>SDA/Data:</label>';
                h += '<input type="number" id="halCfgPinSda" value="' + (d.cfgPinSda !== undefined ? d.cfgPinSda : -1) + '" min="-1" max="54" style="width:60px;">';
                h += '</div>';
                h += '<div class="hal-form-row">';
                h += '<label>SCL/CLK:</label>';
                h += '<input type="number" id="halCfgPinScl" value="' + (d.cfgPinScl !== undefined ? d.cfgPinScl : -1) + '" min="-1" max="54" style="width:60px;">';
                h += '</div>';
            }

            // Save/Cancel
            h += '<div class="hal-form-buttons">';
            h += '<button class="btn btn-primary btn-sm" onclick="halSaveConfig(' + d.slot + ')">Save</button>';
            h += '<button class="btn btn-sm" onclick="halCancelEdit()">Cancel</button>';
            h += '</div>';

            h += '</div>';
            return h;
        }

        function halToggleExpand(slot) {
            halExpandedSlot = (halExpandedSlot === slot) ? -1 : slot;
            halEditingSlot = -1;
            renderHalDevices();
        }

        function halStartEdit(slot) {
            halEditingSlot = slot;
            halExpandedSlot = slot;
            renderHalDevices();
        }

        function halCancelEdit() {
            halEditingSlot = -1;
            renderHalDevices();
        }

        function halSaveConfig(slot) {
            var cfg = {};
            cfg.slot = slot;

            var labelEl = document.getElementById('halCfgLabel');
            if (labelEl) cfg.label = labelEl.value;

            var enabledEl = document.getElementById('halCfgEnabled');
            if (enabledEl) cfg.enabled = enabledEl.checked;

            var i2cAddrEl = document.getElementById('halCfgI2cAddr');
            if (i2cAddrEl) cfg.i2cAddr = parseInt(i2cAddrEl.value, 16) || 0;

            var i2cBusEl = document.getElementById('halCfgI2cBus');
            if (i2cBusEl) cfg.i2cBus = parseInt(i2cBusEl.value) || 0;

            var i2cSpeedEl = document.getElementById('halCfgI2cSpeed');
            if (i2cSpeedEl) cfg.i2cSpeed = parseInt(i2cSpeedEl.value) || 0;

            var i2sPortEl = document.getElementById('halCfgI2sPort');
            if (i2sPortEl) cfg.i2sPort = parseInt(i2sPortEl.value) || 0;

            var volumeEl = document.getElementById('halCfgVolume');
            if (volumeEl) cfg.volume = parseInt(volumeEl.value) || 0;

            var muteEl = document.getElementById('halCfgMute');
            if (muteEl) cfg.mute = muteEl.checked;

            var pinSdaEl = document.getElementById('halCfgPinSda');
            if (pinSdaEl) cfg.pinSda = parseInt(pinSdaEl.value);

            var pinSclEl = document.getElementById('halCfgPinScl');
            if (pinSclEl) cfg.pinScl = parseInt(pinSclEl.value);

            var pinAEl = document.getElementById('halCfgPinA');
            if (pinAEl) cfg.pinSda = parseInt(pinAEl.value);

            fetch('/api/hal/devices', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(cfg)
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.status === 'ok') {
                    showToast('Device config saved');
                    halEditingSlot = -1;
                    loadHalDeviceList();
                } else {
                    showToast('Save failed: ' + (data.error || ''), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halReinitDevice(slot) {
            fetch('/api/hal/devices/reinit', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slot })
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                showToast(data.status === 'ok' ? 'Device re-initialized' : 'Reinit failed', data.status !== 'ok');
                loadHalDeviceList();
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halRemoveDevice(slot) {
            if (!confirm('Remove this device? It can be re-added later.')) return;
            fetch('/api/hal/devices', {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slot })
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.status === 'ok') {
                    showToast('Device removed');
                    halExpandedSlot = -1;
                    loadHalDeviceList();
                } else {
                    showToast('Remove failed: ' + (data.error || ''), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halAddFromPreset() {
            fetch('/api/hal/db/presets')
                .then(function(r) { return r.json(); })
                .then(function(presets) {
                    var sel = document.getElementById('halAddPresetSelect');
                    if (!sel) return;
                    sel.innerHTML = '<option value="">-- Select Device --</option>';
                    for (var i = 0; i < presets.length; i++) {
                        var p = presets[i];
                        sel.innerHTML += '<option value="' + escapeHtml(p.compatible) + '">' + escapeHtml(p.name) + ' (' + ['Unknown','DAC','ADC','Codec','Amp','DSP','Sensor'][p.type || 0] + ')</option>';
                    }
                })
                .catch(function(err) { showToast('Failed to load presets: ' + err, true); });
        }

        function halRegisterPreset() {
            var sel = document.getElementById('halAddPresetSelect');
            if (!sel || !sel.value) { showToast('Select a device first', true); return; }

            fetch('/api/hal/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ compatible: sel.value })
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.status === 'ok') {
                    showToast('Device registered in slot ' + data.slot);
                    loadHalDeviceList();
                } else {
                    showToast('Registration failed: ' + (data.error || ''), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function triggerHalRescan() {
            if (halScanning) return;  // Prevent double-click
            halScanning = true;
            renderHalDevices();
            fetch('/api/hal/scan', { method: 'POST' })
                .then(function(r) {
                    if (r.status === 409) {
                        showToast('Scan already in progress');
                        return null;
                    }
                    return r.json();
                })
                .then(function(data) {
                    if (data) showToast('Scan complete: ' + (data.devicesFound || 0) + ' devices found');
                })
                .catch(function(err) {
                    showToast('Scan failed: ' + err.message, true);
                })
                .finally(function() {
                    halScanning = false;
                    renderHalDevices();
                });
        }

        function loadHalDeviceList() {
            fetch('/api/hal/devices')
                .then(function(r) { return r.json(); })
                .then(function(devices) {
                    halDevices = devices;
                    renderHalDevices();
                })
                .catch(function(err) {
                    console.error('Failed to load HAL devices:', err);
                });
        }

        function exportDeviceYaml(slot) {
            var d = null;
            for (var i = 0; i < halDevices.length; i++) {
                if (halDevices[i].slot === slot) { d = halDevices[i]; break; }
            }
            if (!d) return;
            var yaml = deviceToYaml(d);
            var blob = new Blob([yaml], { type: 'text/yaml' });
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = (d.compatible || 'device').replace(/,/g, '_') + '.yaml';
            a.click();
            URL.revokeObjectURL(a.href);
        }

        function importDeviceYaml() {
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.yaml,.yml';
            input.onchange = function() {
                if (!input.files || !input.files[0]) return;
                var reader = new FileReader();
                reader.onload = function(e) {
                    var text = e.target.result;
                    var parsed = parseDeviceYaml(text);
                    if (!parsed || !parsed.compatible) {
                        showToast('Invalid YAML: missing compatible field', true);
                        return;
                    }
                    // Register with server
                    fetch('/api/hal/devices', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(parsed)
                    })
                    .then(function(r) { return r.json(); })
                    .then(function(data) {
                        if (data.status === 'ok') {
                            showToast('Device imported to slot ' + data.slot);
                            loadHalDeviceList();
                        } else {
                            showToast('Import failed: ' + (data.error || ''), true);
                        }
                    })
                    .catch(function(err) { showToast('Import error: ' + err, true); });
                };
                reader.readAsText(input.files[0]);
            };
            input.click();
        }
