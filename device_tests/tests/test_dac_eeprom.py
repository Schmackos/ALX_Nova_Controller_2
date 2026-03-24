"""DAC EEPROM tests (READ-ONLY).

Tests the EEPROM read, scan, and preset listing endpoints.
NO tests program or erase the EEPROM — those are destructive
hardware operations.
"""

import pytest


# ===========================================================================
# Read / Scan
# ===========================================================================

@pytest.mark.audio
@pytest.mark.hal
class TestDacEeprom:
    """Verify DAC EEPROM read and scan endpoints."""

    def test_eeprom_read_returns_json(self, api):
        """GET /api/hal/eeprom returns valid JSON."""
        resp = api.get("/api/hal/eeprom")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict)

    def test_eeprom_scan(self, api):
        """POST /api/hal/eeprom/scan re-scans I2C + EEPROM."""
        resp = api.post("/api/hal/eeprom/scan")
        assert resp.status_code == 200
        data = resp.json()
        # Should report success and scan count
        assert "success" in data or "scanned" in data

    def test_eeprom_presets_list(self, api):
        """GET /api/hal/eeprom/presets returns preset list from HAL DB."""
        resp = api.get("/api/hal/eeprom/presets")
        assert resp.status_code == 200
        data = resp.json()
        # Should be a list or have presets field
        presets = data if isinstance(data, list) else data.get("presets", [])
        assert isinstance(presets, list)

    def test_eeprom_parsed_fields(self, api):
        """If EEPROM found, parsed data should have device info."""
        data = api.get("/api/hal/eeprom").json()
        found = data.get("found", False)
        if not found:
            pytest.skip("No EEPROM found on this device")
        parsed = data.get("parsed", {})
        assert "deviceName" in parsed or "manufacturer" in parsed or "deviceId" in parsed, (
            f"EEPROM parsed data missing info. Keys: {list(parsed.keys())}"
        )


# ===========================================================================
# Validation
# ===========================================================================

@pytest.mark.audio
@pytest.mark.hal
class TestDacEepromValidation:
    """Verify EEPROM endpoint error handling."""

    def test_eeprom_program_missing_fields(self, api):
        """POST /api/hal/eeprom with empty body should fail gracefully."""
        resp = api.post("/api/hal/eeprom", json={})
        # Should be error, not crash
        assert resp.status_code < 500

    def test_eeprom_no_error_count(self, api):
        """EEPROM read errors should be 0 on healthy device."""
        data = api.get("/api/hal/eeprom").json()
        errors = data.get("readErrors", data.get("errors", 0))
        assert errors == 0, f"EEPROM read errors: {errors}"
