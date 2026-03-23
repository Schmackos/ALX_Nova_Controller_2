"""Settings persistence and export/import tests."""

import pytest


@pytest.mark.settings
class TestSettings:
    """Verify settings read, write, and export functionality."""

    def test_get_settings(self, api):
        """Settings endpoint returns valid JSON with expected fields."""
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        data = resp.json()
        # Settings should contain at least some known keys
        assert isinstance(data, dict), "Settings response is not a JSON object"
        assert len(data) > 0, "Settings response is empty"

    def test_export_settings(self, api):
        """Settings export returns a versioned JSON document."""
        resp = api.get("/api/settings/export")
        assert resp.status_code == 200
        data = resp.json()
        # Export v2 should have a version field
        version = data.get("version", data.get("exportVersion", ""))
        assert version, (
            f"Export missing version field. Keys: {list(data.keys())[:10]}"
        )

    def test_roundtrip_dark_mode_toggle(self, api):
        """Toggle darkMode and verify it persists across reads."""
        # Read current value
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        original = resp.json().get("darkMode", False)

        # Toggle it
        new_value = not original
        resp = api.post("/api/settings", json={"darkMode": new_value})
        assert resp.status_code == 200

        # Read back
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        assert resp.json().get("darkMode") == new_value, (
            f"darkMode did not toggle: expected {new_value}"
        )

        # Restore original value
        api.post("/api/settings", json={"darkMode": original})

    @pytest.mark.reboot
    @pytest.mark.slow
    def test_setting_survives_reboot(self, api, reboot_device):
        """A changed setting must persist after a device reboot."""
        # Read current value
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        original = resp.json().get("darkMode", False)

        # Set to a known value
        target = not original
        resp = api.post("/api/settings", json={"darkMode": target})
        assert resp.status_code == 200

        # Reboot and re-authenticate
        reboot_device()

        # Verify the setting persisted
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        assert resp.json().get("darkMode") == target, (
            f"darkMode did not survive reboot: expected {target}"
        )

        # Restore original
        api.post("/api/settings", json={"darkMode": original})

    def test_auth_change_requires_current_password(self, api):
        """Password change endpoint must reject requests without current password."""
        resp = api.post(
            "/api/auth/change",
            json={"newPassword": "shouldfail"},
        )
        # Should fail without currentPassword field
        assert resp.status_code in (400, 401, 403), (
            f"Expected rejection, got {resp.status_code}"
        )
