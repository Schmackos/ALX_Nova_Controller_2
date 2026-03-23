"""Boot health checks: verify the device booted cleanly."""

import pytest


class TestBootHealth:
    """Verify the device completed a healthy boot sequence."""

    def test_no_serial_errors(self, health_parser):
        """No ERROR-level log lines should appear during normal boot."""
        errors = health_parser.get_errors()
        # Filter out known benign errors (e.g., MQTT not configured)
        critical_errors = [
            e for e in errors
            if "MQTT" not in e.module  # MQTT errors OK if not configured
        ]
        assert len(critical_errors) == 0, (
            f"Found {len(critical_errors)} error(s) during boot:\n"
            + "\n".join(e.raw for e in critical_errors[:10])
        )

    def test_auth_initialized(self, api):
        """Authentication system must be initialized.

        Boot milestones are not available via serial (device is already
        booted when tests start). Verify auth is working by checking that
        our authenticated session can reach a protected endpoint.
        """
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200, (
            f"Auth not working — API returned {resp.status_code}. "
            "Auth system may not have initialized."
        )

    def test_settings_loaded(self, api):
        """Settings must be loaded from config.json or defaults.

        Verified indirectly: if settings failed to load, the diagnostics
        endpoint would not be reachable or would show error state.
        """
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # If we get a valid diagnostics response with device info,
        # settings were loaded successfully
        assert "device" in data or "system" in data, (
            "Diagnostics response missing expected sections — "
            "settings may not have loaded"
        )

    def test_hal_discovery_complete(self, api):
        """HAL device discovery must complete.

        Verified via REST: if HAL discovery completed, devices are present.
        """
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()
        assert isinstance(devices, list), "HAL devices response is not a list"
        assert len(devices) > 0, (
            "No HAL devices found — discovery may not have completed"
        )

    def test_critical_milestones_met(self, api):
        """All critical boot milestones (auth, settings, HAL) must be met.

        Verified via REST: auth works (we are authenticated), settings loaded
        (diagnostics endpoint responds), HAL discovery ran (devices present).
        """
        # Auth — already proven by fixture, but double-check
        diag_resp = api.get("/api/diagnostics")
        assert diag_resp.status_code == 200, "Diagnostics endpoint unreachable"

        # HAL discovery
        hal_resp = api.get("/api/hal/devices")
        assert hal_resp.status_code == 200, "HAL devices endpoint unreachable"
        devices = hal_resp.json()
        assert len(devices) > 0, "No HAL devices — discovery may have failed"

    def test_heap_not_critical(self, api):
        """Internal heap must not be in critical state after boot."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # Diagnostics nests system info under "system" key
        system = data.get("system", data)
        heap_free = system.get("freeHeap", 0)
        heap_critical = system.get("heapCritical", False)
        assert not heap_critical, (
            f"Heap is in CRITICAL state: {heap_free} bytes free"
        )

    def test_no_crash_log(self, api):
        """No crash log should be present from a previous crash."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        system = data.get("system", data)
        # Check wasCrash flag (firmware uses wasCrash, not crashLog string)
        was_crash = system.get("wasCrash", False)
        crash_history = system.get("crashHistory", [])
        recent_crashes = [c for c in crash_history if c.get("wasCrash", False)]
        assert not was_crash, (
            f"Last reset was a crash. History: {recent_crashes[:3]}"
        )

    def test_uptime_reasonable(self, api):
        """Uptime should be positive (device is running)."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # Uptime is nested under "system" in the diagnostics response
        system = data.get("system", data)
        uptime = system.get("uptimeSeconds", 0)
        assert uptime > 0, (
            f"Uptime is 0 — device may not be running properly. "
            f"System keys: {list(system.keys()) if isinstance(system, dict) else 'N/A'}"
        )
