        // ===== I/O Registry -- Deprecated (merged into HAL Devices) =====
        // Backend io_registry.cpp still exists for pipeline channel mapping
        // but the web UI now uses HAL Devices exclusively.

        function handleIoRegistryState(d) {
            // No-op: I/O registry UI removed, HAL Devices tab handles everything
        }

        function escapeHtml(str) {
            var div = document.createElement('div');
            div.appendChild(document.createTextNode(str));
            return div.innerHTML;
        }
