        // ===== HAL Device Management =====
        // Provides device discovery, configuration, monitoring, and CRUD UI

        // Capability flags (mirrors HAL_CAP_* in hal_types.h)
        var HAL_CAP_HW_VOLUME   = 1 << 0;
        var HAL_CAP_FILTERS     = 1 << 1;
        var HAL_CAP_MUTE        = 1 << 2;
        var HAL_CAP_ADC_PATH    = 1 << 3;
        var HAL_CAP_DAC_PATH    = 1 << 4;
        var HAL_CAP_PGA_CONTROL = 1 << 5;
        var HAL_CAP_HPF_CONTROL = 1 << 6;
        var HAL_CAP_CODEC       = 1 << 7;
        var HAL_CAP_MQA         = 1 << 8;
        var HAL_CAP_LINE_DRIVER = 1 << 9;
        var HAL_CAP_APLL        = 1 << 10;
        var HAL_CAP_DSD         = 1 << 11;
        var HAL_CAP_HP_AMP      = 1 << 12;

        var halDevices = [];
        var halScanning = false;
        var halExpandedSlot = -1;
        var halEditingSlot = -1;
        var halDeviceCount = 0;
        var halDeviceMax = 24;
        var halDriverCount = 0;
        var halDriverMax = 24;

        function handleHalDeviceState(data) {
            halScanning = data.scanning || false;
            halDevices = data.devices || [];
            if (data.deviceCount !== undefined) halDeviceCount = data.deviceCount;
            if (data.deviceMax !== undefined) halDeviceMax = data.deviceMax;
            if (data.driverCount !== undefined) halDriverCount = data.driverCount;
            if (data.driverMax !== undefined) halDriverMax = data.driverMax;
            renderHalDevices();
        }

        function renderHalDevices() {
            var container = document.getElementById('hal-device-list');
            if (!container) return;
            halInitDelegation();  // Ensure delegation is set up when content is rendered

            var scanBtn = document.getElementById('hal-rescan-btn');
            if (scanBtn) {
                scanBtn.disabled = halScanning;
                scanBtn.textContent = halScanning ? 'Scanning...' : 'Rescan Devices';
            }

            // Update capacity indicator
            var capEl = document.getElementById('hal-capacity-indicator');
            if (capEl) {
                var devPct = halDeviceMax > 0 ? (halDeviceCount / halDeviceMax * 100) : 0;
                var drvPct = halDriverMax > 0 ? (halDriverCount / halDriverMax * 100) : 0;
                var warnClass = (devPct >= 80 || drvPct >= 80) ? ' hal-capacity-warn' : '';
                capEl.className = 'hal-capacity' + warnClass;
                capEl.textContent = 'Devices: ' + halDeviceCount + '/' + halDeviceMax +
                    '  Drivers: ' + halDriverCount + '/' + halDriverMax;
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
                case 6: return { cls: 'amber', label: 'Disabled' };
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
            h += '<div class="hal-device-header" data-action="hal-toggle-expand" data-slot="' + d.slot + '">';
            h += '<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor" aria-hidden="true"><path d="' + ti.icon + '"/></svg>';
            h += '<span class="hal-device-name">' + escapeHtml(displayName) + '</span>';
            if (d.type === 6 && d.temperature !== undefined) {
                h += '<span class="hal-temp-reading">' + d.temperature.toFixed(1) + ' &deg;C</span>';
            }
            h += '<span class="status-dot status-' + si.cls + '" title="' + si.label + '"></span>';
            // Icon action buttons — left of toggle, data-stop-propagation so card doesn't expand
            h += '<button class="hal-icon-btn" data-action="hal-start-edit" data-slot="' + d.slot + '" data-stop-propagation="1" title="Edit"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z"/></svg></button>';
            if (d.discovery !== 0) {  // Can't remove builtins
                h += '<button class="hal-icon-btn hal-icon-btn-danger" data-action="hal-confirm-remove" data-slot="' + d.slot + '" data-name="' + escapeHtml(displayName) + '" data-stop-propagation="1" title="Remove device"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z"/></svg></button>';
            }
            h += '<button class="hal-icon-btn" data-action="hal-reinit" data-slot="' + d.slot + '" data-stop-propagation="1" title="Re-initialize"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M17.65,6.35C16.2,4.9 14.21,4 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20C15.73,20 18.84,17.45 19.73,14H17.65C16.83,16.33 14.61,18 12,18A6,6 0 0,1 6,12A6,6 0 0,1 12,6C13.66,6 15.14,6.69 16.22,7.78L13,11H20V4L17.65,6.35Z"/></svg></button>';
            h += '<button class="hal-icon-btn" data-action="hal-export-yaml" data-slot="' + d.slot + '" data-stop-propagation="1" title="Export YAML"><svg viewBox="0 0 24 24" width="15" height="15" fill="currentColor" aria-hidden="true"><path d="M5,20H19V18H5M19,9H15V3H9V9H5L12,16L19,9Z"/></svg></button>';
            // Enable/disable toggle — visible on card without needing to open edit form
            var togChecked = (d.cfgEnabled !== false) ? 'checked' : '';
            h += '<label class="hal-enable-toggle" title="' + (d.cfgEnabled !== false ? 'Enabled — click to disable' : 'Disabled — click to enable') + '" data-stop-propagation="1">';
            h += '<input type="checkbox" ' + togChecked + ' data-action="hal-toggle-enabled" data-slot="' + d.slot + '">';
            h += '<span class="hal-toggle-track"></span></label>';
            h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" class="hal-expand-icon' + (expanded ? ' rotated' : '') + '" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
            h += '</div>';

            // Badge row
            h += '<div class="hal-device-info">';
            h += '<span class="badge badge-' + si.cls + '">' + si.label + '</span>';
            h += '<span class="badge">' + ti.label + '</span>';
            h += '<span class="badge">' + halGetDiscLabel(d.discovery) + '</span>';
            if (d.ready) h += '<span class="badge badge-green">Ready</span>';
            // Capability badges
            if (d.capabilities) {
                if (d.capabilities & HAL_CAP_HW_VOLUME)   h += '<span class="hal-cap-badge">Vol</span>';
                if (d.capabilities & HAL_CAP_MUTE)        h += '<span class="hal-cap-badge">Mute</span>';
                if (d.capabilities & HAL_CAP_PGA_CONTROL) h += '<span class="hal-cap-badge">PGA</span>';
                if (d.capabilities & HAL_CAP_HPF_CONTROL) h += '<span class="hal-cap-badge">HPF</span>';
                if (d.capabilities & HAL_CAP_CODEC)       h += '<span class="hal-cap-badge">Codec</span>';
                if (d.capabilities & HAL_CAP_MQA)         h += '<span class="hal-cap-badge">MQA</span>';
                if (d.capabilities & HAL_CAP_LINE_DRIVER) h += '<span class="hal-cap-badge">Line Driver</span>';
                if (d.capabilities & HAL_CAP_APLL)        h += '<span class="hal-cap-badge">APLL</span>';
                if (d.capabilities & HAL_CAP_DSD)         h += '<span class="hal-cap-badge">DSD</span>';
                if (d.capabilities & HAL_CAP_HP_AMP)      h += '<span class="hal-cap-badge">HP Amp</span>';
            }
            h += '</div>';

            // Error/unavailable diagnostic banner
            h += halBuildErrorBanner(d);

            // Expanded detail section
            if (expanded) {
                h += '<div class="hal-device-details">';

                // Device info
                h += '<div class="hal-detail-row"><span>Compatible:</span><span>' + escapeHtml(d.compatible || '') + '</span></div>';
                if (d.manufacturer) h += '<div class="hal-detail-row"><span>Manufacturer:</span><span>' + escapeHtml(d.manufacturer) + '</span></div>';
                h += '<div class="hal-detail-row"><span>Bus:</span><span>' + halGetBusLabel(d.busType || 0) + (d.busIndex > 0 ? ' #' + d.busIndex : '') + '</span></div>';
                if (d.i2cAddr > 0) h += '<div class="hal-detail-row"><span>I2C Address:</span><span>0x' + d.i2cAddr.toString(16).toUpperCase().padStart(2, '0') + '</span></div>';
                if (d.busFreq > 0) h += '<div class="hal-detail-row"><span>Bus Freq:</span><span>' + (d.busFreq >= 1000000 ? (d.busFreq/1000000).toFixed(1) + ' MHz' : (d.busFreq/1000) + ' kHz') + '</span></div>';
                if (d.pinA > 0) {
                    var pinALabel = (d.busType === 1) ? 'Pin A (SDA):' : 'Pin A (Data):';
                    h += '<div class="hal-detail-row"><span>' + pinALabel + '</span><span>GPIO ' + d.pinA + '</span></div>';
                }
                if (d.pinB > 0) {
                    var pinBLabel = (d.busType === 1) ? 'Pin B (SCL):' : 'Pin B (CLK):';
                    h += '<div class="hal-detail-row"><span>' + pinBLabel + '</span><span>GPIO ' + d.pinB + '</span></div>';
                }
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
                    if (d.capabilities & 32) caps.push('PGA Control');
                    if (d.capabilities & 64) caps.push('HPF Control');
                    if (d.capabilities & 128) caps.push('Codec');
                    if (d.capabilities & 256) caps.push('MQA');
                    if (d.capabilities & 512) caps.push('Line Driver');
                    if (d.capabilities & 1024) caps.push('APLL');
                    if (d.capabilities & 2048) caps.push('DSD');
                    if (d.capabilities & 4096) caps.push('HP Amp');
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
                    if (d.sampleRates & 64) rates.push('384k');
                    if (d.sampleRates & 128) rates.push('768k');
                    h += '<div class="hal-detail-row"><span>Sample Rates:</span><span>' + rates.join(', ') + '</span></div>';
                }

                // Edit form
                if (editing) {
                    h += halBuildEditForm(d);
                }


                h += '</div>'; // hal-device-details
            }

            h += '</div>'; // card
            return h;
        }

        function halBuildErrorBanner(d) {
            // Show banner only for ERROR (5) or UNAVAILABLE (4) with a reason
            var reason = d.errorReason || d.error;
            if ((d.state !== 5 && d.state !== 4) || !reason) return '';
            var isError = (d.state === 5);
            var bannerCls = isError ? 'hal-error-banner hal-error-banner-error' : 'hal-error-banner hal-error-banner-warn';
            var tips = halGetErrorTips(d);

            var h = '<div class="' + bannerCls + '" role="alert">';
            h += '<div class="hal-error-banner-header">';
            h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg>';
            h += '<span class="hal-error-reason">' + escapeHtml(reason) + '</span>';
            h += '</div>';

            if (tips.length > 0) {
                h += '<div class="hal-error-tips-toggle" tabindex="0" role="button" aria-expanded="false" data-action="hal-toggle-tips" data-slot="' + d.slot + '">';
                h += '<svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true" class="hal-tips-chevron"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
                h += ' Troubleshooting tips</div>';
                h += '<div class="hal-error-tips" id="hal-error-tips-' + d.slot + '" style="display:none">';
                h += '<ul>';
                for (var i = 0; i < tips.length; i++) {
                    h += '<li>' + escapeHtml(tips[i]) + '</li>';
                }
                h += '</ul></div>';
            }
            h += '</div>';
            return h;
        }

        function halGetErrorTips(d) {
            var tips = [];
            var busType = d.busType || 0;
            if (busType === 1) {
                // I2C
                tips.push('Check I2C wiring: SDA and SCL connections');
                tips.push('Verify pull-up resistors on SDA/SCL lines (4.7k typical)');
                tips.push('Confirm I2C address matches device configuration');
                if (d.busIndex === 0) {
                    tips.push('Bus 0 shares GPIO 48/54 with WiFi SDIO — disable WiFi or use a different bus');
                }
            } else if (busType === 2) {
                // I2S
                tips.push('Check I2S pin assignments (BCK, LRC, DATA, MCLK)');
                tips.push('Verify clock source and sample rate configuration');
                tips.push('Ensure no I2S port conflict with another device');
            } else if (busType === 4) {
                // GPIO
                tips.push('Verify GPIO pin number is correct and not claimed by another device');
                tips.push('Check for pin conflicts in the HAL pin table');
            }
            tips.push('Try re-initializing the device (use the \u21bb button)');
            tips.push('Check the Debug Console for detailed error logs');
            return tips;
        }

        function halToggleErrorTips(slot) {
            var el = document.getElementById('hal-error-tips-' + slot);
            if (!el) return;
            var visible = el.style.display !== 'none';
            el.style.display = visible ? 'none' : 'block';
            // Update aria-expanded on the toggle
            var toggle = el.previousElementSibling;
            if (toggle && toggle.classList.contains('hal-error-tips-toggle')) {
                toggle.setAttribute('aria-expanded', String(!visible));
                var chevron = toggle.querySelector('.hal-tips-chevron');
                if (chevron) {
                    chevron.style.transform = visible ? '' : 'rotate(180deg)';
                }
            }
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

            // Audio Config section (for audio devices: DAC, ADC, Codec)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Audio Config</div>';
                h += '<div class="hal-form-row"><label>Sample Rate:</label>';
                h += '<select id="halCfgSampleRate">';
                h += '<option value="0"' + (!(d.cfgSampleRate) || d.cfgSampleRate === 0 ? ' selected' : '') + '>Auto</option>';
                h += '<option value="44100"' + (d.cfgSampleRate === 44100 ? ' selected' : '') + '>44100 Hz</option>';
                h += '<option value="48000"' + (d.cfgSampleRate === 48000 ? ' selected' : '') + '>48000 Hz</option>';
                h += '<option value="96000"' + (d.cfgSampleRate === 96000 ? ' selected' : '') + '>96000 Hz</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>Bit Depth:</label>';
                h += '<select id="halCfgBitDepth">';
                h += '<option value="0"' + (!(d.cfgBitDepth) || d.cfgBitDepth === 0 ? ' selected' : '') + '>Auto</option>';
                h += '<option value="16"' + (d.cfgBitDepth === 16 ? ' selected' : '') + '>16-bit</option>';
                h += '<option value="24"' + (d.cfgBitDepth === 24 ? ' selected' : '') + '>24-bit</option>';
                h += '<option value="32"' + (d.cfgBitDepth === 32 ? ' selected' : '') + '>32-bit</option>';
                h += '</select></div>';
            }

            // Advanced section (all audio devices)
            if (d.type >= 1 && d.type <= 3) {
                h += '<div class="hal-form-section">Advanced</div>';

                h += '<div class="hal-form-row"><label>MCLK Multiple:</label>';
                h += '<select id="halCfgMclkMultiple">';
                h += '<option value="0"' + (!d.cfgMclkMultiple || d.cfgMclkMultiple === 0 ? ' selected' : '') + '>Auto (256×)</option>';
                h += '<option value="256"' + (d.cfgMclkMultiple === 256 ? ' selected' : '') + '>256×</option>';
                h += '<option value="384"' + (d.cfgMclkMultiple === 384 ? ' selected' : '') + '>384×</option>';
                h += '<option value="512"' + (d.cfgMclkMultiple === 512 ? ' selected' : '') + '>512×</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>I2S Format:</label>';
                h += '<select id="halCfgI2sFormat">';
                h += '<option value="0"' + (d.cfgI2sFormat === 0 || d.cfgI2sFormat === undefined ? ' selected' : '') + '>Philips (I2S)</option>';
                h += '<option value="1"' + (d.cfgI2sFormat === 1 ? ' selected' : '') + '>MSB / Left-Justified</option>';
                h += '<option value="2"' + (d.cfgI2sFormat === 2 ? ' selected' : '') + '>LSB / Right-Justified</option>';
                h += '</select></div>';

                h += '<div class="hal-form-row"><label>PA Control Pin:</label>';
                h += '<input type="number" id="halCfgPaControlPin" value="' + (d.cfgPaControlPin !== undefined ? d.cfgPaControlPin : -1) + '" min="-1" max="53" style="width:60px;">';
                h += ' <span style="font-size:10px;opacity:0.5">(-1 = none)</span></div>';

                if (d.capabilities & HAL_CAP_PGA_CONTROL) {
                    h += '<div class="hal-form-row"><label>PGA Gain:</label>';
                    h += '<select id="halCfgPgaGain">';
                    for (var pi = 0; pi <= 42; pi += 6) {
                        h += '<option value="' + pi + '"' + (d.cfgPgaGain === pi ? ' selected' : '') + '>' + pi + ' dB</option>';
                    }
                    h += '</select></div>';
                }

                if (d.capabilities & HAL_CAP_HPF_CONTROL) {
                    h += '<div class="hal-form-row"><label>High-Pass Filter:</label>';
                    h += '<label class="toggle-label"><input type="checkbox" id="halCfgHpfEnabled"' + (d.cfgHpfEnabled ? ' checked' : '') + '> <span>Enabled</span></label>';
                    h += '</div>';
                }

                if (d.type === 2) {
                    var filterPresets = [
                        'Minimum Phase',
                        'Linear Apodizing Fast',
                        'Linear Fast',
                        'Linear Fast Low Ripple',
                        'Linear Slow',
                        'Minimum Fast',
                        'Minimum Slow',
                        'Minimum Slow Low Dispersion'
                    ];
                    h += '<div class="hal-form-row"><label>Filter Preset:</label>';
                    h += '<select id="halCfgFilterPreset">';
                    for (var fi = 0; fi < filterPresets.length; fi++) {
                        h += '<option value="' + fi + '"' + (d.cfgFilterMode === fi ? ' selected' : '') + '>' + filterPresets[fi] + '</option>';
                    }
                    h += '</select></div>';
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

            // Pin configuration
            if (busType === 1 || busType === 2) {
                h += '<div class="hal-form-section">Pin Configuration <span style="font-size:10px;opacity:0.5">(-1 = board default)</span></div>';
                if (busType === 1) {
                    h += '<div class="hal-form-row"><label>SDA Pin:</label>';
                    h += '<input type="number" id="halCfgPinSda" value="' + (d.cfgPinSda !== undefined ? d.cfgPinSda : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>SCL Pin:</label>';
                    h += '<input type="number" id="halCfgPinScl" value="' + (d.cfgPinScl !== undefined ? d.cfgPinScl : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                }
                if (busType === 2) {
                    h += '<div class="hal-form-row"><label>Data Pin:</label>';
                    h += '<input type="number" id="halCfgPinData" value="' + (d.cfgPinData !== undefined ? d.cfgPinData : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>MCLK Pin:</label>';
                    h += '<input type="number" id="halCfgPinMclk" value="' + (d.cfgPinMclk !== undefined ? d.cfgPinMclk : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>BCK Pin:</label>';
                    h += '<input type="number" id="halCfgPinBck" value="' + (d.cfgPinBck !== undefined ? d.cfgPinBck : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    h += '<div class="hal-form-row"><label>LRC/WS Pin:</label>';
                    h += '<input type="number" id="halCfgPinLrc" value="' + (d.cfgPinLrc !== undefined ? d.cfgPinLrc : -1) + '" min="-1" max="54" style="width:60px;"></div>';
                    if (d.type === 2) {  // ADC only — FMT selects Philips vs MSB format
                        h += '<div class="hal-form-row"><label>FMT Pin:</label>';
                        h += '<input type="number" id="halCfgPinFmt" value="' + (d.cfgPinFmt !== undefined ? d.cfgPinFmt : -1) + '" min="-1" max="54" style="width:60px;">';
                        h += ' <span style="font-size:10px;opacity:0.5">(-1 = not wired; set HIGH for MSB, LOW for Philips)</span></div>';
                    }
                }
                h += '<div style="font-size:11px;opacity:0.55;margin-top:4px;padding:0 2px">Changes apply immediately on Save. I2S devices will have a brief audio dropout.</div>';
            }

            // Save/Cancel
            h += '<div class="hal-form-buttons">';
            h += '<button class="btn btn-primary btn-sm" data-action="hal-save-config" data-slot="' + d.slot + '">Save</button>';
            h += '<button class="btn btn-sm" data-action="hal-cancel-edit">Cancel</button>';
            h += '</div>';

            h += '</div>';
            return h;
        }

        function halToggleExpand(slot) {
            halExpandedSlot = (halExpandedSlot === slot) ? -1 : slot;
            halEditingSlot = -1;
            renderHalDevices();
        }

        function halToggleDeviceEnabled(slot, enabled) {
            apiFetch('/api/hal/devices', {
                method: 'PUT',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({slot: slot, enabled: enabled})
            }).then(function(r) {
                if (r.ok) loadHalDeviceList();
            });
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

            cfg.pinData = parseInt(document.getElementById('halCfgPinData') ? document.getElementById('halCfgPinData').value : '-1') || -1;
            cfg.pinMclk = parseInt(document.getElementById('halCfgPinMclk') ? document.getElementById('halCfgPinMclk').value : '-1') || -1;

            var pinAEl = document.getElementById('halCfgPinA');
            if (pinAEl) cfg.pinSda = parseInt(pinAEl.value);

            // Audio config fields (key names match backend PUT handler)
            var srEl = document.getElementById('halCfgSampleRate');
            if (srEl) cfg.sampleRate = parseInt(srEl.value) || 0;
            var bdEl = document.getElementById('halCfgBitDepth');
            if (bdEl) cfg.bitDepth = parseInt(bdEl.value) || 0;
            var mclkEl = document.getElementById('halCfgMclkMultiple');
            if (mclkEl) cfg.cfgMclkMultiple = parseInt(mclkEl.value) || 0;
            var i2sfEl = document.getElementById('halCfgI2sFormat');
            if (i2sfEl) cfg.cfgI2sFormat = parseInt(i2sfEl.value) || 0;
            var pgaEl = document.getElementById('halCfgPgaGain');
            if (pgaEl) cfg.cfgPgaGain = parseInt(pgaEl.value) || 0;
            var hpfEl = document.getElementById('halCfgHpfEnabled');
            if (hpfEl) cfg.cfgHpfEnabled = hpfEl.checked;
            var filterPresetEl = document.getElementById('halCfgFilterPreset');
            if (filterPresetEl) cfg.filterMode = parseInt(filterPresetEl.value) || 0;
            var paEl = document.getElementById('halCfgPaControlPin');
            if (paEl) cfg.cfgPaControlPin = parseInt(paEl.value);
            // New I2S pin fields
            var pinBckEl = document.getElementById('halCfgPinBck');
            if (pinBckEl) cfg.pinBck = parseInt(pinBckEl.value);
            var pinLrcEl = document.getElementById('halCfgPinLrc');
            if (pinLrcEl) cfg.pinLrc = parseInt(pinLrcEl.value);
            var pinFmtEl = document.getElementById('halCfgPinFmt');
            if (pinFmtEl) cfg.pinFmt = parseInt(pinFmtEl.value);

            apiFetch('/api/hal/devices', {
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
            apiFetch('/api/hal/devices/reinit', {
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

        function halConfirmRemove(slot, name) {
            if (!confirm('Remove "' + name + '"?\nThe device can be re-added later via Add Device.')) return;
            halRemoveDevice(slot);
        }

        function halRemoveDevice(slot) {
            apiFetch('/api/hal/devices', {
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
            apiFetch('/api/hal/db/presets')
                .then(function(r) {
                    if (!r.ok) throw new Error('Server error ' + r.status);
                    return r.json();
                })
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

            apiFetch('/api/hal/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ compatible: sel.value })
            })
            .then(function(r) {
                if (!r.ok) return r.json().then(function(d) { throw new Error(d.error || ('HTTP ' + r.status)); });
                return r.json();
            })
            .then(function(data) {
                showToast('Device registered in slot ' + data.slot);
                loadHalDeviceList();
            })
            .catch(function(err) { showToast('Registration failed: ' + err, true); });
        }

        function triggerHalRescan() {
            if (halScanning) return;  // Prevent double-click
            halScanning = true;
            renderHalDevices();
            apiFetch('/api/hal/scan', { method: 'POST' })
                .then(function(r) {
                    if (r.status === 409) {
                        showToast('Scan already in progress');
                        return null;
                    }
                    return r.json();
                })
                .then(function(data) {
                    if (data) {
                        var msg = 'Scan complete: ' + (data.devicesFound || 0) + ' devices found';
                        if (data.partialScan) msg += ' (Bus 0 skipped — WiFi SDIO conflict)';
                        showToast(msg, data.partialScan);
                    }
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
            apiFetch('/api/hal/devices')
                .then(function(r) { return r.json(); })
                .then(function(devices) {
                    halDevices = devices;
                    renderHalDevices();
                })
                .catch(function(err) {
                    console.error('Failed to load HAL devices:', err);
                    showToast('Failed to load device list: ' + err.message, 'error');
                });
            loadHalSettings();
        }

        function loadHalSettings() {
            apiFetch('/api/hal/settings')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    var cb = document.getElementById('halAutoDiscovery');
                    if (cb) cb.checked = d.halAutoDiscovery !== false;
                })
                .catch(function(err) { console.warn('[HAL] Failed to load settings:', err); });
        }

        function setHalAutoDiscovery(enabled) {
            apiFetch('/api/hal/settings', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ halAutoDiscovery: enabled })
            })
            .then(function(r) { return r.json(); })
            .then(function() { showToast('Auto-discovery ' + (enabled ? 'enabled' : 'disabled'), 'success'); })
            .catch(function(err) { showToast('Failed: ' + err.message, 'error'); });
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
                    apiFetch('/api/hal/devices', {
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

        var halUnknownDevices = [];

        function handleHalUnknownDevices(devices) {
            halUnknownDevices = devices || [];
            renderHalUnknownDevices();
        }

        // ===== Custom Device Upload =====

        var halCustomFileContent = null;

        function halOpenCustomUpload() {
            var modal = document.getElementById('halCustomUploadModal');
            if (modal) modal.style.display = 'flex';
            var preview = document.getElementById('halCustomPreview');
            if (preview) preview.style.display = 'none';
            var btn = document.getElementById('halCustomUploadBtn');
            if (btn) btn.disabled = true;
            halCustomFileContent = null;
        }

        function halCloseCustomUpload() {
            var modal = document.getElementById('halCustomUploadModal');
            if (modal) modal.style.display = 'none';
        }

        function halCustomFileSelected(input) {
            var file = input.files[0];
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function(e) {
                try {
                    var schema = JSON.parse(e.target.result);
                    halCustomFileContent = e.target.result;
                    var preview = document.getElementById('halCustomPreview');
                    if (preview) {
                        preview.style.display = 'block';
                        preview.innerHTML = '<strong>' + escapeHtml(schema.name || schema.compatible || 'Unknown') + '</strong><br>' +
                            'Compatible: ' + escapeHtml(schema.compatible || '—') + '<br>' +
                            'Component: ' + escapeHtml(schema.component || '—') + '<br>' +
                            'Bus: ' + escapeHtml(schema.bus || '—');
                    }
                    var btn = document.getElementById('halCustomUploadBtn');
                    if (btn) btn.disabled = false;
                } catch(err) {
                    showToast('Invalid JSON file', true);
                }
            };
            reader.readAsText(file);
        }

        function halUploadCustomDevice() {
            if (!halCustomFileContent) return;
            apiFetch('/api/hal/devices/custom', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: halCustomFileContent
            })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.ok) {
                    showToast('Custom device uploaded');
                    halCloseCustomUpload();
                    loadHalDeviceList();
                } else {
                    showToast('Upload failed: ' + (data.error || 'Unknown error'), true);
                }
            })
            .catch(function(err) { showToast('Upload error: ' + err, true); });
        }

        function renderHalUnknownDevices() {
            var container = document.getElementById('hal-unknown-list');
            if (!container) return;
            if (!halUnknownDevices || halUnknownDevices.length === 0) {
                container.innerHTML = '';
                return;
            }
            var h = '<div style="margin-top:8px;font-size:12px;font-weight:500;opacity:0.7;">Unidentified Devices</div>';
            for (var i = 0; i < halUnknownDevices.length; i++) {
                var u = halUnknownDevices[i];
                h += '<div class="card" style="border-left:3px solid #f90;margin-top:8px;">';
                h += '<div class="card-title" style="display:flex;align-items:center;gap:8px;">';
                h += '<svg viewBox="0 0 24 24" width="16" height="16" fill="#f90" aria-hidden="true"><path d="M13 14H11V9H13M13 18H11V16H13M1 21H23L12 2L1 21Z"/></svg>';
                h += 'Unknown Device</div>';
                h += '<div class="info-row"><span class="info-label">I2C Address</span><span class="info-value">0x' + ((u.i2cAddr || u.i2cAddress || 0)).toString(16).toUpperCase().padStart(2,'0') + '</span></div>';
                if (u.deviceName) h += '<div class="info-row"><span class="info-label">Partial Name</span><span class="info-value">' + escapeHtml(u.deviceName) + '</span></div>';
                h += '<div style="font-size:11px;opacity:0.55;margin-top:6px;">No driver match found. Program via EEPROM Programming, then Rescan.</div>';
                h += '</div>';
            }
            container.innerHTML = h;
        }

        // ===== Custom Device Create Modal =====

        var halCcRegCount = 0;
        var halCcSelectedAddr = null;
        var halCcSelectedBus = -1;

        function halOpenCustomCreate() {
            var modal = document.getElementById('halCustomCreateModal');
            if (modal) modal.classList.add('active');
            // Reset form
            var nameEl = document.getElementById('halCcName');
            if (nameEl) nameEl.value = '';
            var typeEl = document.getElementById('halCcType');
            if (typeEl) typeEl.value = '1';
            var busEl = document.getElementById('halCcBus');
            if (busEl) busEl.value = '1';
            var addrEl = document.getElementById('halCcI2cAddr');
            if (addrEl) addrEl.value = '';
            var i2cBusEl = document.getElementById('halCcI2cBus');
            if (i2cBusEl) i2cBusEl.value = '2';
            var portEl = document.getElementById('halCcI2sPort');
            if (portEl) portEl.value = '2';
            var chEl = document.getElementById('halCcChannels');
            if (chEl) chEl.value = '2';
            halCcRegCount = 0;
            halCcSelectedAddr = null;
            halCcSelectedBus = -1;
            var regBody = document.getElementById('halCcRegBody');
            if (regBody) regBody.innerHTML = '';
            halCcTypeChanged();
            halCcBusChanged();
            halCcUpdateCompat();
            halCcFetchUnmatched();
        }

        function halCloseCustomCreate() {
            var modal = document.getElementById('halCustomCreateModal');
            if (modal) modal.classList.remove('active');
        }

        function halCcFetchUnmatched() {
            var container = document.getElementById('halCcAddrChips');
            if (!container) return;
            container.innerHTML = '<span class="hal-cc-chip-skip">Loading...</span>';
            apiFetch('/api/hal/scan/unmatched')
                .then(function(r) {
                    if (!r.ok) throw new Error('HTTP ' + r.status);
                    return r.json();
                })
                .then(function(data) {
                    var addrs = data.addresses || data || [];
                    if (!addrs.length) {
                        container.innerHTML = '<span class="hal-cc-chip-skip">No unmatched addresses found. Run Rescan first.</span>';
                        return;
                    }
                    var h = '';
                    for (var i = 0; i < addrs.length; i++) {
                        var item = addrs[i];
                        var addr = typeof item === 'object' ? (item.address || item.addr || 0) : item;
                        var bus = typeof item === 'object' ? (item.bus !== undefined ? item.bus : 2) : 2;
                        var addrHex = '0x' + addr.toString(16).toUpperCase().padStart(2, '0');
                        h += '<button class="hal-cc-chip" data-action="hal-cc-select-addr" data-addr="' + addr + '" data-bus="' + bus + '">';
                        h += addrHex;
                        h += '<span class="hal-cc-chip-bus">Bus ' + bus + '</span>';
                        h += '</button>';
                    }
                    // Show skipped buses
                    if (data.skippedBuses && data.skippedBuses.length > 0) {
                        for (var s = 0; s < data.skippedBuses.length; s++) {
                            h += '<span class="hal-cc-chip-skip">Bus ' + data.skippedBuses[s] + ' skipped (WiFi active)</span>';
                        }
                    }
                    container.innerHTML = h;
                })
                .catch(function() {
                    container.innerHTML = '<span class="hal-cc-chip-skip">Could not fetch addresses. Run Rescan and try again.</span>';
                });
        }

        function halCcSelectAddr(addr, bus, el) {
            // Deselect previous
            var chips = document.querySelectorAll('#halCcAddrChips .hal-cc-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.remove('selected');
            }
            // Select this one
            if (el) el.classList.add('selected');
            halCcSelectedAddr = addr;
            halCcSelectedBus = bus;
            // Auto-fill I2C fields
            var addrEl = document.getElementById('halCcI2cAddr');
            if (addrEl) addrEl.value = '0x' + addr.toString(16).toUpperCase().padStart(2, '0');
            var busEl = document.getElementById('halCcI2cBus');
            if (busEl) busEl.value = String(bus);
            // Switch bus type to I2C
            var mainBusEl = document.getElementById('halCcBus');
            if (mainBusEl) mainBusEl.value = '1';
            halCcBusChanged();
        }

        function halCcTypeChanged() {
            var typeEl = document.getElementById('halCcType');
            var type = typeEl ? parseInt(typeEl.value) : 1;
            // Show/hide audio section for DAC(1), ADC(2), Codec(3)
            var audioSection = document.getElementById('halCcAudioSection');
            if (audioSection) audioSection.style.display = (type >= 1 && type <= 3) ? '' : 'none';
            halCcBuildCapCheckboxes(type);
            halCcUpdateCompat();
        }

        function halCcBusChanged() {
            var busEl = document.getElementById('halCcBus');
            var bus = busEl ? parseInt(busEl.value) : 1;
            var i2cSection = document.getElementById('halCcI2cSection');
            if (i2cSection) i2cSection.style.display = (bus === 1) ? '' : 'none';
        }

        function halCcUpdateCompat() {
            var nameEl = document.getElementById('halCcName');
            var name = nameEl ? nameEl.value.trim() : '';
            var slug = name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
            var compat = 'custom,' + (slug || 'my-device');
            var display = document.getElementById('halCcCompatDisplay');
            if (display) display.textContent = compat;
        }

        function halCcAddInitReg() {
            var body = document.getElementById('halCcRegBody');
            if (!body) return;
            var idx = halCcRegCount++;
            var tr = document.createElement('tr');
            tr.id = 'halCcReg' + idx;
            tr.innerHTML = '<td><input type="text" id="halCcRegAddr' + idx + '" placeholder="0x00" value="0x"></td>' +
                '<td><input type="text" id="halCcRegVal' + idx + '" placeholder="0x00" value="0x"></td>' +
                '<td><button class="hal-cc-regdel" data-action="hal-cc-remove-reg" data-idx="' + idx + '" title="Remove">&times;</button></td>';
            body.appendChild(tr);
        }

        function halCcRemoveInitReg(idx) {
            var row = document.getElementById('halCcReg' + idx);
            if (row) row.remove();
        }

        function halCcBuildCapCheckboxes(type) {
            var container = document.getElementById('halCcCaps');
            if (!container) return;
            var caps = [
                { bit: 4, label: 'DAC Output', defOn: (type === 1 || type === 3) },
                { bit: 3, label: 'ADC Input', defOn: (type === 2 || type === 3) },
                { bit: 7, label: 'Codec', defOn: (type === 3) },
                { bit: 0, label: 'HW Volume', defOn: false },
                { bit: 1, label: 'Filters', defOn: false },
                { bit: 2, label: 'Mute', defOn: false },
                { bit: 5, label: 'PGA Control', defOn: false },
                { bit: 6, label: 'HPF Control', defOn: false },
                { bit: 8, label: 'MQA', defOn: false },
                { bit: 9, label: 'Line Driver', defOn: false },
                { bit: 10, label: 'APLL', defOn: false },
                { bit: 11, label: 'DSD', defOn: false },
                { bit: 12, label: 'HP Amp', defOn: false }
            ];
            var h = '';
            for (var i = 0; i < caps.length; i++) {
                var c = caps[i];
                h += '<label><input type="checkbox" value="' + c.bit + '" class="halCcCapCheck"' + (c.defOn ? ' checked' : '') + '> ' + escapeHtml(c.label) + '</label>';
            }
            container.innerHTML = h;
        }

        function halCcCollectCaps() {
            var checks = document.querySelectorAll('.halCcCapCheck:checked');
            var val = 0;
            for (var i = 0; i < checks.length; i++) {
                val |= (1 << parseInt(checks[i].value));
            }
            return val;
        }

        function halCcCollectInitRegs() {
            var regs = [];
            var body = document.getElementById('halCcRegBody');
            if (!body) return regs;
            var rows = body.querySelectorAll('tr');
            for (var i = 0; i < rows.length; i++) {
                var regInput = rows[i].querySelector('input[id^="halCcRegAddr"]');
                var valInput = rows[i].querySelector('input[id^="halCcRegVal"]');
                if (!regInput || !valInput) continue;
                var reg = parseInt(regInput.value, 16);
                var val = parseInt(valInput.value, 16);
                if (isNaN(reg) || isNaN(val)) continue;
                if (reg < 0 || reg > 255 || val < 0 || val > 255) continue;
                regs.push({ reg: reg, val: val });
            }
            return regs;
        }

        function halCcValidate() {
            var nameEl = document.getElementById('halCcName');
            var name = nameEl ? nameEl.value.trim() : '';
            if (!name) { showToast('Device name is required', true); return null; }
            if (name.length > 32) { showToast('Name must be 32 characters or less', true); return null; }

            var typeEl = document.getElementById('halCcType');
            var type = typeEl ? parseInt(typeEl.value) : 1;

            var busEl = document.getElementById('halCcBus');
            var busType = busEl ? parseInt(busEl.value) : 1;

            var i2cAddr = 0;
            var i2cBus = 2;
            if (busType === 1) {
                var addrEl = document.getElementById('halCcI2cAddr');
                var addrStr = addrEl ? addrEl.value.trim() : '';
                i2cAddr = parseInt(addrStr, 16);
                if (isNaN(i2cAddr) || i2cAddr < 0x08 || i2cAddr > 0x77) {
                    showToast('I2C address must be hex 0x08-0x77', true);
                    return null;
                }
                var i2cBusEl = document.getElementById('halCcI2cBus');
                i2cBus = i2cBusEl ? parseInt(i2cBusEl.value) : 2;
            }

            var i2sPort = 2;
            var channels = 2;
            if (type >= 1 && type <= 3) {
                var portEl = document.getElementById('halCcI2sPort');
                i2sPort = portEl ? parseInt(portEl.value) : 2;
                var chEl = document.getElementById('halCcChannels');
                channels = chEl ? parseInt(chEl.value) : 2;
            }

            var caps = halCcCollectCaps();
            if ((type >= 1 && type <= 3) && caps === 0) {
                showToast('Select at least one capability for audio devices', true);
                return null;
            }

            // Validate init registers
            var body = document.getElementById('halCcRegBody');
            if (body) {
                var rows = body.querySelectorAll('tr');
                for (var i = 0; i < rows.length; i++) {
                    var regInput = rows[i].querySelector('input[id^="halCcRegAddr"]');
                    var valInput = rows[i].querySelector('input[id^="halCcRegVal"]');
                    if (!regInput || !valInput) continue;
                    var rv = parseInt(regInput.value, 16);
                    var vv = parseInt(valInput.value, 16);
                    if (isNaN(rv) || rv < 0 || rv > 255) {
                        showToast('Init register address must be hex 0x00-0xFF', true);
                        return null;
                    }
                    if (isNaN(vv) || vv < 0 || vv > 255) {
                        showToast('Init register value must be hex 0x00-0xFF', true);
                        return null;
                    }
                }
            }

            var slug = name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
            var compatible = 'custom,' + (slug || 'my-device');

            return {
                name: name,
                compatible: compatible,
                type: type,
                busType: busType,
                i2cAddr: i2cAddr,
                i2cBus: i2cBus,
                i2sPort: i2sPort,
                channels: channels,
                capabilities: caps,
                initRegs: halCcCollectInitRegs()
            };
        }

        function halSubmitCustomCreate(event) {
            if (event) event.preventDefault();
            var data = halCcValidate();
            if (!data) return;

            apiFetch('/api/hal/devices/custom/create', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(function(r) { return r.json(); })
            .then(function(result) {
                if (result.ok || result.status === 'ok') {
                    showToast('Device created in slot ' + (result.slot !== undefined ? result.slot : '?'));
                    halCloseCustomCreate();
                    loadHalDeviceList();
                } else {
                    showToast('Create failed: ' + (result.error || 'Unknown error'), true);
                }
            })
            .catch(function(err) { showToast('Error: ' + err, true); });
        }

        function halCcBuildExportJson() {
            var data = halCcValidate();
            if (!data) return null;
            return JSON.stringify(data, null, 2);
        }

        function halExportCustomSchema() {
            var json = halCcBuildExportJson();
            if (!json) return;
            var data = JSON.parse(json);
            var filename = (data.compatible || 'custom-device').replace(/,/g, '_') + '.json';
            var blob = new Blob([json], { type: 'application/json' });
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = filename;
            a.click();
            URL.revokeObjectURL(a.href);
        }

        function halSubmitToAlx() {
            var json = halCcBuildExportJson();
            if (!json) return;
            var data = JSON.parse(json);
            // Download the file
            var filename = (data.compatible || 'custom-device').replace(/,/g, '_') + '.json';
            var blob = new Blob([json], { type: 'application/json' });
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = filename;
            a.click();
            URL.revokeObjectURL(a.href);
            // Open GitHub issue with pre-filled template
            var title = encodeURIComponent('Custom Device: ' + (data.name || 'Unknown'));
            var body = encodeURIComponent('## Custom Device Schema\n\nPlease find the attached JSON schema file: `' + filename + '`\n\n### Device Info\n- **Name**: ' + data.name + '\n- **Compatible**: ' + data.compatible + '\n- **Type**: ' + data.type + '\n- **Bus**: ' + data.busType + '\n\n### Notes\n\n(Describe your device and how it should be used)\n');
            var url = 'https://github.com/ALX-Audio/ALX_Nova_Controller_2/issues/new?title=' + title + '&body=' + body + '&labels=custom-device';
            window.open(url, '_blank');
        }

        // ===== Event Delegation for HAL Device List =====
        // Set up once on the devices tab to handle all dynamically rendered hal cards and custom device UI
        function halInitDelegation() {
            var tabEl = document.getElementById('devices');
            if (!tabEl || tabEl.dataset.halDelegationInit) return;
            tabEl.dataset.halDelegationInit = '1';

            tabEl.addEventListener('click', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                var action = el.dataset.action;

                // Stop propagation for icon buttons inside headers
                if (el.dataset.stopPropagation) {
                    e.stopPropagation();
                }

                if (action === 'hal-toggle-expand') {
                    halToggleExpand(parseInt(el.dataset.slot));
                } else if (action === 'hal-start-edit') {
                    e.stopPropagation();
                    halStartEdit(parseInt(el.dataset.slot));
                } else if (action === 'hal-confirm-remove') {
                    e.stopPropagation();
                    halConfirmRemove(parseInt(el.dataset.slot), el.dataset.name);
                } else if (action === 'hal-reinit') {
                    e.stopPropagation();
                    halReinitDevice(parseInt(el.dataset.slot));
                } else if (action === 'hal-export-yaml') {
                    e.stopPropagation();
                    exportDeviceYaml(parseInt(el.dataset.slot));
                } else if (action === 'hal-save-config') {
                    halSaveConfig(parseInt(el.dataset.slot));
                } else if (action === 'hal-cancel-edit') {
                    halCancelEdit();
                } else if (action === 'hal-toggle-tips') {
                    halToggleErrorTips(parseInt(el.dataset.slot));
                } else if (action === 'hal-cc-select-addr') {
                    halCcSelectAddr(parseInt(el.dataset.addr), parseInt(el.dataset.bus), el);
                } else if (action === 'hal-cc-remove-reg') {
                    halCcRemoveInitReg(parseInt(el.dataset.idx));
                }
            });

            // Handle keyboard for tips toggle (accessibility)
            tabEl.addEventListener('keydown', function(e) {
                var el = e.target.closest('[data-action="hal-toggle-tips"]');
                if (!el) return;
                if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    halToggleErrorTips(parseInt(el.dataset.slot));
                }
            });

            // Handle the enable toggle checkbox change (needs to stop propagation on label click)
            tabEl.addEventListener('change', function(e) {
                var el = e.target.closest('[data-action]');
                if (!el) return;
                if (el.dataset.action === 'hal-toggle-enabled') {
                    halToggleDeviceEnabled(parseInt(el.dataset.slot), el.checked);
                }
            });

            // Stop propagation on labels with data-stop-propagation so header click-expand doesn't trigger
            tabEl.addEventListener('click', function(e) {
                var label = e.target.closest('label[data-stop-propagation]');
                if (label) e.stopPropagation();
            }, true);  // capture phase to intercept before the header handler
        }

        // Initialize delegation on DOMContentLoaded
        document.addEventListener('DOMContentLoaded', function() {
            halInitDelegation();
        });
