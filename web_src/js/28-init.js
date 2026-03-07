        // ===== Window Resize Handler =====
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(function() {
                canvasDims = {};
                invalidateBgCache();
                drawCpuGraph();
                drawMemoryGraph();
                drawPsramGraph();
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

            // Initial status bar update
            updateStatusBar(false, null, false, false);

            // Check if settings tab is active and start time updates
            const activePanel = document.querySelector('.panel.active');
            if (activePanel && activePanel.id === 'settings') {
                startTimeUpdates();
            }

            // Check for default password warning
            checkPasswordWarning();

            // Restore debug console filter state from localStorage
            loadDebugFilters();
        };
