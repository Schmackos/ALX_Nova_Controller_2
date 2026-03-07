        // ===== Health Dashboard =====
        // Phase 8 of Debug Architecture: aggregated device health,
        // error counters, and diagnostic event timeline.

        // ----- State -----
        var healthEvents = [];         // diagEvent objects, newest-first (max 100)
        var healthErrorCounts = {};    // { subsystem: { err: N, warn: N } }
        var healthInitialized = false;

        // Subsystem list used for counter table (stable order)
        var HEALTH_SUBSYSTEMS = ['HAL', 'Audio', 'DSP', 'WiFi', 'MQTT', 'System', 'OTA', 'USB'];

        // ----- WS handler -----
        function handleDiagEvent(data) {
            // Prepend to local buffer, cap at 100
            healthEvents.unshift(data);
            if (healthEvents.length > 100) healthEvents.length = 100;

            // Increment error/warn counter for subsystem
            var sub = data.sub || 'System';
            if (!healthErrorCounts[sub]) healthErrorCounts[sub] = { err: 0, warn: 0 };
            if (data.sev === 'E' || data.sev === 'C') {
                healthErrorCounts[sub].err++;
            } else if (data.sev === 'W') {
                healthErrorCounts[sub].warn++;
            }

            // Re-render if health tab is visible
            if (currentActiveTab === 'health') {
                renderHealthDashboard();
            }
        }

        // ----- Rendering entry point -----
        function renderHealthDashboard() {
            renderHealthDeviceGrid();
            renderHealthErrorCounters();
            renderHealthEventList();
            var ts = document.getElementById('healthLastUpdate');
            if (ts) ts.textContent = 'Updated ' + new Date().toLocaleTimeString();
        }

        // ----- Device Grid -----
        function renderHealthDeviceGrid() {
            var container = document.getElementById('healthDeviceGrid');
            if (!container) return;

            if (!halDevices || halDevices.length === 0) {
                container.innerHTML = '<div class="health-empty">No HAL devices registered</div>';
                return;
            }

            var html = '';
            for (var i = 0; i < halDevices.length; i++) {
                var d = halDevices[i];
                var si = halGetStateInfo(d.state);
                var ti = halGetTypeInfo(d.type);
                var stateClass = 'state-ok';
                if (d.state === 5) stateClass = 'state-error';
                else if (d.state === 4 || d.state === 2) stateClass = 'state-warn';
                else if (d.state !== 3) stateClass = '';

                var retries = (d.retries !== undefined) ? d.retries : 0;
                var faults = (d.faults !== undefined) ? d.faults : 0;
                var lastErr = (d.lastErr !== undefined && d.lastErr !== 0) ? '0x' + d.lastErr.toString(16).toUpperCase().padStart(4, '0') : '';

                html += '<div class="health-device-card ' + stateClass + '">';
                html += '<div class="device-name">' + escapeHtml(d.name || 'Device ' + d.slot) + '</div>';
                html += '<div class="device-meta">' + escapeHtml(ti.label) + ' &middot; Slot ' + d.slot + ' &middot; <span style="color:var(--' + (si.cls === 'green' ? 'success' : si.cls === 'red' ? 'error' : si.cls === 'amber' ? 'warning' : 'text-secondary') + ')">' + escapeHtml(si.label) + '</span></div>';
                html += '<div class="device-stats">';
                html += '<span class="stat-label">Retries</span><span class="' + (retries > 0 ? 'stat-val-err' : 'stat-val-ok') + '">' + retries + '</span>';
                html += '<span class="stat-label">Faults</span><span class="' + (faults > 0 ? 'stat-val-err' : 'stat-val-ok') + '">' + faults + '</span>';
                if (lastErr) {
                    html += '<span class="stat-label">Last Err</span><span class="stat-val-err">' + lastErr + '</span>';
                }
                html += '</div>';
                html += '</div>';
            }
            container.innerHTML = html;
        }

        // ----- Error Counters -----
        function renderHealthErrorCounters() {
            var container = document.getElementById('healthErrorCounters');
            if (!container) return;

            var html = '<table class="health-counter-table">';
            html += '<tr><th style="text-align:left;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Subsystem</th>';
            html += '<th style="text-align:center;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Errors</th>';
            html += '<th style="text-align:center;padding:6px 8px;color:var(--text-secondary);font-weight:500;">Warnings</th></tr>';

            for (var i = 0; i < HEALTH_SUBSYSTEMS.length; i++) {
                var sub = HEALTH_SUBSYSTEMS[i];
                var counts = healthErrorCounts[sub] || { err: 0, warn: 0 };
                html += '<tr>';
                html += '<td class="subsys-label">' + escapeHtml(sub) + '</td>';
                html += '<td style="text-align:center;" class="' + (counts.err > 0 ? 'cnt-err' : 'cnt-none') + '">';
                if (counts.err > 0) {
                    html += '<svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true" style="vertical-align:-1px;margin-right:2px;"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2Z"/></svg>';
                    html += counts.err;
                } else {
                    html += '—';
                }
                html += '</td>';
                html += '<td style="text-align:center;" class="' + (counts.warn > 0 ? 'cnt-warn' : 'cnt-none') + '">';
                if (counts.warn > 0) {
                    html += '<svg viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true" style="vertical-align:-1px;margin-right:2px;"><path d="M1,21H23L12,2L1,21Z"/></svg>';
                    html += counts.warn;
                } else {
                    html += '—';
                }
                html += '</td>';
                html += '</tr>';
            }
            html += '</table>';
            container.innerHTML = html;
        }

        // ----- Event List -----
        function renderHealthEventList() {
            var container = document.getElementById('healthEventList');
            if (!container) return;

            if (healthEvents.length === 0) {
                container.innerHTML = '<div class="health-empty">No diagnostic events recorded</div>';
                return;
            }

            var html = '<table><thead><tr>';
            html += '<th>Time</th><th>Sev</th><th>Code</th><th>Device</th><th>Message</th><th>Corr</th>';
            html += '</tr></thead><tbody>';

            var limit = Math.min(healthEvents.length, 50);
            var now = Date.now();
            for (var i = 0; i < limit; i++) {
                var ev = healthEvents[i];
                var sevClass = 'sev-i';
                var sevIcon = '';
                switch (ev.sev) {
                    case 'E':
                        sevClass = 'sev-e';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M12,2C17.53,2 22,6.47 22,12C22,17.53 17.53,22 12,22C6.47,22 2,17.53 2,12C2,6.47 6.47,2 12,2Z"/></svg>';
                        break;
                    case 'W':
                        sevClass = 'sev-w';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M1,21H23L12,2L1,21Z"/></svg>';
                        break;
                    case 'I':
                        sevClass = 'sev-i';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M13,9H11V7H13M13,17H11V11H13M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg>';
                        break;
                    case 'C':
                        sevClass = 'sev-c';
                        sevIcon = '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true"><path d="M12,2L2,12L12,22L22,12L12,2Z"/></svg>';
                        break;
                }

                var timeStr = healthFormatRelativeTime(ev.ts, now);
                var codeStr = ev.code ? '0x' + ev.code.toString(16).toUpperCase().padStart(4, '0') : '';
                var deviceStr = ev.dev ? escapeHtml(ev.dev) : '';
                var msgStr = ev.msg ? escapeHtml(ev.msg) : '';
                var corrStr = (ev.corr !== undefined && ev.corr > 0) ? '#' + ev.corr : '';

                html += '<tr>';
                html += '<td style="white-space:nowrap;">' + timeStr + '</td>';
                html += '<td class="' + sevClass + '">' + sevIcon + '</td>';
                html += '<td style="font-family:monospace;font-size:12px;">' + codeStr + '</td>';
                html += '<td>' + deviceStr + '</td>';
                html += '<td>' + msgStr + '</td>';
                html += '<td class="corr">' + corrStr + '</td>';
                html += '</tr>';
            }
            html += '</tbody></table>';
            container.innerHTML = html;
        }

        // ----- Relative time formatter -----
        function healthFormatRelativeTime(tsMs, nowMs) {
            if (!tsMs) return '--';
            var diff = Math.max(0, nowMs - tsMs);
            if (diff < 1000) return 'now';
            if (diff < 60000) return Math.floor(diff / 1000) + 's ago';
            if (diff < 3600000) return Math.floor(diff / 60000) + 'm ago';
            if (diff < 86400000) return Math.floor(diff / 3600000) + 'h ago';
            return Math.floor(diff / 86400000) + 'd ago';
        }

        // ----- Action: Snapshot download -----
        function healthSnapshot() {
            apiFetch('/api/diag/snapshot').then(function(r) {
                return r.json();
            }).then(function(data) {
                var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                var a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = 'diag-snapshot-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
                a.click();
                URL.revokeObjectURL(a.href);
                showToast('Snapshot downloaded', 'success');
            }).catch(function(err) {
                showToast('Snapshot failed: ' + err.message, 'error');
            });
        }

        // ----- Action: Download journal -----
        function healthDownloadJournal() {
            apiFetch('/api/diagnostics/journal').then(function(r) {
                return r.json();
            }).then(function(data) {
                var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                var a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = 'diag-journal-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
                a.click();
                URL.revokeObjectURL(a.href);
                showToast('Journal downloaded', 'success');
            }).catch(function(err) {
                showToast('Download failed: ' + err.message, 'error');
            });
        }

        // ----- Action: Clear journal -----
        function healthClearJournal() {
            if (!confirm('Clear diagnostic journal? This cannot be undone.')) return;
            apiFetch('/api/diagnostics/journal', { method: 'DELETE' }).then(function() {
                healthEvents = [];
                healthErrorCounts = {};
                renderHealthDashboard();
                showToast('Journal cleared', 'success');
            }).catch(function(err) {
                showToast('Clear failed: ' + err.message, 'error');
            });
        }

        // ----- Tab init (called from switchTab) -----
        function initHealthDashboard() {
            if (healthInitialized) return;
            healthInitialized = true;
            // Initial render with whatever state we have
            renderHealthDashboard();
        }
