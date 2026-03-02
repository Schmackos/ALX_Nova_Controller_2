        // ===== WebSocket instance and reconnect state =====
        let ws = null;
        let wsReconnectDelay = 2000;
        const WS_MIN_RECONNECT_DELAY = 2000;
        const WS_MAX_RECONNECT_DELAY = 30000;
        let wasDisconnectedDuringUpdate = false;
        let hadPreviousConnection = false;

        // ===== Session & Authentication =====
        function getSessionIdFromCookie() {
            const name = "sessionId=";
            const decodedCookie = decodeURIComponent(document.cookie);
            const ca = decodedCookie.split(';');
            for(let i = 0; i < ca.length; i++) {
                let c = ca[i];
                while (c.charAt(0) == ' ') {
                    c = c.substring(1);
                }
                if (c.indexOf(name) === 0) {
                    return c.substring(name.length, c.length);
                }
            }
            return "";
        }

        // Global fetch wrapper for API calls (handles 401 Unauthorized)
        async function apiFetch(url, options = {}) {
            // Auto-include credentials and session header for all API calls
            const sessionId = getSessionIdFromCookie();
            const defaultOptions = {
                credentials: 'include',
                headers: {
                    'X-Session-ID': sessionId
                }
            };

            // Merge options with defaults, ensuring headers are properly combined
            const mergedOptions = {
                ...defaultOptions,
                ...options,
                headers: {
                    ...defaultOptions.headers,
                    ...(options.headers || {})
                }
            };

            try {
                const response = await fetch(url, mergedOptions);

                if (response.status === 401) {
                    console.warn(`Unauthorized (401) on ${url}. Redirecting...`);
                    // Try to parse JSON to see if there's a redirect provided
                    try {
                        const data = await response.clone().json();
                        if (data.redirect) {
                            window.location.href = data.redirect;
                        } else {
                            window.location.href = '/login';
                        }
                    } catch(e) {
                        window.location.href = '/login';
                    }
                    // Return a never-resolving promise to stop further .then() calls
                    return new Promise(() => {});
                }

                return response;
            } catch (error) {
                console.error(`API Fetch Error [${url}]:`, error);
                throw error;
            }
        }

        // ===== Connection Status =====
        let currentWifiConnected = false;
        let currentWifiSSID = '';
        let currentMqttConnected = null;
        let currentAmpState = false;

        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('wsConnectionStatus');
            if (statusEl) {
                if (connected) {
                    statusEl.textContent = 'Connected';
                    statusEl.className = 'info-value text-success';
                } else {
                    statusEl.textContent = 'Disconnected';
                    statusEl.className = 'info-value text-error';
                }
            }
            // Update status bar
            updateStatusBar(currentWifiConnected, currentMqttConnected, currentAmpState, connected);
        }

        // ===== WebSocket Init =====
        function initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.hostname;
            ws = new WebSocket(`${wsProtocol}//${wsHost}:81`);
            ws.binaryType = 'arraybuffer';

            ws.onopen = function() {
                console.log('WebSocket connected');

                // Send authentication immediately
                const sessionId = getSessionIdFromCookie();
                if (sessionId) {
                    ws.send(JSON.stringify({
                        type: 'auth',
                        sessionId: sessionId
                    }));
                } else {
                    console.error('No session ID for WebSocket auth');
                    window.location.href = '/login';
                }
            };

            ws.onmessage = function(event) {
                if (event.data instanceof ArrayBuffer) { handleBinaryMessage(event.data); return; }
                const data = JSON.parse(event.data);
                routeWsMessage(data);
            };

            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
                updateConnectionStatus(false);
            };

            ws.onclose = function() {
                console.log('WebSocket disconnected, reconnecting...');
                updateConnectionStatus(false);
                setTimeout(initWebSocket, wsReconnectDelay);
                wsReconnectDelay = Math.min(wsReconnectDelay * 2, WS_MAX_RECONNECT_DELAY);
            };
        }

        // ===== WebSocket Send Helper =====
        function wsSend(type, payload) {
            if (!ws || ws.readyState !== WebSocket.OPEN) return false;
            ws.send(JSON.stringify(Object.assign({ type: type }, payload || {})));
            return true;
        }
