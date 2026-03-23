"""OTA / firmware update tests (READ-ONLY).

Tests the firmware update status, version check, and release listing
endpoints.  NO tests trigger an actual firmware download or flash —
that would be destructive.

Many tests require internet access (GitHub HTTPS) and will skip if
the device is in AP-only mode.
"""

import pytest


def _get_wifi_mode(api):
    """Return current WiFi mode (sta/ap/ap+sta)."""
    try:
        data = api.get("/api/wifistatus", timeout=5).json()
        return data.get("mode", "unknown")
    except Exception:
        return "unknown"


def _needs_internet(api):
    """Skip test if device likely has no internet (AP-only mode)."""
    mode = _get_wifi_mode(api)
    if mode == "ap":
        pytest.skip("Device in AP-only mode — no internet for OTA checks")


# ===========================================================================
# Status
# ===========================================================================

@pytest.mark.ota
class TestOtaStatus:
    """Verify OTA status and version endpoints."""

    def test_update_status_returns_json(self, api):
        """GET /api/updatestatus returns valid JSON."""
        resp = api.get("/api/updatestatus")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)

    def test_current_version_matches(self, api, request):
        """OTA currentVersion matches firmware version from /api/settings."""
        fw_ver = getattr(request.config, "_firmware_version", None)
        if not fw_ver or fw_ver == "unknown":
            pytest.skip("Firmware version not cached")

        resp = api.get("/api/updatestatus")
        assert resp.status_code == 200
        data = resp.json()
        ota_ver = data.get("currentVersion", data.get("version", ""))
        if not ota_ver:
            pytest.skip("No currentVersion in OTA status")
        assert ota_ver == fw_ver, (
            f"Version mismatch: OTA={ota_ver}, settings={fw_ver}"
        )

    @pytest.mark.slow
    def test_check_update_endpoint(self, api):
        """GET /api/checkupdate returns success field."""
        _needs_internet(api)
        resp = api.get("/api/checkupdate", timeout=30)
        assert resp.status_code == 200
        data = resp.json()
        assert "success" in data or "updateAvailable" in data

    @pytest.mark.slow
    def test_releases_list(self, api):
        """GET /api/releases returns a release list."""
        _needs_internet(api)
        resp = api.get("/api/releases", timeout=30)
        assert resp.status_code == 200
        data = resp.json()
        # Should be a list or have a releases field
        releases = data if isinstance(data, list) else data.get("releases", [])
        assert isinstance(releases, list)


# ===========================================================================
# Validation
# ===========================================================================

@pytest.mark.ota
class TestOtaValidation:
    """Verify OTA endpoint error handling."""

    def test_release_notes_missing_version(self, api):
        """GET /api/releasenotes without version param should fail."""
        resp = api.get("/api/releasenotes")
        # Missing required param — expect error but not crash
        assert resp.status_code < 500

    def test_release_notes_nonexistent_version(self, api):
        """GET /api/releasenotes?version=0.0.0 should fail gracefully."""
        resp = api.get("/api/releasenotes", params={"version": "0.0.0"})
        assert resp.status_code < 500

    def test_start_update_no_update_available(self, api):
        """POST /api/startupdate with no update available returns failure."""
        resp = api.post("/api/startupdate")
        assert resp.status_code < 500
        data = resp.json()
        # Should not actually start an update
        success = data.get("success", data.get("started", None))
        if success is not None:
            assert success is False or success == 0, (
                "startupdate should fail when no update is available"
            )
