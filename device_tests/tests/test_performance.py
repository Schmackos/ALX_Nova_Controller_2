"""Response time performance tests.

Verifies that key REST endpoints respond within acceptable latency
bounds on the ESP32-P4. Uses resp.elapsed from the requests library.
"""

import pytest


@pytest.mark.perf
class TestPerformance:
    """Verify key endpoints respond within time budget."""

    MAX_LATENCY_MS = 500

    def test_health_check_under_500ms(self, api):
        """GET /api/health should respond in <500ms."""
        resp = api.get("/api/health")
        assert resp.status_code == 200
        elapsed_ms = resp.elapsed.total_seconds() * 1000
        assert elapsed_ms < self.MAX_LATENCY_MS, (
            f"/api/health took {elapsed_ms:.0f}ms (limit: {self.MAX_LATENCY_MS}ms)"
        )

    def test_diagnostics_under_500ms(self, api):
        """GET /api/diagnostics should respond in <500ms."""
        resp = api.get("/api/diagnostics")
        assert resp.status_code == 200
        elapsed_ms = resp.elapsed.total_seconds() * 1000
        assert elapsed_ms < self.MAX_LATENCY_MS, (
            f"/api/diagnostics took {elapsed_ms:.0f}ms (limit: {self.MAX_LATENCY_MS}ms)"
        )

    def test_hal_devices_under_500ms(self, api):
        """GET /api/hal/devices should respond in <500ms."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        elapsed_ms = resp.elapsed.total_seconds() * 1000
        assert elapsed_ms < self.MAX_LATENCY_MS, (
            f"/api/hal/devices took {elapsed_ms:.0f}ms (limit: {self.MAX_LATENCY_MS}ms)"
        )
