        // ===== WebSocket instance and reconnect state =====
        let ws = null;
        let wsReconnectDelay = 2000;
        const WS_MIN_RECONNECT_DELAY = 2000;
        const WS_MAX_RECONNECT_DELAY = 30000;
        let wasDisconnectedDuringUpdate = false;
        let hadPreviousConnection = false;

        // ===== Session & Authentication =====
        // Cookie is HttpOnly — browser sends it automatically with credentials: 'include'

        // Global fetch wrapper for API calls (handles 401 Unauthorized + 10s timeout)
        // Automatically prefixes /api/ paths with /api/v1/ for versioned endpoints.
        async function apiFetch(url, options = {}) {
            // Rewrite /api/ to /api/v1/ for all API calls (backward compat preserved on server)
            if (url.startsWith('/api/') && !url.startsWith('/api/v1/') && !url.startsWith('/api/__test__/')) {
                url = '/api/v1/' + url.slice(5);
            }
            const controller = new AbortController();
            const timeoutMs = options.timeout || 10000;
            const timeoutId = setTimeout(function() { controller.abort(); }, timeoutMs);

            // Merge options with defaults, ensuring headers are properly combined
            const mergedOptions = {
                credentials: 'include',
                ...options,
                signal: controller.signal,
                headers: {
                    ...(options.headers || {})
                }
            };

            try {
                const response = await fetch(url, mergedOptions);
                clearTimeout(timeoutId);

                if (response.status === 401) {
                    console.warn(`Unauthorized (401) on ${url}. Redirecting...`);
                    // Try to parse JSON to see if there's a redirect provided
                    try {
                        const data = await response.clone().json();
                        if (data.redirect && data.redirect.startsWith('/') && !data.redirect.startsWith('//')) {
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

                response.safeJson = async function() {
                    if (!this.ok) {
                        let errorMsg = 'Request failed: ' + this.status;
                        try {
                            const errBody = await this.clone().json();
                            if (errBody.error) errorMsg = errBody.error;
                        } catch(e) { /* non-JSON error body, use status text */ }
                        throw new Error(errorMsg);
                    }
                    try {
                        return await this.json();
                    } catch(e) {
                        throw new Error('Invalid response format');
                    }
                };
                return response;
            } catch (error) {
                clearTimeout(timeoutId);
                if (error.name === 'AbortError') {
                    console.warn('Request timeout:', url);
                    throw new Error('Request timed out');
                }
                console.error(`API Fetch Error [${url}]:`, error);
                throw error;
            }
        }

        // ===== HTML Escaping =====
        function escapeHtml(str) {
            if (!str) return '';
            return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }

        // Sanitize HTML from markdown rendering — strip dangerous elements and attributes
        function sanitizeHtml(html) {
            // Remove script tags and their content
            html = html.replace(/<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi, '');
            // Remove iframe, object, embed, form tags (with content)
            html = html.replace(/<(iframe|object|embed|form)\b[^>]*>[\s\S]*?<\/\1>/gi, '');
            html = html.replace(/<(iframe|object|embed|form)\b[^>]*\/?\s*>/gi, '');
            // Remove event handlers (onclick, onerror, onload, etc.)
            html = html.replace(/\s+on\w+\s*=\s*(['"])[^'"]*\1/gi, '');
            html = html.replace(/\s+on\w+\s*=\s*[^\s>]+/gi, '');
            // Remove javascript: URLs
            html = html.replace(/href\s*=\s*(['"])javascript:[^'"]*\1/gi, 'href=$1#$1');
            html = html.replace(/src\s*=\s*(['"])javascript:[^'"]*\1/gi, 'src=$1#$1');
            return html;
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

            ws.onopen = async function() {
                console.log('WebSocket connected');

                // Fetch one-time WS token from server (cookie sent automatically)
                try {
                    const resp = await apiFetch('/api/ws-token');
                    const data = await resp.json();
                    if (data.success && data.token) {
                        ws.send(JSON.stringify({ type: 'auth', token: data.token }));
                    } else {
                        console.error('Failed to get WS token:', data.error);
                        window.location.href = '/login';
                    }
                } catch (e) {
                    console.error('WS token fetch failed:', e);
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
