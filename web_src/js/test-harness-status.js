        // ===== Test Harness Status Card =====
        // Standalone module — not wired into the main web server or build pipeline.
        // Exports renderTestHarnessStatus() as a global function that renders a
        // status card showing test harness module info into a target element.

        /**
         * Build an HTML string for a single info row inside the status card.
         * Uses only static template strings — no dynamic user content injected via innerHTML.
         * @param {string} label - The row label text.
         * @param {string} value - The row value text.
         * @returns {string} HTML markup for the row.
         */
        function _tharBuildRow(label, value) {
            var safeLabel = String(label).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            var safeValue = String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            return '<div class="thar-row">'
                + '<span class="thar-label">' + safeLabel + '</span>'
                + '<span class="thar-value">' + safeValue + '</span>'
                + '</div>';
        }

        /**
         * Build the full HTML for the test harness status card.
         * @param {object} info - Module info object with optional fields.
         * @param {string} [info.name] - Module name.
         * @param {string} [info.version] - Module version string.
         * @param {string} [info.status] - Current status label.
         * @param {number} [info.testCount] - Number of tests in the harness.
         * @param {string} [info.lastRun] - ISO timestamp of the last run.
         * @returns {string} HTML markup for the card.
         */
        function _tharBuildCardHtml(info) {
            var name = info.name || 'Test Harness';
            var version = info.version || '—';
            var status = info.status || 'idle';
            var testCount = (info.testCount !== undefined && info.testCount !== null)
                ? String(info.testCount)
                : '—';
            var lastRun = info.lastRun ? new Date(info.lastRun).toLocaleString() : '—';

            var statusClass = 'thar-status-idle';
            if (status === 'running') {
                statusClass = 'thar-status-running';
            } else if (status === 'pass') {
                statusClass = 'thar-status-pass';
            } else if (status === 'fail') {
                statusClass = 'thar-status-fail';
            }

            var html = '<div class="thar-card">';
            html += '<div class="thar-header">';
            html += '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">'
                + '<path d="M14.5,2.5C15.33,2.5 16,3.17 16,4C16,4.83 15.33,5.5 14.5,5.5C13.67,5.5 13,4.83 13,4C13,3.17 13.67,2.5 14.5,2.5M10.5,6.5A2,2 0 0,0 12.5,8.5H13V15H11.5V10H10.5V8.5L10.5,6.5M6,12A6,6 0 0,0 12,18A6,6 0 0,0 18,12H16A4,4 0 0,1 12,16A4,4 0 0,1 8,12H6Z"/>'
                + '</svg>';

            var safeName = String(name).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            html += '<span class="thar-title">' + safeName + '</span>';
            html += '<span class="thar-status-badge ' + statusClass + '">' + status + '</span>';
            html += '</div>';

            html += '<div class="thar-body">';
            html += _tharBuildRow('Version', version);
            html += _tharBuildRow('Tests', testCount);
            html += _tharBuildRow('Last Run', lastRun);
            html += '</div>';
            html += '</div>';

            return html;
        }

        /**
         * Render a test harness status card into the given container element.
         * If no container is provided, targets an element with id="tharStatusContainer".
         *
         * @param {object} [info] - Module info. Defaults to empty object for placeholder card.
         * @param {string} [info.name] - Human-readable module name.
         * @param {string} [info.version] - Semantic version string (e.g. "1.2.3").
         * @param {string} [info.status] - One of: "idle", "running", "pass", "fail".
         * @param {number} [info.testCount] - Total number of tests in the harness.
         * @param {string} [info.lastRun] - ISO 8601 timestamp of the most recent run.
         * @param {Element|null} [container] - DOM element to render into. Falls back to
         *   document.getElementById("tharStatusContainer") when omitted.
         * @returns {void}
         */
        function renderTestHarnessStatus(info, container) {
            var target = container || document.getElementById('tharStatusContainer');
            if (!target) {
                console.warn('renderTestHarnessStatus: no target container found');
                return;
            }
            var moduleInfo = info || {};
            // innerHTML is intentional here — all values are escaped via _tharBuildRow
            // and _tharBuildCardHtml before insertion (static template pattern).
            target.innerHTML = _tharBuildCardHtml(moduleInfo); // eslint-disable-line no-restricted-syntax
        }
