        // ===== Window Resize Handler =====
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(function() {
                canvasDims = {};
                invalidateBgCache();
                drawCpuGraph();
                drawMemoryGraph();
                drawPsramGraph();
                dspDrawFreqResponse();
            }, 250);
        });

        // ===== Initialization =====
        window.onload = function() {
            initWebSocket();
            loadMqttSettings();
            initFirmwareDragDrop();
            initSidebar();
            loadSavedNetworks();

            // Add input focus listeners
            document.getElementById('appState.timerDuration').addEventListener('focus', () => inputFocusState.timerDuration = true);
            document.getElementById('appState.timerDuration').addEventListener('blur', () => inputFocusState.timerDuration = false);
            document.getElementById('audioThreshold').addEventListener('focus', () => inputFocusState.audioThreshold = true);
            document.getElementById('audioThreshold').addEventListener('blur', () => inputFocusState.audioThreshold = false);

            // Restore VU meter mode from localStorage
            if (vuSegmentedMode) {
                document.getElementById('vuSegmented').checked = true;
                toggleVuMode(true);
            }

            // Fetch input names
            apiFetch('/api/inputnames')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.names && Array.isArray(d.names)) {
                        for (var i = 0; i < d.names.length && i < NUM_ADCS * 2; i++) inputNames[i] = d.names[i];
                        applyInputNames();
                    }
                })
                .catch(function() {});

            // Initial status bar update
            updateStatusBar(false, null, false, false);

            // Check if settings tab is active and start time updates
            const activePanel = document.querySelector('.panel.active');
            if (activePanel && activePanel.id === 'settings') {
                startTimeUpdates();
            }

            // Check for default password warning
            checkPasswordWarning();
        };
