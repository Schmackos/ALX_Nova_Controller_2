        // ===== State Variables =====
        let autoUpdateEnabled = false;
        let darkMode = true;
        let waveformAutoScaleEnabled = false;
        let backlightOn = true;
        let backlightBrightness = 255;
        let screenTimeoutSec = 60;
        let dimEnabled = false;
        let dimTimeoutSec = 10;
        let dimBrightnessPwm = 26;
        let enableCertValidation = true;
        let currentFirmwareVersion = '';
        let currentLatestVersion = '';
        let currentTimezoneOffset = 3600;
        let currentAPSSID = '';
        let manualUploadInProgress = false;
        let debugPaused = false;
        let debugLogBuffer = [];
        const DEBUG_MAX_LINES = 1000;
        let currentLogFilter = 'all'; // all, debug, info, warn, error
        let audioSubscribed = false;
        let currentActiveTab = 'control';

        let vuSegmentedMode = localStorage.getItem('vuSegmented') === 'true';

        // LED bar mode
        let ledBarMode = localStorage.getItem('ledBarMode') === 'true';

        // Input focus state to prevent overwrites during user input
        let inputFocusState = {
            timerDuration: false,
            audioThreshold: false
        };

        // Performance History Data
        let historyData = {
            timestamps: [],
            cpuTotal: [],
            cpuCore0: [],
            cpuCore1: [],
            memoryPercent: [],
            psramPercent: []
        };
        let maxHistoryPoints = 300;

        // Settings tab state
        let currentDstOffset = 0;
        let timeUpdateInterval = null;

        // Smart sensing
        let smartAutoSettingsCollapsed = true;

        // WiFi
        let wifiConnectionPollTimer = null;

        // Window resize handler
        let resizeTimeout;

        // ===== Utility Functions =====
        function showToast(message, type = 'info') {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = 'toast show ' + type;

            setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }

        function formatFreq(f) {
            return f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 0 : 1) + 'k' : f.toFixed(0);
        }

        function formatRssi(rssi) {
            if (rssi === undefined || rssi === null) return 'N/A';
            rssi = parseInt(rssi);
            let text, cls;
            if (rssi >= -50) { text = 'Excellent (90-100%)'; cls = 'text-success'; }
            else if (rssi >= -60) { text = 'Very Good (70-90%)'; cls = 'text-success'; }
            else if (rssi >= -70) { text = 'Fair (50-70%)'; cls = 'text-warning'; }
            else if (rssi >= -80) { text = 'Weak (30-50%)'; cls = 'text-error'; }
            else { text = 'Very Weak (0-30%)'; cls = 'text-error'; }
            return '<span class="' + cls + '">' + rssi + ' dBm - ' + text + '</span>';
        }
