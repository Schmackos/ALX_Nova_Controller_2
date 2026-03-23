"""Health check endpoint tests — validates GET /api/health.

Tests the firmware health check module end-to-end: the endpoint should
return structured pass/fail verdicts for all 9 check categories.
"""

import pytest


@pytest.mark.health
class TestHealthCheckEndpoint:
    """Verify GET /api/health returns correct structured data."""

    def test_health_endpoint_returns_200(self, api):
        """GET /api/health returns 200 with valid JSON."""
        resp = api.get("/api/health")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)

    def test_health_response_has_required_fields(self, api):
        """Response contains type, verdict, counts, checks array."""
        data = api.get("/api/health").json()
        for key in ["type", "verdict", "passCount", "failCount", "warnCount",
                     "skipCount", "total", "durationMs", "checks"]:
            assert key in data, f"Missing required field: {key}"
        assert data["type"] == "healthCheck"

    def test_health_verdict_not_fail(self, api):
        """Healthy device should not have verdict 'fail'."""
        data = api.get("/api/health").json()
        assert data["verdict"] in ("pass", "warn"), (
            f"Verdict is '{data['verdict']}' — device has failing health checks:\n"
            + "\n".join(
                f"  {c['name']}: {c['status']} — {c['detail']}"
                for c in data.get("checks", [])
                if c.get("status") == "fail"
            )
        )

    def test_health_counts_consistent(self, api):
        """pass + warn + fail + skip should equal total."""
        data = api.get("/api/health").json()
        computed = (data["passCount"] + data["warnCount"]
                    + data["failCount"] + data["skipCount"])
        assert computed == data["total"], (
            f"Count mismatch: {data['passCount']}+{data['warnCount']}"
            f"+{data['failCount']}+{data['skipCount']}={computed} != {data['total']}"
        )

    def test_health_checks_array_matches_total(self, api):
        """checks[] array length should match total count."""
        data = api.get("/api/health").json()
        assert len(data["checks"]) == data["total"], (
            f"Array has {len(data['checks'])} items but total={data['total']}"
        )

    def test_health_check_items_have_required_fields(self, api):
        """Each check item has id, name, status, detail."""
        data = api.get("/api/health").json()
        for check in data["checks"]:
            for key in ["id", "name", "status", "detail"]:
                assert key in check, (
                    f"Check item missing field '{key}': {check}"
                )
            assert check["status"] in ("pass", "warn", "fail", "skip"), (
                f"Check '{check['name']}' has invalid status: '{check['status']}'"
            )

    def test_health_heap_check_present(self, api):
        """heap_free check should exist and pass on a healthy device."""
        data = api.get("/api/health").json()
        heap_checks = [c for c in data["checks"] if "heap" in c["name"]]
        assert len(heap_checks) > 0, "No heap check found in health report"
        assert heap_checks[0]["status"] == "pass", (
            f"Heap check failed: {heap_checks[0]['detail']}"
        )

    def test_health_psram_check_present(self, api):
        """PSRAM check should exist and pass."""
        data = api.get("/api/health").json()
        psram_checks = [c for c in data["checks"] if "psram" in c["name"]]
        assert len(psram_checks) > 0, "No PSRAM check found in health report"
        assert psram_checks[0]["status"] == "pass", (
            f"PSRAM check failed: {psram_checks[0]['detail']}"
        )

    def test_health_hal_summary_present(self, api):
        """HAL summary check should exist."""
        data = api.get("/api/health").json()
        hal_checks = [c for c in data["checks"] if "hal" in c["name"]]
        assert len(hal_checks) > 0, "No HAL check found in health report"

    def test_health_duration_reasonable(self, api):
        """Health check should complete in under 5 seconds."""
        data = api.get("/api/health").json()
        assert data["durationMs"] < 5000, (
            f"Health check took {data['durationMs']}ms — expected <5000ms"
        )

    def test_health_deferred_complete(self, api):
        """Deferred phase should be complete (device booted >30s ago)."""
        data = api.get("/api/health").json()
        assert data["deferred"] is True, (
            "Deferred health checks not yet complete. "
            "Wait at least 30s after boot before running tests."
        )

    def test_health_no_unknown_statuses(self, api):
        """All check statuses should be valid enum values."""
        data = api.get("/api/health").json()
        valid = {"pass", "warn", "fail", "skip"}
        for check in data["checks"]:
            assert check["status"] in valid, (
                f"Check '{check['name']}' has unknown status: '{check['status']}'"
            )
