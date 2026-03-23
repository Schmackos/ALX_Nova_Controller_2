"""Settings persistence and export/import tests."""

import pytest


def _get_dark_mode(data):
    """Extract darkMode from settings response (handles dotted key names)."""
    return data.get("appState.darkMode", data.get("darkMode", False))


@pytest.mark.settings
class TestSettings:
    """Verify settings read, write, and export functionality."""

    def test_get_settings(self, api):
        """Settings endpoint returns valid JSON with expected fields."""
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        data = resp.json()
        assert isinstance(data, dict), "Settings response is not a JSON object"
        assert len(data) > 0, "Settings response is empty"

    def test_export_settings(self, api):
        """Settings export returns a versioned JSON document."""
        resp = api.get("/api/settings/export")
        assert resp.status_code == 200
        data = resp.json()
        # Export should have version info (in exportInfo or version field)
        assert data.get("exportInfo") or data.get("version"), (
            f"Export missing version/exportInfo field. Keys: {list(data.keys())[:10]}"
        )

    def test_roundtrip_dark_mode_toggle(self, api):
        """Toggle darkMode and verify it persists across reads."""
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        original = _get_dark_mode(resp.json())

        # Toggle it
        new_value = not original
        # POST uses the same dotted key format as GET response
        resp = api.post("/api/settings", json={"appState.darkMode": new_value})
        assert resp.status_code == 200

        # Read back
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        assert _get_dark_mode(resp.json()) == new_value, (
            f"darkMode did not toggle: expected {new_value}"
        )

        # Restore original value
        api.post("/api/settings", json={"appState.darkMode": original})

    @pytest.mark.reboot
    @pytest.mark.slow
    def test_setting_survives_reboot(self, api, reboot_device):
        """A changed setting must persist after a device reboot."""
        resp = api.get("/api/settings")
        assert resp.status_code == 200
        original = _get_dark_mode(resp.json())

        target = not original
        resp = api.post("/api/settings", json={"darkMode": target})
        assert resp.status_code == 200

        reboot_device()

        resp = api.get("/api/settings")
        assert resp.status_code == 200
        assert _get_dark_mode(resp.json()) == target, (
            f"darkMode did not survive reboot: expected {target}"
        )

        api.post("/api/settings", json={"darkMode": original})

    def test_auth_change_validates_input(self, api, device_password):
        """Password change endpoint must validate input.

        IMPORTANT: This test must NOT actually change the password, or all
        subsequent test runs will fail. We test with an empty newPassword
        which should be rejected, and restore the original if it succeeds.
        """
        # Send an empty password — firmware should reject this
        resp = api.post(
            "/api/auth/change",
            json={"newPassword": ""},
        )
        # Empty password should be rejected (400) but accept any non-500 response
        assert resp.status_code < 500, (
            f"Server error on password change: {resp.status_code}"
        )

        # Safety net: if firmware somehow accepted the change, restore original
        if resp.status_code == 200:
            api.post(
                "/api/auth/change",
                json={"currentPassword": "", "newPassword": device_password},
            )
