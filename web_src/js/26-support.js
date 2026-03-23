// ===== Support / User Manual Section =====
        let manualQrGenerated = false;
        let manualContentLoaded = false;
        let manualRawMarkdown = '';
        const GITHUB_REPO_OWNER = 'Schmackos';
        const GITHUB_REPO_NAME = 'ALX_Nova_Controller_2';
        const MANUAL_URL = `https://github.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}/blob/main/USER_MANUAL.md`;
        const MANUAL_RAW_URL = `https://raw.githubusercontent.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}/main/USER_MANUAL.md`;

        function generateManualQRCode() {
            if (manualQrGenerated) return;

            const qrContainer = document.getElementById('manualQrCode');
            const manualLink = document.getElementById('manualLink');

            manualLink.href = MANUAL_URL;
            manualLink.textContent = MANUAL_URL;

            if (typeof QRCode !== 'undefined') {
                renderQR();
                return;
            }

            const script = document.createElement('script');
            script.src = 'https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js';
            script.onload = () => renderQR();
            script.onerror = () => {
                qrContainer.innerHTML = '<div style="color: var(--text-secondary); padding: 10px; font-size: 13px;">QR code library unavailable (offline)</div>';
            };
            document.head.appendChild(script);

            function renderQR() {
                try {
                    new QRCode(qrContainer, {
                        text: MANUAL_URL,
                        width: 180,
                        height: 180,
                        colorDark: '#000000',
                        colorLight: '#ffffff',
                        correctLevel: QRCode.CorrectLevel.M
                    });
                    manualQrGenerated = true;
                } catch (e) {
                    console.error('QR Generator Error:', e);
                }
            }
        }

        function loadManualContent() {
            if (manualContentLoaded) return;
            manualContentLoaded = true;

            const container = document.getElementById('manualRendered');
            container.innerHTML = '<div class="manual-loading">Loading manual...</div>';

            fetch(MANUAL_RAW_URL)
                .then(r => { if (!r.ok) throw new Error(r.status); return r.text(); })
                .then(md => {
                    manualRawMarkdown = md;
                    loadMarkedAndRender(md);
                })
                .catch(() => {
                    container.innerHTML = '<div class="manual-loading">Manual unavailable offline. Use the QR code above.</div>';
                    manualContentLoaded = false;
                });
        }

        function loadMarkedAndRender(md) {
            const container = document.getElementById('manualRendered');

            if (typeof marked !== 'undefined') {
                container.innerHTML = sanitizeHtml(marked.parse(md));
                return;
            }

            const script = document.createElement('script');
            script.src = 'https://cdn.jsdelivr.net/npm/marked/marked.min.js';
            script.onload = () => {
                container.innerHTML = sanitizeHtml(marked.parse(md));
            };
            script.onerror = () => {
                container.innerHTML = '<pre style="white-space: pre-wrap;">' + md.replace(/</g, '&lt;') + '</pre>';
            };
            document.head.appendChild(script);
        }

        function searchManual(query) {
            const container = document.getElementById('manualRendered');
            const status = document.getElementById('manualSearchStatus');

            if (!manualRawMarkdown || typeof marked === 'undefined') {
                status.textContent = '';
                return;
            }

            container.innerHTML = sanitizeHtml(marked.parse(manualRawMarkdown));

            if (!query || query.length < 2) {
                status.textContent = '';
                return;
            }

            let count = 0;
            const regex = new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');

            function highlightNode(node) {
                if (node.nodeType === 3) {
                    const text = node.textContent;
                    if (regex.test(text)) {
                        regex.lastIndex = 0;
                        const span = document.createElement('span');
                        span.innerHTML = text.replace(regex, m => { count++; return '<span class="search-highlight">' + m + '</span>'; });
                        node.parentNode.replaceChild(span, node);
                    }
                } else if (node.nodeType === 1 && node.childNodes.length && !/(script|style)/i.test(node.tagName)) {
                    Array.from(node.childNodes).forEach(highlightNode);
                }
            }

            highlightNode(container);
            status.textContent = count > 0 ? count + ' match' + (count !== 1 ? 'es' : '') + ' found' : 'No matches';

            if (count > 0) {
                const first = container.querySelector('.search-highlight');
                if (first) first.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
        }

        // ===== Release Notes =====
        function showReleaseNotes() {
            showReleaseNotesFor('latest');
        }

        function showReleaseNotesFor(which) {
            let version, label;
            if (which === 'current') {
                version = currentFirmwareVersion;
                label = 'Current';
            } else if (which === 'latest') {
                version = currentLatestVersion;
                label = 'Latest';
            } else {
                version = which;
                label = null;
            }

            if (!version) {
                showToast('Version information not available', 'error');
                return;
            }

            // If already open with the same version, toggle it closed
            const container = document.getElementById('inlineReleaseNotes');
            const currentShownVersion = container.dataset.version;

            if (container.classList.contains('open') && currentShownVersion === version) {
                toggleInlineReleaseNotes(false);
                return;
            }

            // Fetch and show
            apiFetch(`/api/releasenotes?version=${version}`)
            .then(res => res.json())
            .then(data => {
                const titleText = label ? `Release Notes v${version} (${label})` : `Release Notes v${version}`;
                document.getElementById('inlineReleaseNotesTitle').textContent = titleText;
                document.getElementById('inlineReleaseNotesContent').textContent = data.notes || 'No release notes available for this version.';

                container.dataset.version = version;
                toggleInlineReleaseNotes(true);
            })
            .catch(err => showToast('Failed to load release notes', 'error'));
        }

        function toggleInlineReleaseNotes(show) {
            const container = document.getElementById('inlineReleaseNotes');
            if (show) {
                container.classList.add('open');
            } else {
                container.classList.remove('open');
            }
        }
