        // HAL Devices Management
        // Provides device discovery, configuration, and monitoring UI

        var halDevices = [];
        var halScanning = false;

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
                var d = halDevices[i];
                html += buildHalDeviceCard(d);
            }
            container.innerHTML = html;
        }

        function buildHalDeviceCard(d) {
            var stateClass = 'grey';
            var stateLabel = 'Unknown';
            switch (d.state) {
                case 1: stateClass = 'blue'; stateLabel = 'Detected'; break;
                case 2: stateClass = 'amber'; stateLabel = 'Configuring'; break;
                case 3: stateClass = 'green'; stateLabel = 'Available'; break;
                case 4: stateClass = 'red'; stateLabel = 'Unavailable'; break;
                case 5: stateClass = 'red'; stateLabel = 'Error'; break;
                case 6: stateClass = 'amber'; stateLabel = 'Manual'; break;
                case 7: stateClass = 'grey'; stateLabel = 'Removed'; break;
            }

            var typeLabel = 'Unknown';
            var typeIcon = 'M17,17H7V7H17M21,11V9H19V7C19,5.89 18.1,5 17,5H15V3H13V5H11V3H9V5H7C5.89,5 5,5.89 5,7V9H3V11H5V13H3V15H5V17C5,18.1 5.89,19 7,19H9V21H11V19H13V21H15V19H17C18.1,19 19,18.1 19,17V15H21V13H19V11'; // mdi-chip
            switch (d.type) {
                case 1: typeLabel = 'DAC'; typeIcon = 'M12,3L1,9L12,15L21,10.09V17H23V9M5,13.18V17.18L12,21L19,17.18V13.18L12,17L5,13.18Z'; break;
                case 2: typeLabel = 'ADC'; typeIcon = 'M12,2A3,3 0 0,1 15,5V11A3,3 0 0,1 12,14A3,3 0 0,1 9,11V5A3,3 0 0,1 12,2M19,11C19,14.53 16.39,17.44 13,17.93V21H11V17.93C7.61,17.44 5,14.53 5,11H7A5,5 0 0,0 12,16A5,5 0 0,0 17,11H19Z'; break;
                case 3: typeLabel = 'Codec'; break;
                case 4: typeLabel = 'Amp'; typeIcon = 'M14,3.23V5.29C16.89,6.15 19,8.83 19,12C19,15.17 16.89,17.84 14,18.7V20.77C18,19.86 21,16.28 21,12C21,7.72 18,4.14 14,3.23M16.5,12C16.5,10.23 15.5,8.71 14,7.97V16C15.5,15.29 16.5,13.76 16.5,12M3,9V15H7L12,20V4L7,9H3Z'; break;
                case 5: typeLabel = 'DSP'; break;
                case 6: typeLabel = 'Sensor'; typeIcon = 'M15,13V5A3,3 0 0,0 9,5V13A5,5 0 1,0 15,13M12,4A1,1 0 0,1 13,5V8H11V5A1,1 0 0,1 12,4Z'; break;
            }

            var discLabel = ['Builtin', 'EEPROM', 'GPIO ID', 'Manual', 'Online'][d.discovery] || 'Unknown';

            var html = '<div class="card hal-device-card">';
            html += '<div class="hal-device-header">';
            html += '<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor" aria-hidden="true"><path d="' + typeIcon + '"/></svg>';
            html += '<span class="hal-device-name">' + escapeHtml(d.name || d.compatible) + '</span>';
            html += '<span class="status-dot status-' + stateClass + '"></span>';
            html += '</div>';
            html += '<div class="hal-device-info">';
            html += '<span class="badge badge-' + stateClass + '">' + stateLabel + '</span>';
            html += '<span class="badge">' + typeLabel + '</span>';
            html += '<span class="badge">' + discLabel + '</span>';
            html += '</div>';
            html += '<div class="hal-device-details">';
            if (d.compatible) html += '<div class="hal-detail-row"><span>Compatible:</span><span>' + escapeHtml(d.compatible) + '</span></div>';
            if (d.i2cAddr > 0) html += '<div class="hal-detail-row"><span>I2C Address:</span><span>0x' + d.i2cAddr.toString(16).toUpperCase().padStart(2, '0') + '</span></div>';
            if (d.channels > 0) html += '<div class="hal-detail-row"><span>Channels:</span><span>' + d.channels + '</span></div>';
            html += '<div class="hal-detail-row"><span>Slot:</span><span>' + d.slot + '</span></div>';
            html += '<div class="hal-detail-row"><span>Ready:</span><span>' + (d.ready ? 'Yes' : 'No') + '</span></div>';
            html += '</div>';
            html += '</div>';
            return html;
        }

        function triggerHalRescan() {
            halScanning = true;
            renderHalDevices();
            fetch('/api/hal/scan', { method: 'POST' })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    showToast('Scan complete: ' + (data.devicesFound || 0) + ' devices found', 'success');
                })
                .catch(function(err) {
                    showToast('Scan failed: ' + err.message, 'error');
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
