        // ===== Tab Switching =====
        function switchTab(tabId) {
            // Update mobile tab buttons
            document.querySelectorAll('.tab').forEach(tab => {
                tab.classList.toggle('active', tab.dataset.tab === tabId);
            });
            // Update sidebar items
            document.querySelectorAll('.sidebar-item').forEach(item => {
                item.classList.toggle('active', item.dataset.tab === tabId);
            });
            // Update panels
            document.querySelectorAll('.panel').forEach(panel => {
                panel.classList.toggle('active', panel.id === tabId);
            });

            // Start/stop time updates when switching to/from settings tab
            if (tabId === 'settings') {
                startTimeUpdates();
            } else {
                stopTimeUpdates();
            }

            // Load HAL devices and I2S ports when switching to devices tab
            if (tabId === 'devices') {
                loadHalDeviceList();
                loadI2sPorts();
            }

            // Load support content when switching to support tab
            if (tabId === 'support') {
                generateManualQRCode();
                loadManualContent();
            }

            // Health tab — lazy init + re-render
            if (tabId === 'health') {
                initHealthDashboard();
                renderHealthDashboard();
            }

            // Audio tab — render active sub-view on tab switch
            if (tabId === 'audio') {
                renderAudioSubView();
            }

            // Audio tab subscription management
            if (tabId === 'audio' && !audioSubscribed) {
                audioSubscribed = true;
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                }
                // Sync LED toggle
                const ledToggle = document.getElementById('ledModeToggle');
                if (ledToggle) ledToggle.checked = ledBarMode;
                // Cache VU DOM refs + invalidate canvas caches for fresh tab
                cacheVuDomRefs();
                canvasDims = {};
                invalidateBgCache();
                // Draw initial empty canvases for each ADC
                for (let a = 0; a < numInputLanes; a++) {
                    drawAudioWaveform(null, a);
                    drawSpectrumBars(null, 0, a);
                }
            } else if (tabId !== 'audio' && audioSubscribed) {
                audioSubscribed = false;
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
                // Stop animation and reset state
                if (audioAnimFrameId) { cancelAnimationFrame(audioAnimFrameId); audioAnimFrameId = null; }
                if (vuAnimFrameId) { cancelAnimationFrame(vuAnimFrameId); vuAnimFrameId = null; }
                for (let a = 0; a < numInputLanes; a++) {
                    waveformCurrent[a] = null; waveformTarget[a] = null;
                    spectrumTarget[a].fill(0);
                    spectrumCurrent[a].fill(0);
                    spectrumPeaks[a].fill(0); spectrumPeakTimes[a].fill(0);
                }
                vuDomRefs = null;
            }

            currentActiveTab = tabId;
        }

        // ===== Sidebar Toggle =====
        function toggleSidebar() {
            const sidebar = document.getElementById('sidebar');
            const body = document.body;
            sidebar.classList.toggle('collapsed');
            body.classList.toggle('sidebar-collapsed');
            // Save preference
            localStorage.setItem('sidebarCollapsed', sidebar.classList.contains('collapsed'));
        }

        function initSidebar() {
            const collapsed = localStorage.getItem('sidebarCollapsed') === 'true';
            if (collapsed) {
                document.getElementById('sidebar').classList.add('collapsed');
                document.body.classList.add('sidebar-collapsed');
            }
        }
