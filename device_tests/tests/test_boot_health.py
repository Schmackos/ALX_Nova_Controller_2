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

    def test_auth_initialized(self, health_parser):
        """Authentication system must be initialized."""
        assert health_parser.milestones.auth_initialized, (
            "Auth system did not initialize — boot may have failed early"
        )

    def test_settings_loaded(self, health_parser):
        """Settings must be loaded from config.json or defaults."""
        assert health_parser.milestones.settings_loaded, (
            "Settings were not loaded during boot"
        )

    def test_hal_discovery_complete(self, health_parser):
        """HAL device discovery must complete."""
        assert health_parser.milestones.hal_discovery_done, (
            "HAL discovery did not complete"
        )

    def test_critical_milestones_met(self, health_parser):
        """All critical boot milestones (auth, settings, HAL) must be met."""
        assert health_parser.milestones.critical_milestones_met, (
            f"Critical milestones not met: {health_parser.summary()['milestones']}"
        )

    def test_heap_not_critical(self, api):
        """Internal heap must not be in critical state after boot."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        # Check heap is not critical (< 40KB free)
        heap_free = data.get("heapFree", 0)
        heap_critical = data.get("heapCritical", False)
        assert not heap_critical, (
            f"Heap is in CRITICAL state: {heap_free} bytes free"
        )

    def test_no_crash_log(self, api):
        """No crash log should be present from a previous crash."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        crash_log = data.get("crashLog", "")
        assert not crash_log, f"Crash log found: {crash_log[:200]}"

    def test_uptime_reasonable(self, api):
        """Uptime should be positive (device is running)."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        data = resp.json()
        uptime = data.get("uptimeSeconds", 0)
        assert uptime > 0, "Uptime is 0 — device may not be running properly"
