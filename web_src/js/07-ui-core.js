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

            // Load support content when switching to support tab
            if (tabId === 'support') {
                generateManualQRCode();
                loadManualContent();
            }

            // DSP tab: redraw frequency response, load routing, subscribe audio for RTA
            if (tabId === 'dsp') {
                canvasDims = {};
                setTimeout(dspDrawFreqResponse, 50);
                dspLoadRouting();
                if (typeof updatePeqCopyToDropdown === 'function') updatePeqCopyToDropdown();
                if (typeof updateChainCopyToDropdown === 'function') updateChainCopyToDropdown();
                if (peqGraphLayers.rta && ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: true }));
                    ws.send(JSON.stringify({ type: 'setSpectrumEnabled', enabled: true }));
                }
            } else if (currentActiveTab === 'dsp' && peqGraphLayers.rta && tabId !== 'audio') {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
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
                for (let a = 0; a < NUM_ADCS; a++) {
                    drawAudioWaveform(null, a);
                    drawSpectrumBars(null, 0, a);
                }
                // Update ADC2 panel visibility
                updateAdc2Visibility();
                // Load input names into fields
                loadInputNameFields();
            } else if (tabId !== 'audio' && audioSubscribed) {
                audioSubscribed = false;
                var dspRtaTakeover = (tabId === 'dsp' && peqGraphLayers.rta);
                if (!dspRtaTakeover && ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'subscribeAudio', enabled: false }));
                }
                // Stop animation and reset state
                if (audioAnimFrameId) { cancelAnimationFrame(audioAnimFrameId); audioAnimFrameId = null; }
                if (vuAnimFrameId) { cancelAnimationFrame(vuAnimFrameId); vuAnimFrameId = null; }
                for (let a = 0; a < NUM_ADCS; a++) {
                    waveformCurrent[a] = null; waveformTarget[a] = null;
                    if (!dspRtaTakeover) { spectrumTarget[a].fill(0); }
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
