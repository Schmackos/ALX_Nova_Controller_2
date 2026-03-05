// ===== Debug Console =====

        function appendDebugLog(timestamp, message, level, module) {
            level = level || 'info';
            if (debugPaused) {
                debugLogBuffer.push({ timestamp: timestamp, message: message, level: level, module: module });
                return;
            }

            // Determine log level from message if not provided
            var detectedLevel = level;
            if (message.includes('[E]') || message.includes('Error') || message.includes('❌')) {
                detectedLevel = 'error';
            } else if (message.includes('[W]') || message.includes('Warning') || message.includes('⚠')) {
                detectedLevel = 'warn';
            } else if (message.includes('[D]')) {
                detectedLevel = 'debug';
            } else if (message.includes('[I]') || message.includes('Info') || message.includes('ℹ')) {
                detectedLevel = 'info';
            }

            // Track module for chip creation
            module = module || extractModule(message) || 'Other';
            if (!knownModules[module]) {
                knownModules[module] = { total: 0, errors: 0, warnings: 0 };
                createModuleChip(module);
            }
            knownModules[module].total++;
            if (detectedLevel === 'error') knownModules[module].errors++;
            if (detectedLevel === 'warn')  knownModules[module].warnings++;
            updateChipBadge(module);

            var consoleEl = document.getElementById('debugConsole');
            var entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = detectedLevel;
            entry.dataset.module = module;

            var ts = formatDebugTimestamp(timestamp);
            var msgClass = 'log-message';
            if (detectedLevel === 'error') msgClass += ' error';
            else if (detectedLevel === 'warn') msgClass += ' warning';
            else if (detectedLevel === 'debug') msgClass += ' debug';
            else if (message.includes('✅') || message.includes('Success')) msgClass += ' success';
            else msgClass += ' info';

            var tsSpan = document.createElement('span');
            tsSpan.className = 'log-timestamp';
            tsSpan.dataset.ms = timestamp;
            tsSpan.textContent = '[' + ts + ']';

            var msgSpan = document.createElement('span');
            msgSpan.className = msgClass;
            msgSpan.textContent = message;

            entry.appendChild(tsSpan);
            entry.appendChild(msgSpan);

            // Apply combined filter visibility
            entry.style.display = isEntryVisible(entry) ? '' : 'none';

            // Apply search highlight if active
            if (debugSearchTerm) { applySearchHighlight(entry); }

            // Check if user is near the bottom before adding (within 40px)
            var wasAtBottom = (consoleEl.scrollHeight - consoleEl.scrollTop - consoleEl.clientHeight) < 40;

            consoleEl.appendChild(entry);

            // Limit entries
            while (consoleEl.children.length > DEBUG_MAX_LINES) {
                consoleEl.removeChild(consoleEl.firstChild);
            }

            // Only auto-scroll if user was already at the bottom
            if (wasAtBottom && entry.style.display !== 'none') {
                consoleEl.scrollTop = consoleEl.scrollHeight;
            }
        }

        function formatDebugTimestamp(ms) {
            if (debugTimestampMode === 'absolute' && ntpOffsetMs > 0) {
                var d = new Date(ntpOffsetMs + ms);
                return d.toLocaleTimeString() + '.' + String(ms % 1000).padStart(3, '0');
            }
            // Relative (uptime) mode
            var s = Math.floor(ms / 1000);
            var frac = ms % 1000;
            var hours = Math.floor(s / 3600); s %= 3600;
            var mins = Math.floor(s / 60); var secs = s % 60;
            return String(hours).padStart(2, '0') + ':' +
                   String(mins).padStart(2, '0') + ':' +
                   String(secs).padStart(2, '0') + '.' +
                   String(frac).padStart(3, '0');
        }

        function toggleDebugPause() {
            debugPaused = !debugPaused;
            const btn = document.getElementById('pauseBtn');
            if (debugPaused) {
                btn.textContent = 'Resume';
            } else {
                btn.textContent = 'Pause';
                // Flush buffer
                debugLogBuffer.forEach(function(log) { appendDebugLog(log.timestamp, log.message, log.level, log.module); });
                debugLogBuffer = [];
            }
        }

        // ===== Pin Table Sorting =====
        let pinSortCol = 0;
        let pinSortAsc = true;
        function sortPinTable(col) {
            const table = document.getElementById('pinTable');
            const tbody = table.querySelector('tbody');
            const rows = Array.from(tbody.querySelectorAll('tr'));
            if (col === pinSortCol) { pinSortAsc = !pinSortAsc; } else { pinSortCol = col; pinSortAsc = true; }
            rows.sort((a, b) => {
                let aVal = a.cells[col].textContent.trim();
                let bVal = b.cells[col].textContent.trim();
                if (col === 0) { return pinSortAsc ? parseInt(aVal) - parseInt(bVal) : parseInt(bVal) - parseInt(aVal); }
                return pinSortAsc ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
            });
            rows.forEach(r => tbody.appendChild(r));
            table.querySelectorAll('th').forEach(th => {
                th.classList.remove('sorted');
                th.querySelector('.sort-arrow').innerHTML = '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg>';
            });
            const th = table.querySelectorAll('th')[col];
            th.classList.add('sorted');
            th.querySelector('.sort-arrow').innerHTML = pinSortAsc
                ? '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,15.41L12,10.83L16.59,15.41L18,14L12,8L6,14L7.41,15.41Z"/></svg>'
                : '<svg viewBox="0 0 24 24" width="10" height="10" fill="currentColor" aria-hidden="true"><path d="M7.41,8.58L12,13.17L16.59,8.58L18,10L12,16L6,10L7.41,8.58Z"/></svg>';
        }

        function clearDebugConsole() {
            knownModules = {};
            var chipsContainer = document.getElementById('moduleChips');
            if (chipsContainer) chipsContainer.innerHTML = '';

            var consoleEl = document.getElementById('debugConsole');
            var entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = 'info';
            entry.dataset.module = 'Other';
            entry.innerHTML = '<span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span>';

            consoleEl.innerHTML = '';
            consoleEl.appendChild(entry);
            debugLogBuffer = [];
        }

        function setLogFilter(level) {
            currentLogFilter = level;
            refilterAll();
            saveDebugFilters();
        }

        function downloadDebugLog() {
            const console = document.getElementById('debugConsole');
            const entries = Array.from(console.children);

            if (entries.length === 0) {
                showToast('No logs to download', 'warning');
                return;
            }

            // Build log content
            let logContent = '=== Debug Log Export ===\n';
            logContent += `Exported: ${new Date().toISOString()}\n`;
            logContent += `Device: ${currentAPSSID || 'Unknown'}\n`;
            logContent += `Firmware: ${currentFirmwareVersion || 'Unknown'}\n`;
            logContent += `Total Entries: ${entries.length}\n`;
            logContent += `Filter: ${currentLogFilter}\n`;
            logContent += '========================\n\n';

            entries.forEach(entry => {
                // Only include visible entries (respects current filter)
                if (entry.style.display !== 'none') {
                    const text = entry.textContent || entry.innerText;
                    logContent += text + '\n';
                }
            });

            // Create blob and download
            const blob = new Blob([logContent], { type: 'text/plain' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.style.display = 'none';
            a.href = url;

            // Generate filename with timestamp
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
            const deviceName = currentAPSSID.replace(/[^a-zA-Z0-9]/g, '_') || 'device';
            a.download = `${deviceName}_debug_log_${timestamp}.txt`;

            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);

            showToast(`Log downloaded (${entries.filter(e => e.style.display !== 'none').length} entries)`, 'success');
        }

        function downloadDiagnostics() {
            showToast('Generating diagnostics...', 'info');
            apiFetch('/api/diagnostics')
                .then(res => {
                    if (!res.ok) throw new Error('Failed to generate diagnostics');
                    return res.blob();
                })
                .then(blob => {
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.style.display = 'none';
                    a.href = url;
                    // Generate filename with timestamp
                    const now = new Date();
                    const timestamp = now.toISOString().replace(/[:.]/g, '-').slice(0, -5);
                    a.download = `diagnostics-${timestamp}.json`;
                    document.body.appendChild(a);
                    a.click();
                    window.URL.revokeObjectURL(url);
                    document.body.removeChild(a);
                    showToast('Diagnostics downloaded', 'success');
                })
                .catch(err => {
                    console.error('Download error:', err);
                    showToast('Failed to download diagnostics', 'error');
                });
        }

        // ===== Module Chip Filtering =====
        // escapeHtml() defined in 14-io-registry.js (loads earlier in concat order)

        function extractModule(msg) {
            var m = msg.match(/\[[DIWE]\]\s*\[([^\]]+)\]/);
            return m ? m[1] : null;
        }

        function createModuleChip(module) {
            var container = document.getElementById('moduleChips');
            if (!container) return;
            var chip = document.createElement('button');
            chip.className = 'btn-chip';
            chip.dataset.module = module;
            chip.innerHTML = escapeHtml(module) + ' <span class="chip-badge">0</span>';
            chip.onclick = function() { toggleModuleFilter(module); };
            if (currentModuleFilters.has(module)) {
                chip.classList.add('active');
            }
            container.appendChild(chip);
        }

        function toggleModuleFilter(module) {
            if (currentModuleFilters.has(module)) {
                currentModuleFilters.delete(module);
            } else {
                currentModuleFilters.add(module);
            }
            var chips = document.querySelectorAll('#moduleChips .btn-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.toggle('active', currentModuleFilters.has(chips[i].dataset.module));
            }
            refilterAll();
            saveDebugFilters();
        }

        function clearModuleFilter() {
            currentModuleFilters.clear();
            var chips = document.querySelectorAll('#moduleChips .btn-chip');
            for (var i = 0; i < chips.length; i++) {
                chips[i].classList.remove('active');
            }
            refilterAll();
            saveDebugFilters();
        }

        function updateChipBadge(module) {
            var chip = document.querySelector('#moduleChips .btn-chip[data-module="' + CSS.escape(module) + '"]');
            if (!chip) return;
            var info = knownModules[module];
            var badge = chip.querySelector('.chip-badge');
            if (!badge) return;
            badge.textContent = info.total;
            badge.className = 'chip-badge';
            if (info.errors > 0) badge.classList.add('has-errors');
            else if (info.warnings > 0) badge.classList.add('has-warnings');
        }

        // ===== Search =====

        function setDebugSearch(term) {
            debugSearchTerm = term.toLowerCase();
            refilterAll();
            saveDebugFilters();
        }

        function clearDebugSearch() {
            debugSearchTerm = '';
            var input = document.getElementById('debugSearchInput');
            if (input) input.value = '';
            refilterAll();
            saveDebugFilters();
        }

        function applySearchHighlight(entry) {
            var msgSpan = entry.querySelector('.log-message');
            if (!msgSpan) return;
            if (!debugSearchTerm) {
                msgSpan.textContent = msgSpan.textContent;
                return;
            }
            var text = msgSpan.textContent;
            var lower = text.toLowerCase();
            var idx = lower.indexOf(debugSearchTerm);
            if (idx >= 0) {
                var before = text.substring(0, idx);
                var match = text.substring(idx, idx + debugSearchTerm.length);
                var after = text.substring(idx + debugSearchTerm.length);
                msgSpan.innerHTML = escapeHtml(before) +
                    '<span class="log-highlight">' + escapeHtml(match) + '</span>' +
                    escapeHtml(after);
            } else {
                msgSpan.textContent = msgSpan.textContent;
            }
        }

        // ===== Combined Visibility & Re-filter =====

        function isEntryVisible(entry) {
            if (currentLogFilter !== 'all' && entry.dataset.level !== currentLogFilter) return false;
            if (currentModuleFilters.size > 0 && !currentModuleFilters.has(entry.dataset.module)) return false;
            if (debugSearchTerm) {
                var msg = entry.querySelector('.log-message');
                if (msg && msg.textContent.toLowerCase().indexOf(debugSearchTerm) === -1) return false;
            }
            return true;
        }

        function refilterAll() {
            var consoleEl = document.getElementById('debugConsole');
            if (!consoleEl) return;
            for (var i = 0; i < consoleEl.children.length; i++) {
                var entry = consoleEl.children[i];
                entry.style.display = isEntryVisible(entry) ? '' : 'none';
                if (debugSearchTerm) applySearchHighlight(entry);
            }
            consoleEl.scrollTop = consoleEl.scrollHeight;
        }

        // ===== Timestamp Toggle =====

        function toggleTimestampMode() {
            debugTimestampMode = (debugTimestampMode === 'relative') ? 'absolute' : 'relative';
            var btn = document.getElementById('timestampToggle');
            if (btn) btn.textContent = (debugTimestampMode === 'relative') ? 'Uptime' : 'Clock';
            var entries = document.querySelectorAll('#debugConsole .log-entry');
            for (var i = 0; i < entries.length; i++) {
                var ts = entries[i].querySelector('.log-timestamp');
                if (ts && ts.dataset.ms) {
                    ts.textContent = '[' + formatDebugTimestamp(parseInt(ts.dataset.ms)) + ']';
                }
            }
            saveDebugFilters();
        }

        // ===== Filter Persistence =====

        function saveDebugFilters() {
            try {
                localStorage.setItem('debugFilters', JSON.stringify({
                    level: currentLogFilter,
                    modules: Array.from(currentModuleFilters),
                    search: debugSearchTerm,
                    timestampMode: debugTimestampMode
                }));
            } catch(e) {}
        }

        function loadDebugFilters() {
            try {
                var saved = JSON.parse(localStorage.getItem('debugFilters'));
                if (!saved) return;
                if (saved.level) {
                    currentLogFilter = saved.level;
                    var sel = document.getElementById('logLevelFilter');
                    if (sel) sel.value = saved.level;
                }
                if (saved.modules && saved.modules.length) {
                    currentModuleFilters = new Set(saved.modules);
                }
                if (saved.search) {
                    debugSearchTerm = saved.search;
                    var input = document.getElementById('debugSearchInput');
                    if (input) input.value = saved.search;
                }
                if (saved.timestampMode) {
                    debugTimestampMode = saved.timestampMode;
                    var btn = document.getElementById('timestampToggle');
                    if (btn) btn.textContent = (debugTimestampMode === 'relative') ? 'Uptime' : 'Clock';
                }
            } catch(e) {}
        }
