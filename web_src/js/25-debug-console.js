// ===== Debug Console =====

        function appendDebugLog(timestamp, message, level = 'info') {
            if (debugPaused) {
                debugLogBuffer.push({ timestamp, message, level });
                return;
            }

            // Determine log level from message if not provided
            let detectedLevel = level;
            if (message.includes('[E]') || message.includes('Error') || message.includes('❌')) {
                detectedLevel = 'error';
            } else if (message.includes('[W]') || message.includes('Warning') || message.includes('⚠')) {
                detectedLevel = 'warn';
            } else if (message.includes('[D]')) {
                detectedLevel = 'debug';
            } else if (message.includes('[I]') || message.includes('Info') || message.includes('ℹ')) {
                detectedLevel = 'info';
            }

            const console = document.getElementById('debugConsole');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = detectedLevel; // Store level for filtering

            const ts = formatDebugTimestamp(timestamp);
            let msgClass = 'log-message';
            if (detectedLevel === 'error') msgClass += ' error';
            else if (detectedLevel === 'warn') msgClass += ' warning';
            else if (detectedLevel === 'debug') msgClass += ' debug';
            else if (message.includes('✅') || message.includes('Success')) msgClass += ' success';
            else msgClass += ' info';

            entry.innerHTML = `<span class="log-timestamp">[${ts}]</span><span class="${msgClass}">${message}</span>`;

            // Apply filter visibility (but always add to DOM)
            if (currentLogFilter !== 'all' && detectedLevel !== currentLogFilter) {
                entry.style.display = 'none';
            }

            // Check if user is near the bottom before adding (within 40px)
            const wasAtBottom = (console.scrollHeight - console.scrollTop - console.clientHeight) < 40;

            console.appendChild(entry);

            // Limit entries
            while (console.children.length > DEBUG_MAX_LINES) {
                console.removeChild(console.firstChild);
            }

            // Only auto-scroll if user was already at the bottom
            if (wasAtBottom && entry.style.display !== 'none') {
                console.scrollTop = console.scrollHeight;
            }
        }

        function formatDebugTimestamp(millis) {
            const date = new Date(millis);
            const h = date.getHours().toString().padStart(2, '0');
            const m = date.getMinutes().toString().padStart(2, '0');
            const s = date.getSeconds().toString().padStart(2, '0');
            const ms = date.getMilliseconds().toString().padStart(3, '0');
            return `${h}:${m}:${s}.${ms}`;
        }

        function toggleDebugPause() {
            debugPaused = !debugPaused;
            const btn = document.getElementById('pauseBtn');
            if (debugPaused) {
                btn.textContent = 'Resume';
            } else {
                btn.textContent = 'Pause';
                // Flush buffer
                debugLogBuffer.forEach(log => appendDebugLog(log.timestamp, log.message, log.level));
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
                th.querySelector('.sort-arrow').innerHTML = '&#9650;';
            });
            const th = table.querySelectorAll('th')[col];
            th.classList.add('sorted');
            th.querySelector('.sort-arrow').innerHTML = pinSortAsc ? '&#9650;' : '&#9660;';
        }

        function clearDebugConsole() {
            const console = document.getElementById('debugConsole');
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.dataset.level = 'info';
            entry.innerHTML = '<span class="log-timestamp">[--:--:--.---]</span><span class="log-message info">Console cleared</span>';

            console.innerHTML = '';
            console.appendChild(entry);
            debugLogBuffer = [];
        }

        function setLogFilter(level) {
            currentLogFilter = level;

            // Apply filter to all console entries
            const console = document.getElementById('debugConsole');
            const allEntries = Array.from(console.children);

            allEntries.forEach(entry => {
                const entryLevel = entry.dataset.level;
                if (level === 'all' || entryLevel === level) {
                    entry.style.display = '';
                } else {
                    entry.style.display = 'none';
                }
            });

            // Auto-scroll to bottom after filtering
            console.scrollTop = console.scrollHeight;
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
